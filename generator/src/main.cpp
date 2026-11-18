// Copyright (c) Choreo contributors

#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <choreo/document/project.hpp>
#include <choreo/document/trajectory.hpp>
#include <generator.hpp>
#include <progress_update_sender/client.hpp>
#include <sleipnir/optimization/solver/exit_status.hpp>
#include <trajopt/swerve_trajectory_generator.hpp>
#include <wpi/math/trajectory/DifferentialTrajectory.hpp>
#include <wpi/math/trajectory/HolonomicTrajectory.hpp>
#include <wpi/util/json.hpp>

#include "cli_parser.hpp"
#include "document_adapter.hpp"
#include "segment.hpp"
#include "split_to_segments.hpp"

///
/// @brief Validates the input trajectory and project files for trajectory
/// generation.
/// @param project The project file.
/// @param trajectory The trajectory file.
/// @throws std::runtime_error if the input trajectory is invalid.
void validate_generation_input(const choreo::document::Project& project,
                               const choreo::document::Trajectory& trajectory) {
  const auto& params = trajectory.params;
  if (params.waypoints.size() < 2) {
    throw std::runtime_error(
        "Input trajectory has " + std::to_string(params.waypoints.size()) +
        " waypoint(s); at least 2 are required for generation");
  }

  size_t invalid_enabled_constraints = 0;
  for (const auto& constraint : params.constraints) {
    if (!constraint.enabled) {
      continue;
    }
    const auto from = choreo::ResolveWaypointId(constraint.from, params.waypoints);
    const auto to = constraint.to
              ? choreo::ResolveWaypointId(*constraint.to,
                             params.waypoints)
              : std::nullopt;
    if (!from || (constraint.to && !to)) {
      invalid_enabled_constraints++;
    }
  }
  if (invalid_enabled_constraints > 0) {
    throw std::runtime_error(
        "Input trajectory contains " +
        std::to_string(invalid_enabled_constraints) +
        " enabled constraint(s) with invalid waypoint references");
  }

    if (trajectory.output.has_value()) {
    const bool sample_type_matches_project =
      (project.type == choreo::document::DriveType::Swerve &&
       std::holds_alternative<
         std::optional<wpi::math::HolonomicTrajectory>>(
         trajectory.output->trajectory)) ||
      (project.type == choreo::document::DriveType::Differential &&
       std::holds_alternative<
         std::optional<wpi::math::DifferentialTrajectory>>(
         trajectory.output->trajectory));

    if (!sample_type_matches_project) {
      throw std::runtime_error(
          "Input trajectory sample_type does not match project drive type");
    }
  }
}

/// @brief Generates a trajectory file using the specified trajectory generator.
/// @tparam Generator The trajectory generator type.
/// @param chor The project file.
/// @param originalTrajectory The original trajectory file.
/// @return The generated trajectory file, ready to be written to file, sent to
/// a client, etc.
template <choreo::ChoreoTrajectoryGenerator Generator>
choreo::document::Trajectory generate(
  const choreo::document::Project& chor,
  const choreo::document::Trajectory& originalTrajectory,
    const choreo::progress_update_sender::Client* sender = nullptr) {
  // Intentionally make two copies of the trajectory file
  auto traj_unscratch = choreo::document::Trajectory{originalTrajectory};
  using Sample = typename Generator::Sample;
  std::function<void(const std::vector<Sample>&)> progress_callback;
  if (sender != nullptr) {
    progress_callback = [sender](const std::vector<Sample>& samples) {
      sender->sendIncompleteTrajectory(samples);
    };
  }

  Generator generator(choreo::document::Project{chor},
                      choreo::document::Trajectory{traj_unscratch},
                      std::move(progress_callback));

  auto samplesExp = generator.generate();
  if (!samplesExp) {
    throw std::runtime_error("Trajectory generation failed");
  }
  auto samples = *samplesExp;
  if (samples.empty()) {
    throw std::runtime_error("Trajectory generation returned no samples");
  }
  std::vector<choreo::Segment> segments = generator.get_segments();

  // create a vector of timestamps for the interval matching each waypoint
  std::vector<double> waypoint_timestamps;
  waypoint_timestamps.reserve(segments.size());
  std::vector<std::uint32_t> splits;
  size_t timestamp_index = 0;
  for (size_t segment_index = 0; segment_index < segments.size();
       ++segment_index) {
    const auto& segment = segments[segment_index];
    if (segment_index + 1 == segments.size()) {
      waypoint_timestamps.push_back(samples.back().time.value());
      if (segment.start.split) {
        splits.push_back(static_cast<std::uint32_t>(samples.size() - 1));
      }
      continue;
    }
    if (timestamp_index >= samples.size()) {
      throw std::runtime_error("Waypoint sample index is out of range");
    }
    waypoint_timestamps.push_back(samples[timestamp_index].time.value());
    if (segment.start.split) {
      splits.push_back(static_cast<std::uint32_t>(timestamp_index));
    }
    timestamp_index += segment.start.intervals;
  }

  choreo::document::TrajectoryOutput output;
  output.waypoints = waypoint_timestamps;
  output.splits = std::move(splits);
  if constexpr (std::is_same_v<typename Generator::Sample,
                               wpi::math::HolonomicSample>) {
    output.trajectory =
        std::optional<wpi::math::HolonomicTrajectory>{
            wpi::math::HolonomicTrajectory{samples}};
  } else {
    output.trajectory =
        std::optional<wpi::math::DifferentialTrajectory>{
            wpi::math::DifferentialTrajectory{samples}};
  }
  traj_unscratch.output = std::move(output);
  traj_unscratch.snapshot = originalTrajectory.params;
  traj_unscratch.config = chor.config;

  traj_unscratch.events = originalTrajectory.events;
  for (auto& event : traj_unscratch.events) {
    const auto waypoint = choreo::ResolveWaypointId(
        event.from.target, originalTrajectory.params.waypoints);
    if (waypoint && *waypoint < waypoint_timestamps.size()) {
      event.from.targetTime = waypoint_timestamps[*waypoint] +
                              event.from.offset.value.value();
    } else {
      event.from.targetTime.reset();
    }
  }
  return traj_unscratch;
}

/// @brief Reads the project and trajectory files from the specified paths and
/// generates a new trajectory file.
/// @param args The command line arguments containing the paths to the project
/// and trajectory files.
/// @return The generated trajectory file.
/// @throws std::runtime_error if the input files are invalid or generation
/// fails.
choreo::document::Trajectory read_and_generate(
    const CliArgs& args,
    const choreo::progress_update_sender::Client* sender = nullptr) {
  std::println("Generating trajectories from: {}", args.chor_path.string());
  std::println("Trajectory to generate: {}", args.traj_path.string());
  if (sender != nullptr) {
    sender->sendDiagnosticText("generator: loading input files");
  }
  // Read the project and trajectory documents from the specified paths.
  std::string chor_contents;
  std::string traj_contents;
  {
    std::ifstream chor_file(args.chor_path);
    chor_contents = std::string(std::istreambuf_iterator<char>(chor_file),
                                std::istreambuf_iterator<char>());
    std::ifstream traj_file(args.traj_path);
    traj_contents = std::string(std::istreambuf_iterator<char>(traj_file),
                                std::istreambuf_iterator<char>());
  }
    choreo::document::Project chor = wpi::util::json::parse_or_throw(
                       std::string_view{chor_contents})
                       .get<choreo::document::Project>();
    choreo::document::Trajectory traj = wpi::util::json::parse_or_throw(
                        std::string_view{traj_contents})
                        .get<choreo::document::Trajectory>();

  validate_generation_input(chor, traj);

  // This copy will not be modified unless generation is successful.
  auto driveType = chor.type;
  switch (driveType) {
    case choreo::document::DriveType::Swerve:
      return generate<choreo::TrajectoryGenerator<
          wpi::math::HolonomicSample, trajopt::SwerveSolution,
          trajopt::SwerveDrivetrain, trajopt::SwerveTrajectoryGenerator,
          trajopt::SwerveTrajectory>>(chor, traj, sender);
    case choreo::document::DriveType::Differential:
      return generate<choreo::TrajectoryGenerator<
          wpi::math::DifferentialSample, trajopt::DifferentialSolution,
          trajopt::DifferentialDrivetrain,
          trajopt::DifferentialTrajectoryGenerator,
          trajopt::DifferentialTrajectory>>(chor, traj, sender);
    default:
      throw std::runtime_error("Unsupported drive type");
  }
}

int main(int argc, char** argv) {
  std::println("Choreo Generator CLI");

  try {
    CliArgs args = parse_arguments(argc, argv);
    std::optional<choreo::progress_update_sender::Client> progress_sender;

    if (!args.progress_url.empty()) {
      progress_sender.emplace();
      progress_sender->open(args.progress_url);
      progress_sender->sendDiagnosticText(
          "generator: progress sender initialized");
    }

    if (!args.error_message.empty()) {
      std::println(stderr, "Error: {}", args.error_message);
      if (progress_sender.has_value()) {
        progress_sender->sendError(args.error_message);
      }
      return 1;
    }
    const choreo::document::Trajectory traj = read_and_generate(
        args, progress_sender.has_value() ? &*progress_sender : nullptr);

    // at this point the traj is fully edited. Send it where it needs to go.
    const auto serialized_traj = wpi::util::json(traj).to_string();

    if (!args.output_path.empty()) {
      std::ofstream output_file(args.output_path);
      output_file << wpi::util::json(traj).to_string_pretty();
    }

    if (progress_sender.has_value()) {
      progress_sender->sendCompleteTrajectory(serialized_traj);
    }
    std::println("Trajectory generation complete");
    return 0;
  } catch (const std::exception& e) {
    std::println(stderr, "Generator runtime error: {}", e.what());
    return 2;
  } catch (...) {
    std::println(stderr, "Generator runtime error: unknown exception");
    return 3;
  }
}

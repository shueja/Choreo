// Copyright (c) Choreo contributors

#pragma once
#include <expected>
#include <functional>
#include <print>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include <choreo/document/project.hpp>
#include <choreo/document/trajectory.hpp>
#include <wpi/math/trajectory/DifferentialSample.hpp>
#include <wpi/math/trajectory/HolonomicSample.hpp>
#include <sleipnir/optimization/solver/exit_status.hpp>
#include <trajopt/geometry/pose2.hpp>
#include <trajopt/path/path_builder.hpp>

#include "segment.hpp"
#include "split_to_segments.hpp"
#include "document_adapter.hpp"

namespace choreo {
template <typename SampleType, typename TrajoptSolutionType,
          typename TrajoptDrivetrainType, typename GeneratorType,
          typename TrajectoryType>
class TrajectoryGenerator {
 public:
  using Sample = SampleType;
  using Builder =
      trajopt::PathBuilder<TrajoptDrivetrainType, TrajoptSolutionType>;
  // Owns the project and trajectory documents.
  TrajectoryGenerator(
      document::Project projectFile, document::Trajectory trajectoryFile,
      std::function<void(const std::vector<Sample>&)> progress_callback = {})
      : projectFile(std::move(projectFile)),
        trajectoryFile(std::move(trajectoryFile)),
        progress_callback(std::move(progress_callback)) {
    // This is still one segment per Choreo waypoint.
    segments = convert_to_segments(this->trajectoryFile.params);

    for (size_t i = 0; i + 1 < segments.size(); ++i) {
      segments[i].update_start_intervals(
          this->trajectoryFile.params.targetDt.value, segments[i + 1],
          this->projectFile.config);
    }
    if constexpr (std::is_same_v<TrajoptDrivetrainType,
                                 trajopt::SwerveDrivetrain>) {
      generator.set_drivetrain(ToSwerveDrivetrain(this->projectFile.config));
    } else if constexpr (std::is_same_v<TrajoptDrivetrainType,
                                        trajopt::DifferentialDrivetrain>) {
      generator.set_drivetrain(
          ToDifferentialDrivetrain(this->projectFile.config));
    }
    apply_waypoints();
    apply_constraints();
  }
  std::vector<Segment> get_segments() const { return segments; }
  // TODO: Have generate() return the updated trajectory document.
  std::expected<std::vector<Sample>, slp::ExitStatus> generate() {
    return generate_internal();
  }

 private:
  document::Project projectFile;
  document::Trajectory trajectoryFile;
  std::vector<choreo::Segment> segments;
  std::vector<choreo::Segment> only_waypoint_segments;
  Builder generator;
  std::function<void(const std::vector<Sample>&)> progress_callback;

  std::vector<Sample> to_samples(const TrajoptSolutionType& solution) const {
    auto trajectory = TrajectoryType(solution);
    std::vector<Sample> samples;
    samples.reserve(trajectory.samples.size());
    for (const auto& sample : trajectory.samples) {
      samples.emplace_back(ToWPILibSample(sample));
    }
    return samples;
  }

  void apply_constraints() {
    const auto bumperPoints = ToTrajoptBumperPoints(projectFile.config);
    // Now that our segments list is reindexed to match TrajoptLib's counts, we
    // can apply the constraints
    for (int j = 0; j < only_waypoint_segments.size();
         j++) {  // only for waypoints with a segment after them.
      if (j < only_waypoint_segments.size() - 1) {
        for (const auto& constraint :
             only_waypoint_segments[j].segment_constraints) {
            generator.add_segment_constraint(
              j, j + 1,
              ToTrajoptConstraint(constraint, bumperPoints));
        }
      }
      for (const auto& constraint :
           only_waypoint_segments[j].waypoint_constraints) {
        generator.add_waypoint_constraint(
          j, ToTrajoptConstraint(constraint, bumperPoints));
      }
    }
  }
  void apply_waypoints() {
    std::vector<trajopt::Pose2d> initial_guess_points;
    std::vector<size_t> intervals;
    for (int i = 0; i < segments.size(); i++) {
      const auto& wpt = segments[i].start;
      if (!segments[i].coalesce_with_previous) {
        if (i != segments.size() - 1) {
          intervals.push_back(wpt.intervals);
        }
        if (!only_waypoint_segments.empty()) {
          generator.sgmt_initial_guess_points(only_waypoint_segments.size() - 1,
                                              initial_guess_points);
        }
        initial_guess_points.clear();
        const auto& pose = wpt.pose.value;
        if (wpt.fixTranslation && wpt.fixHeading) {
          generator.pose_wpt(only_waypoint_segments.size(),
                             ToTrajoptPose(wpt));
        } else if (wpt.fixTranslation && !wpt.fixHeading) {
          generator.translation_wpt(only_waypoint_segments.size(),
                                    {pose.X().value(), pose.Y().value()},
                                    pose.Rotation().Radians().value());
        } else if (!wpt.fixTranslation && wpt.fixHeading) {
          generator.empty_wpt(only_waypoint_segments.size(),
                              ToTrajoptPose(wpt));
          generator.add_waypoint_constraint(
              only_waypoint_segments.size(),
              trajopt::HeadingAngleConstraint{
                  pose.Rotation().Radians().value(), 0.0});
        } else {
          generator.empty_wpt(only_waypoint_segments.size(),
                              ToTrajoptPose(wpt));
        }
        only_waypoint_segments.push_back(segments[i]);
      } else {
        intervals.back() += wpt.intervals;
        initial_guess_points.push_back(ToTrajoptPose(wpt));
      }
    }
    std::println("Intervals: {}", intervals);
    generator.set_control_interval_counts(std::move(intervals));
  }

  std::expected<std::vector<Sample>, slp::ExitStatus> generate_internal() {
    if (progress_callback) {
      generator.add_callback(
          [this](const TrajoptSolutionType& partial_solution, int64_t) {
            progress_callback(to_samples(partial_solution));
          });
    }

    GeneratorType traj_generator{generator};
    auto solution = traj_generator.generate(true);
    if (!solution) {
      return std::unexpected(solution.error());
    }
    return to_samples(solution.value());
  }
};

template <typename T>
struct is_choreo_trajectory_generator : std::false_type {};

template <typename DriveType, typename SolutionType, typename DrivetrainType,
          typename TrajectoryGeneratorType, typename TrajectoryType>
struct is_choreo_trajectory_generator<
    choreo::TrajectoryGenerator<DriveType, SolutionType, DrivetrainType,
                                TrajectoryGeneratorType, TrajectoryType>>
    : std::true_type {};

template <typename T>
concept ChoreoTrajectoryGenerator =
    is_choreo_trajectory_generator<std::remove_cvref_t<T>>::value;

}  // namespace choreo

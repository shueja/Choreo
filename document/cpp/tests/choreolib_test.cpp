#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include <wpi/util/json.hpp>

#include "choreo/Choreo.hpp"
#include "choreo/util/TrajSchemaVersion.hpp"

int main(int argc, char** argv) {
  static_assert(choreo::kTrajSchemaVersion ==
                choreo::document::kTrajectorySchemaVersion);
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  wpi::math::HolonomicSample sample;
  const auto root = wpi::util::json::object(
      "uuid", "path", "version", 4, "name", "Path", "config", nullptr,
      "snapshot", nullptr, "params",
      wpi::util::json::object(
          "waypoints", wpi::util::json::array(), "constraints",
          wpi::util::json::array(), "targetDt",
          wpi::util::json::object("exp", "0.02 s", "val", 0.02)),
      "output",
      wpi::util::json::object(
          "sampleType", "Swerve", "waypoints", std::vector<double>{0.0},
          "splits", std::vector<int>{0}, "trajectory",
          wpi::util::json::object(
              "samples", std::vector<wpi::math::HolonomicSample>{sample})),
      "events", std::vector<wpi::util::json>{});

  const auto trajectory =
      choreo::Choreo::LoadTrajectoryString(root.to_string());
  const auto& samples = std::get<std::optional<wpi::math::HolonomicTrajectory>>(
                            trajectory.output.trajectory)
                            ->Samples();
  static_assert(std::is_same_v<
                typename std::remove_cvref_t<decltype(samples)>::value_type,
                wpi::math::HolonomicSample>);
  if (trajectory.name != "Path" || samples.size() != 1 ||
      samples.front().time.value() != 0.0) {
    return EXIT_FAILURE;
  }
  wpi::math::DifferentialSample differential;
  differential.leftVelocity = wpi::units::meters_per_second_t{2.0};
  auto differentialRoot = root;
  differentialRoot["output"]["sampleType"] = "Differential";
  differentialRoot["output"]["trajectory"]["samples"] =
      std::vector{differential};
  const auto tank =
      choreo::Choreo::LoadTrajectoryString(differentialRoot.to_string());
  const auto& tankSamples =
      std::get<std::optional<wpi::math::DifferentialTrajectory>>(
          tank.output.trajectory)
          ->Samples();
  if (tankSamples.size() != 1 ||
      tankSamples.front().leftVelocity.value() != 2.0) {
    return EXIT_FAILURE;
  }
  const std::filesystem::path path{argv[1]};
  {
    std::ofstream output{path, std::ios::binary};
    output << root.to_string();
    if (!output) {
      return EXIT_FAILURE;
    }
  }
  const auto loaded = choreo::Choreo::LoadTrajectory(path);
  std::filesystem::remove(path);
  if (loaded.name != trajectory.name) {
    return EXIT_FAILURE;
  }
  try {
    choreo::Choreo::LoadTrajectory("nonexistent-choreo-trajectory.traj");
    return EXIT_FAILURE;
  } catch (const std::invalid_argument&) {
  }
  return EXIT_SUCCESS;
}
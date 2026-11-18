#include "choreo/document/trajectory_output.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {
namespace {

using wpi::util::json;

std::vector<std::uint32_t> ReadSplits(const json& value,
                                      std::size_t sampleCount) {
  std::vector<std::uint32_t> splits;
  for (const auto& entry : value.get_array()) {
    if (!entry.is_int() || entry.get_int() < 0 ||
        (sampleCount != 0 &&
         static_cast<std::uint64_t>(entry.get_int()) >= sampleCount) ||
        static_cast<std::uint64_t>(entry.get_int()) >
            std::numeric_limits<std::uint32_t>::max()) {
      throw std::invalid_argument("trajectory split index is out of range");
    }
    const auto index = static_cast<std::uint32_t>(entry.get_int());
    if (!splits.empty() && index <= splits.back()) {
      throw std::invalid_argument(
          "trajectory split indices must be strictly increasing");
    }
    splits.push_back(index);
  }
  if (sampleCount != 0 && (splits.empty() || splits.front() != 0)) {
    splits.insert(splits.begin(), 0);
  }
  return splits;
}

template <typename Sample, typename TrajectoryType>
std::pair<TrajectoryType, double> ReadSamples(
    const json& serialized, const std::vector<std::uint32_t>& splits) {
  const auto& array = serialized.at("samples").get_array();
  if (array.empty()) {
    throw std::invalid_argument("generated trajectory has no samples");
  }
  std::vector<Sample> samples;
  samples.reserve(array.size());
  for (const auto& entry : array) {
    if constexpr (std::is_same_v<Sample, wpi::math::DifferentialSample>) {
      detail::CheckProperties(
          entry, {"time", "pose", "velocity", "acceleration", "leftVelocity",
                  "rightVelocity"});
    } else {
      detail::CheckProperties(entry,
                              {"time", "pose", "velocity", "acceleration"});
    }
    detail::CheckPose(entry.at("pose"));
    detail::CheckProperties(entry.at("velocity"), {"vx", "vy", "omega"});
    detail::CheckProperties(entry.at("acceleration"), {"ax", "ay", "alpha"});
    auto sample = entry.get<Sample>();
    if (!std::isfinite(sample.time.value()) ||
        !std::isfinite(sample.pose.X().value()) ||
        !std::isfinite(sample.pose.Y().value()) ||
        !std::isfinite(sample.pose.Rotation().Radians().value()) ||
        !std::isfinite(sample.velocity.vx.value()) ||
        !std::isfinite(sample.velocity.vy.value()) ||
        !std::isfinite(sample.velocity.omega.value()) ||
        !std::isfinite(sample.acceleration.ax.value()) ||
        !std::isfinite(sample.acceleration.ay.value()) ||
        !std::isfinite(sample.acceleration.alpha.value())) {
      throw std::invalid_argument("sample values must be finite");
    }
    if constexpr (std::is_same_v<Sample, wpi::math::DifferentialSample>) {
      if (!std::isfinite(sample.leftVelocity.value()) ||
          !std::isfinite(sample.rightVelocity.value())) {
        throw std::invalid_argument("wheel velocities must be finite");
      }
    }
    samples.push_back(std::move(sample));
  }

  std::vector<std::size_t> order(samples.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::stable_sort(order.begin(), order.end(),
                   [&](std::size_t left, std::size_t right) {
                     return samples[left].time < samples[right].time;
                   });
  for (std::size_t index = 0; index < order.size(); ++index) {
    const auto originalSegment =
        std::upper_bound(splits.begin(), splits.end(), order[index]);
    const auto sortedSegment =
        std::upper_bound(splits.begin(), splits.end(), index);
    if (originalSegment != sortedSegment) {
      throw std::invalid_argument(
          "sorting samples would change trajectory split membership");
    }
  }
  for (auto split : splits) {
    if (split != 0 && order[split] != split) {
      throw std::invalid_argument(
          "sorting samples would change a trajectory split boundary");
    }
  }

  const double origin = samples[order.front()].time.value();
  std::vector<Sample> sorted;
  sorted.reserve(samples.size());
  for (auto index : order) {
    auto sample = samples[index];
    sample.time = wpi::units::second_t{sample.time.value() - origin};
    if (!std::isfinite(sample.time.value())) {
      throw std::invalid_argument("normalized sample timestamp must be finite");
    }
    sorted.push_back(std::move(sample));
  }
  return {TrajectoryType{std::move(sorted)}, origin};
}

}

TrajectoryOutput ReadTrajectoryOutput(const json& serialized, double& origin) {
  detail::CheckProperties(serialized,
                          {"sampleType", "waypoints", "splits", "trajectory"});
  detail::CheckProperties(serialized.at("trajectory"), {"samples"});
  const auto& samples = serialized.at("trajectory").at("samples").get_array();
  TrajectoryOutput result;
  result.splits = ReadSplits(serialized.at("splits"), samples.size());
  const auto sampleType = serialized.at("sampleType").get_string();
  origin = 0;
  if (sampleType == "Swerve") {
    std::optional<wpi::math::HolonomicTrajectory> trajectory;
    if (!samples.empty()) {
      auto [value, start] = ReadSamples<wpi::math::HolonomicSample,
                                        wpi::math::HolonomicTrajectory>(
          serialized.at("trajectory"), result.splits);
      trajectory = std::move(value);
      origin = start;
    }
    result.trajectory = std::move(trajectory);
  } else if (sampleType == "Differential") {
    std::optional<wpi::math::DifferentialTrajectory> trajectory;
    if (!samples.empty()) {
      auto [value, start] = ReadSamples<wpi::math::DifferentialSample,
                                        wpi::math::DifferentialTrajectory>(
          serialized.at("trajectory"), result.splits);
      trajectory = std::move(value);
      origin = start;
    }
    result.trajectory = std::move(trajectory);
  } else {
    throw std::invalid_argument("unsupported trajectory sample type");
  }
  for (const auto& time : serialized.at("waypoints").get_array()) {
    const double normalized = time.get_number() - origin;
    if (!std::isfinite(normalized)) {
      throw std::invalid_argument(
          "normalized waypoint timestamp must be finite");
    }
    result.waypoints.push_back(normalized);
  }
  return result;
}

void to_json(json& serialized, const TrajectoryOutput& output) {
  serialized =
      json::object("waypoints", output.waypoints, "splits", output.splits);
  std::visit(
      [&](const auto& trajectory) {
        using TrajectoryType =
            typename std::remove_cvref_t<decltype(trajectory)>::value_type;
        serialized["sampleType"] =
            std::is_same_v<TrajectoryType, wpi::math::HolonomicTrajectory>
                ? "Swerve"
                : "Differential";
        serialized["trajectory"] = trajectory
                                       ? json{*trajectory}
                                       : json::object("samples", json::array());
      },
      output.trajectory);
}

void from_json(const json& serialized, TrajectoryOutput& output) {
  double origin;
  output = ReadTrajectoryOutput(serialized, origin);
}

}
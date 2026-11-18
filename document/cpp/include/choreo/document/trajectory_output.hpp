#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include <wpi/math/trajectory/DifferentialTrajectory.hpp>
#include <wpi/math/trajectory/HolonomicTrajectory.hpp>
#include <wpi/util/json.hpp>

namespace choreo::document {

class TrajectoryOutput {
 public:
  std::vector<double> waypoints;
  std::vector<std::uint32_t> splits;
  std::variant<std::optional<wpi::math::HolonomicTrajectory>,
               std::optional<wpi::math::DifferentialTrajectory>>
      trajectory;
};

void to_json(wpi::util::json& json, const TrajectoryOutput& value);
void from_json(const wpi::util::json& json, TrajectoryOutput& value);
TrajectoryOutput ReadTrajectoryOutput(const wpi::util::json& json,
                                      double& origin);

}
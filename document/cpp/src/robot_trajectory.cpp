#include "choreo/document/robot_trajectory.hpp"

#include <stdexcept>

#include <wpi/util/json.hpp>

namespace choreo::document {

RobotTrajectory LoadRobotTrajectory(std::string_view content) {
  const auto trajectory =
      wpi::util::json::parse_or_throw(content).get<Trajectory>();
  if (!trajectory.output || std::visit([](const auto& value) { return !value; },
                                       trajectory.output->trajectory)) {
    throw std::invalid_argument("trajectory has no generated samples");
  }
  RobotTrajectory result;
  result.name = trajectory.name;
  result.output = *trajectory.output;
  for (const auto& event : trajectory.events) {
    if (event.from.targetTime) {
      result.events.push_back(event);
    } else {
      result.warnings.push_back("unresolved event: " + event.uuid);
    }
  }
  return result;
}

}
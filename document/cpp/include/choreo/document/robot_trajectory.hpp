#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "choreo/document/document.hpp"

namespace choreo::document {

class RobotTrajectory {
 public:
  std::string name;
  TrajectoryOutput output;
  std::vector<EventMarker> events;
  std::vector<std::string> warnings;
};

RobotTrajectory LoadRobotTrajectory(std::string_view content);

}
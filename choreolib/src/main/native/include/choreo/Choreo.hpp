#pragma once

#include <filesystem>
#include <string_view>

#include "choreo/document/robot_trajectory.hpp"

namespace choreo {

using Trajectory = document::RobotTrajectory;

class Choreo {
 public:
  static Trajectory LoadTrajectory(const std::filesystem::path& path);
  static Trajectory LoadTrajectoryString(std::string_view content);
};

}
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "choreo/document/event_marker.hpp"
#include "choreo/document/parameters.hpp"
#include "choreo/document/robot_config.hpp"
#include "choreo/document/trajectory_output.hpp"
#include "choreo/document/trajectory_schema_version.hpp"

namespace choreo::document {

class Trajectory {
 public:
  std::string uuid;
  std::string name;
  std::uint32_t version = kTrajectorySchemaVersion;
  std::optional<RobotConfig> config;
  std::optional<Parameters> snapshot;
  Parameters params;
  std::optional<TrajectoryOutput> output;
  std::vector<EventMarker> events;
};

void to_json(wpi::util::json& json, const Trajectory& trajectory);
void from_json(const wpi::util::json& json, Trajectory& trajectory);

}
#include "choreo/document/trajectory.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

using wpi::util::json;

void to_json(json& serialized, const Trajectory& trajectory) {
  if (trajectory.version != kTrajectorySchemaVersion) {
    throw std::invalid_argument("unsupported trajectory schema version");
  }
  serialized = json::object(
      "uuid", trajectory.uuid, "name", trajectory.name, "version",
      trajectory.version, "config",
      trajectory.config ? json{*trajectory.config} : json{}, "snapshot",
      trajectory.snapshot ? json{*trajectory.snapshot} : json{}, "params",
      trajectory.params, "output",
      trajectory.output ? json{*trajectory.output} : json{}, "events",
      trajectory.events);
}

void from_json(const json& root, Trajectory& trajectory) {
  detail::CheckProperties(root, {"uuid", "name", "version", "config",
                                 "snapshot", "params", "output", "events"});
  if (!root.at("version").is_int() ||
      root.at("version").get_int() != kTrajectorySchemaVersion) {
    throw std::invalid_argument("unsupported trajectory schema version");
  }
  Trajectory result;
  result.uuid = root.at("uuid").get_string();
  result.name = root.at("name").get_string();
  if (!root.at("config").is_null()) {
    result.config = root.at("config").get<RobotConfig>();
  }
  if (!root.at("snapshot").is_null()) {
    result.snapshot = root.at("snapshot").get<Parameters>();
  }
  result.params = root.at("params").get<Parameters>();
  double origin = 0;
  if (!root.at("output").is_null()) {
    result.output = ReadTrajectoryOutput(root.at("output"), origin);
  }
  for (const auto& serializedEvent : root.at("events").get_array()) {
    auto event = serializedEvent.get<EventMarker>();
    if (event.from.targetTime) {
      *event.from.targetTime -= origin;
      if (!std::isfinite(*event.from.targetTime)) {
        throw std::invalid_argument("invalid event timestamp");
      }
    }
    result.events.push_back(std::move(event));
  }
  trajectory = std::move(result);
}

}
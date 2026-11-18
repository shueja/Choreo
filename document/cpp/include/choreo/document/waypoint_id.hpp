#pragma once

#include <string>
#include <variant>

#include <wpi/util/json.hpp>

namespace choreo::document {

enum class WaypointBoundary { First, Last };
using WaypointId = std::variant<std::string, WaypointBoundary>;

WaypointId ReadWaypointId(const wpi::util::json& json);
wpi::util::json WriteWaypointId(const WaypointId& waypoint);

}
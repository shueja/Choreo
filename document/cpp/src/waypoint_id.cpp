#include "choreo/document/waypoint_id.hpp"

#include <stdexcept>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

WaypointId ReadWaypointId(const wpi::util::json& value) {
  if (value.is_string()) {
    const auto name = value.get_string();
    if (name == "first") {
      return WaypointBoundary::First;
    }
    if (name == "last") {
      return WaypointBoundary::Last;
    }
    throw std::invalid_argument("invalid waypoint boundary");
  }
  detail::CheckProperties(value, {"uuid"});
  return value.at("uuid").get_string();
}

wpi::util::json WriteWaypointId(const WaypointId& waypoint) {
  if (auto* uuid = std::get_if<std::string>(&waypoint)) {
    return wpi::util::json::object("uuid", *uuid);
  }
  switch (std::get<WaypointBoundary>(waypoint)) {
    case WaypointBoundary::First:
      return wpi::util::json{"first"};
    case WaypointBoundary::Last:
      return wpi::util::json{"last"};
  }
  throw std::invalid_argument("invalid waypoint boundary");
}

}
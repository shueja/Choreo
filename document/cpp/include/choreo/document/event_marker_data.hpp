#pragma once

#include <optional>

#include "choreo/document/expression.hpp"
#include "choreo/document/waypoint_id.hpp"

namespace choreo::document {

class EventMarkerData {
 public:
  WaypointId target;
  std::optional<double> targetTime;
  Expression<dimension::Time> offset;
};

void to_json(wpi::util::json& json, const EventMarkerData& value);
void from_json(const wpi::util::json& json, EventMarkerData& value);

}
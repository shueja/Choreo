#pragma once

#include <string>

#include "choreo/document/event_marker_data.hpp"

namespace choreo::document {

class EventMarker {
 public:
  std::string uuid;
  std::string name;
  EventMarkerData from;
};

void to_json(wpi::util::json& json, const EventMarker& value);
void from_json(const wpi::util::json& json, EventMarker& value);

}
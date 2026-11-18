#include "choreo/document/event_marker_data.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const EventMarkerData& value) {
  if (value.targetTime && !std::isfinite(*value.targetTime)) {
    throw std::invalid_argument("event timestamp must be finite");
  }
  json = wpi::util::json::object(
      "target", WriteWaypointId(value.target), "targetTime",
      value.targetTime ? wpi::util::json{*value.targetTime} : wpi::util::json{},
      "offset", value.offset);
}

void from_json(const wpi::util::json& json, EventMarkerData& value) {
  detail::CheckProperties(json, {"target", "targetTime", "offset"});
  EventMarkerData result;
  result.target = ReadWaypointId(json.at("target"));
  if (!json.at("targetTime").is_null()) {
    result.targetTime = json.at("targetTime").get_number();
    if (!std::isfinite(*result.targetTime)) {
      throw std::invalid_argument("event timestamp must be finite");
    }
  }
  result.offset = json.at("offset").get<Expression<dimension::Time>>();
  value = std::move(result);
}

}
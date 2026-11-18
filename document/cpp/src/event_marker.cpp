#include "choreo/document/event_marker.hpp"

#include <utility>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const EventMarker& value) {
  detail::CheckUuid(value.uuid);
  json = wpi::util::json::object("uuid", value.uuid, "name", value.name, "from",
                                 value.from);
}

void from_json(const wpi::util::json& json, EventMarker& value) {
  detail::CheckProperties(json, {"uuid", "name", "from"});
  EventMarker result{json.at("uuid").get_string(), json.at("name").get_string(),
                     json.at("from").get<EventMarkerData>()};
  detail::CheckUuid(result.uuid);
  value = std::move(result);
}

}
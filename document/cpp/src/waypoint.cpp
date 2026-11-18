#include "choreo/document/waypoint.hpp"

#include <limits>
#include <stdexcept>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

wpi::util::json Waypoint::ToJson() const {
  detail::CheckUuid(uuid);
  wpi::util::json json;
  json.set_object();
  json["uuid"] = uuid;
  json["pose"] = pose.ToJson();
  json["intervals"] = intervals;
  json["split"] = split;
  json["fixTranslation"] = fixTranslation;
  json["fixHeading"] = fixHeading;
  json["overrideIntervals"] = overrideIntervals;
  return json;
}

Waypoint Waypoint::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(
      json, {"uuid", "pose", "intervals", "split", "fixTranslation",
             "fixHeading", "overrideIntervals"});
  const auto& intervals = json.at("intervals");
  if (!intervals.is_int() || intervals.get_int() < 0 ||
      intervals.get_int() > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument(
        "waypoint intervals must be a nonnegative integer");
  }
        const auto uuid = json.at("uuid").get_string();
        detail::CheckUuid(uuid);
        return {uuid,
          PoseExpression::FromJson(json.at("pose")),
          static_cast<std::uint32_t>(intervals.get_int()),
          json.at("split").get_bool(),
          json.at("fixTranslation").get_bool(),
          json.at("fixHeading").get_bool(),
          json.at("overrideIntervals").get_bool()};
}

void to_json(wpi::util::json& json, const Waypoint& waypoint) {
  json = waypoint.ToJson();
}
void from_json(const wpi::util::json& json, Waypoint& waypoint) {
  waypoint = Waypoint::FromJson(json);
}

}
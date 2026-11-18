#include "choreo/document/parameters.hpp"

#include <utility>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const Parameters& value) {
  json = wpi::util::json::object("waypoints", value.waypoints, "constraints",
                                 value.constraints, "targetDt", value.targetDt);
}

void from_json(const wpi::util::json& json, Parameters& value) {
  detail::CheckProperties(json, {"waypoints", "constraints", "targetDt"});
  Parameters result;
  for (const auto& waypoint : json.at("waypoints").get_array()) {
    result.waypoints.push_back(waypoint.get<Waypoint>());
  }
  for (const auto& constraint : json.at("constraints").get_array()) {
    result.constraints.push_back(constraint.get<Constraint>());
  }
  result.targetDt = json.at("targetDt").get<Expression<dimension::Time>>();
  value = std::move(result);
}

}
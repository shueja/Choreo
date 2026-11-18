#include "choreo/document/robot_config.hpp"

#include "choreo/document/json_validation.hpp"
namespace choreo::document {
wpi::util::json RobotConfig::ToJson() const {
  return wpi::util::json::object("mass", mass.ToJson(), "inertia",
                                 inertia.ToJson(), "gearing", gearing.ToJson(),
                                 "radius", radius.ToJson(), "cof", cof.ToJson(),
                                 "wheels", wheels.ToJson(), "bumpers",
                                 bumpers.ToJson(), "motor", motor.ToJson());
}
RobotConfig RobotConfig::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(json, {"mass", "inertia", "gearing", "radius", "cof",
                                 "wheels", "bumpers", "motor"});
  return {decltype(mass)::FromJson(json.at("mass")),
          decltype(inertia)::FromJson(json.at("inertia")),
          decltype(gearing)::FromJson(json.at("gearing")),
          decltype(radius)::FromJson(json.at("radius")),
          decltype(cof)::FromJson(json.at("cof")),
          Wheels::FromJson(json.at("wheels")),
          PolygonExpression::FromJson(json.at("bumpers")),
          MotorConfig::FromJson(json.at("motor"))};
}
void to_json(wpi::util::json& json, const RobotConfig& value) {
  json = value.ToJson();
}
void from_json(const wpi::util::json& json, RobotConfig& value) {
  value = RobotConfig::FromJson(json);
}
}
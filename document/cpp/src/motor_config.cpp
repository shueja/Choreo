#include "choreo/document/motor_config.hpp"

#include "choreo/document/json_validation.hpp"
namespace choreo::document {
wpi::util::json MotorConfig::ToJson() const {
  return wpi::util::json::object(
      "freeSpeed", freeSpeed.ToJson(), "stallTorque", stallTorque.ToJson(),
      "kT", kT.ToJson(), "kV", kV.ToJson(), "supplyLimit", supplyLimit.ToJson(),
      "statorLimit", statorLimit.ToJson());
}
MotorConfig MotorConfig::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(json, {"freeSpeed", "stallTorque", "kT", "kV",
                                 "supplyLimit", "statorLimit"});
  return {decltype(freeSpeed)::FromJson(json.at("freeSpeed")),
          decltype(stallTorque)::FromJson(json.at("stallTorque")),
          decltype(kT)::FromJson(json.at("kT")),
          decltype(kV)::FromJson(json.at("kV")),
          decltype(supplyLimit)::FromJson(json.at("supplyLimit")),
          decltype(statorLimit)::FromJson(json.at("statorLimit"))};
}
void to_json(wpi::util::json& json, const MotorConfig& value) {
  json = value.ToJson();
}
void from_json(const wpi::util::json& json, MotorConfig& value) {
  value = MotorConfig::FromJson(json);
}
}
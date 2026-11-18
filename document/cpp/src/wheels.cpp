#include "choreo/document/wheels.hpp"

#include <stdexcept>

#include "choreo/document/json_validation.hpp"
namespace choreo::document {
wpi::util::json Wheels::ToJson() const {
  if (!differential && !swerve && !mecanum) {
    throw std::invalid_argument("at least one wheel configuration is required");
  }
  auto json = wpi::util::json::object();
  if (differential) {
    json["differential"] = differential->ToJson();
  }
  if (swerve) {
    json["swerve"] = swerve->ToJson();
  }
  if (mecanum) {
    json["mecanum"] = mecanum->ToJson();
  }
  return json;
}
Wheels Wheels::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(json, {"differential", "swerve", "mecanum"});
  Wheels result;
  if (json.contains("differential")) {
    result.differential = DifferentialWheels::FromJson(json.at("differential"));
  }
  if (json.contains("swerve")) {
    result.swerve = SwerveWheels::FromJson(json.at("swerve"));
  }
  if (json.contains("mecanum")) {
    result.mecanum = MecanumWheels::FromJson(json.at("mecanum"));
  }
  if (!result.differential && !result.swerve && !result.mecanum) {
    throw std::invalid_argument("at least one wheel configuration is required");
  }
  return result;
}
void to_json(wpi::util::json& json, const Wheels& value) {
  json = value.ToJson();
}
void from_json(const wpi::util::json& json, Wheels& value) {
  value = Wheels::FromJson(json);
}
}
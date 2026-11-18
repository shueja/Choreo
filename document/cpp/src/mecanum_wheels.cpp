#include "choreo/document/mecanum_wheels.hpp"

#include "choreo/document/json_validation.hpp"
namespace choreo::document {
wpi::util::json MecanumWheels::ToJson() const {
  return wpi::util::json::object(
      "frontLeft", frontLeft.ToJson(), "frontRight", frontRight.ToJson(),
      "backLeft", backLeft.ToJson(), "backRight", backRight.ToJson());
}
MecanumWheels MecanumWheels::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(json,
                          {"frontLeft", "frontRight", "backLeft", "backRight"});
  return {TranslationExpression::FromJson(json.at("frontLeft")),
          TranslationExpression::FromJson(json.at("frontRight")),
          TranslationExpression::FromJson(json.at("backLeft")),
          TranslationExpression::FromJson(json.at("backRight"))};
}
void to_json(wpi::util::json& json, const MecanumWheels& value) {
  json = value.ToJson();
}
void from_json(const wpi::util::json& json, MecanumWheels& value) {
  value = MecanumWheels::FromJson(json);
}
}
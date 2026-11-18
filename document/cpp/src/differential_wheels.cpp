#include "choreo/document/differential_wheels.hpp"

#include "choreo/document/json_validation.hpp"
namespace choreo::document {
wpi::util::json DifferentialWheels::ToJson() const {
  return wpi::util::json::object("trackwidth", trackwidth.ToJson());
}
DifferentialWheels DifferentialWheels::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(json, {"trackwidth"});
  return {decltype(trackwidth)::FromJson(json.at("trackwidth"))};
}
void to_json(wpi::util::json& json, const DifferentialWheels& value) {
  json = value.ToJson();
}
void from_json(const wpi::util::json& json, DifferentialWheels& value) {
  value = DifferentialWheels::FromJson(json);
}
}
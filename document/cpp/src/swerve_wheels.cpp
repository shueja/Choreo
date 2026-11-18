#include "choreo/document/swerve_wheels.hpp"

#include <stdexcept>

#include "choreo/document/json_validation.hpp"
namespace choreo::document {
wpi::util::json SwerveWheels::ToJson() const {
  auto array = wpi::util::json::array();
  for (const auto& module : modules) {
    array.emplace_back(module.ToJson());
  }
  return wpi::util::json::object("modules", std::move(array));
}
SwerveWheels SwerveWheels::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(json, {"modules"});
  const auto& array = json.at("modules").get_array();
  if (array.size() != 4) {
    throw std::invalid_argument("swerve requires four modules");
  }
  SwerveWheels result;
  for (std::size_t index = 0; index < result.modules.size(); ++index) {
    result.modules[index] = TranslationExpression::FromJson(array[index]);
  }
  return result;
}
void to_json(wpi::util::json& json, const SwerveWheels& value) {
  json = value.ToJson();
}
void from_json(const wpi::util::json& json, SwerveWheels& value) {
  value = SwerveWheels::FromJson(json);
}
}
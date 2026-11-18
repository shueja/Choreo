#include "choreo/document/max_angular_velocity.hpp"

#include <stdexcept>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const MaxAngularVelocity& value) {
  json =
      wpi::util::json::object("type", "MaxAngularVelocity", "max", value.max);
}

void from_json(const wpi::util::json& json, MaxAngularVelocity& value) {
  detail::CheckProperties(json, {"type", "max"});
  if (json.at("type").get_string() != "MaxAngularVelocity") {
    throw std::invalid_argument("invalid constraint type");
  }
  value = {json.at("max").get<Expression<dimension::AngularVelocity>>()};
}

}
#include "choreo/document/max_linear_velocity.hpp"

#include <stdexcept>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const MaxLinearVelocity& value) {
  json = wpi::util::json::object("type", "MaxLinearVelocity", "max", value.max);
}

void from_json(const wpi::util::json& json, MaxLinearVelocity& value) {
  detail::CheckProperties(json, {"type", "max"});
  if (json.at("type").get_string() != "MaxLinearVelocity") {
    throw std::invalid_argument("invalid constraint type");
  }
  value = {json.at("max").get<Expression<dimension::LinearVelocity>>()};
}

}
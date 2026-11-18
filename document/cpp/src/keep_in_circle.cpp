#include "choreo/document/keep_in_circle.hpp"

#include <stdexcept>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const KeepInCircle& value) {
  if (value.circle.value.XSemiAxis() != value.circle.value.YSemiAxis()) {
    throw std::invalid_argument("circle ellipse must have equal semi-axes");
  }
  json =
      wpi::util::json::object("type", "KeepInCircle", "circle", value.circle);
}

void from_json(const wpi::util::json& json, KeepInCircle& value) {
  detail::CheckProperties(json, {"type", "circle"});
  if (json.at("type").get_string() != "KeepInCircle") {
    throw std::invalid_argument("invalid constraint type");
  }
  auto circle = json.at("circle").get<EllipseExpression>();
  if (circle.value.XSemiAxis() != circle.value.YSemiAxis()) {
    throw std::invalid_argument("circle ellipse must have equal semi-axes");
  }
  value = {std::move(circle)};
}

}
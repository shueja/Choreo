#include "choreo/document/heading_constraint.hpp"

#include <stdexcept>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const HeadingConstraint& value) {
  json = wpi::util::json::object("type", "Heading", "heading", value.heading,
                                 "tolerance", value.tolerance);
}

void from_json(const wpi::util::json& json, HeadingConstraint& value) {
  detail::CheckProperties(json, {"type", "heading", "tolerance"});
  if (json.at("type").get_string() != "Heading") {
    throw std::invalid_argument("invalid constraint type");
  }
  value = {json.at("heading").get<RotationExpression>(),
           json.at("tolerance").get<Expression<dimension::Angle>>()};
}

}
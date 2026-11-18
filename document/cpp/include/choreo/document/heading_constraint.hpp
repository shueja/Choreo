#pragma once

#include "choreo/document/expression.hpp"

namespace choreo::document {

class HeadingConstraint {
 public:
  RotationExpression heading;
  Expression<dimension::Angle> tolerance;
};

void to_json(wpi::util::json& json, const HeadingConstraint& value);
void from_json(const wpi::util::json& json, HeadingConstraint& value);

}
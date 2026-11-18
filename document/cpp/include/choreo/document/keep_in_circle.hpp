#pragma once

#include "choreo/document/expression.hpp"

namespace choreo::document {

class KeepInCircle {
 public:
  EllipseExpression circle;
};

void to_json(wpi::util::json& json, const KeepInCircle& value);
void from_json(const wpi::util::json& json, KeepInCircle& value);

}
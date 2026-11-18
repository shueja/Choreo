#pragma once

#include "choreo/document/expression.hpp"

namespace choreo::document {

class MaxLinearVelocity {
 public:
  Expression<dimension::LinearVelocity> max;
};

void to_json(wpi::util::json& json, const MaxLinearVelocity& value);
void from_json(const wpi::util::json& json, MaxLinearVelocity& value);

}
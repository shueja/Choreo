#pragma once

#include "choreo/document/expression.hpp"

namespace choreo::document {

class MaxAngularVelocity {
 public:
  Expression<dimension::AngularVelocity> max;
};

void to_json(wpi::util::json& json, const MaxAngularVelocity& value);
void from_json(const wpi::util::json& json, MaxAngularVelocity& value);

}
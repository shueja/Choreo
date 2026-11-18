#pragma once
#include "choreo/document/expression.hpp"
namespace choreo::document {
class DifferentialWheels {
 public:
  Expression<dimension::Length> trackwidth;
  wpi::util::json ToJson() const;
  static DifferentialWheels FromJson(const wpi::util::json& json);
};
void to_json(wpi::util::json& json, const DifferentialWheels& value);
void from_json(const wpi::util::json& json, DifferentialWheels& value);
}
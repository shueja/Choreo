#pragma once
#include "choreo/document/motor_config.hpp"
#include "choreo/document/wheels.hpp"
namespace choreo::document {
class RobotConfig {
 public:
  Expression<dimension::Mass> mass;
  Expression<dimension::MomentOfInertia> inertia;
  Expression<dimension::Dimensionless> gearing;
  Expression<dimension::Length> radius;
  Expression<dimension::Dimensionless> cof;
  Wheels wheels;
  PolygonExpression bumpers;
  MotorConfig motor;
  wpi::util::json ToJson() const;
  static RobotConfig FromJson(const wpi::util::json& json);
};
void to_json(wpi::util::json& json, const RobotConfig& value);
void from_json(const wpi::util::json& json, RobotConfig& value);
}
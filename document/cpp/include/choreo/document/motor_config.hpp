#pragma once
#include "choreo/document/expression.hpp"
namespace choreo::document {
class MotorConfig {
 public:
  Expression<dimension::AngularVelocity> freeSpeed;
  Expression<dimension::Torque> stallTorque;
  Expression<dimension::TorqueConstant> kT;
  Expression<dimension::BackEmfConstant> kV;
  Expression<dimension::Current> supplyLimit;
  Expression<dimension::Current> statorLimit;
  wpi::util::json ToJson() const;
  static MotorConfig FromJson(const wpi::util::json& json);
};
void to_json(wpi::util::json& json, const MotorConfig& value);
void from_json(const wpi::util::json& json, MotorConfig& value);
}
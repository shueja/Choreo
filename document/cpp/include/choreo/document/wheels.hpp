#pragma once
#include <optional>

#include "choreo/document/differential_wheels.hpp"
#include "choreo/document/mecanum_wheels.hpp"
#include "choreo/document/swerve_wheels.hpp"
namespace choreo::document {
class Wheels {
 public:
  std::optional<DifferentialWheels> differential;
  std::optional<SwerveWheels> swerve;
  std::optional<MecanumWheels> mecanum;
  wpi::util::json ToJson() const;
  static Wheels FromJson(const wpi::util::json& json);
};
void to_json(wpi::util::json& json, const Wheels& value);
void from_json(const wpi::util::json& json, Wheels& value);
}
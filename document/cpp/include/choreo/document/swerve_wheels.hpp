#pragma once
#include <array>

#include "choreo/document/expression.hpp"
namespace choreo::document {
class SwerveWheels {
 public:
  std::array<TranslationExpression, 4> modules;
  wpi::util::json ToJson() const;
  static SwerveWheels FromJson(const wpi::util::json& json);
};
void to_json(wpi::util::json& json, const SwerveWheels& value);
void from_json(const wpi::util::json& json, SwerveWheels& value);
}
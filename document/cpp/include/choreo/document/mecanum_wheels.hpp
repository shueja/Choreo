#pragma once
#include "choreo/document/expression.hpp"
namespace choreo::document {
class MecanumWheels {
 public:
  TranslationExpression frontLeft;
  TranslationExpression frontRight;
  TranslationExpression backLeft;
  TranslationExpression backRight;
  wpi::util::json ToJson() const;
  static MecanumWheels FromJson(const wpi::util::json& json);
};
void to_json(wpi::util::json& json, const MecanumWheels& value);
void from_json(const wpi::util::json& json, MecanumWheels& value);
}
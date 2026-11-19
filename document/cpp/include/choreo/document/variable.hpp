#pragma once

#include <map>
#include <optional>
#include <string>
#include <variant>

#include <wpi/math/geometry/Transform2d.hpp>
#include <wpi/math/geometry/Twist2d.hpp>
#include <wpi/math/shape/Rectangle2d.hpp>

#include "choreo/document/expression.hpp"

namespace choreo::document {

enum class VariableType {
  Quantity,
  Pose2d,
  Translation2d,
  Polygon2d,
  Rotation2d,
  Transform2d,
  Twist2d,
  Rectangle2d,
  Ellipse2d
};

using VariableValue =
    std::variant<double, wpi::math::Pose2d, wpi::math::Translation2d,
                 wpi::math::Polygon2d, wpi::math::Rotation2d,
                 wpi::math::Transform2d, wpi::math::Twist2d,
                 wpi::math::Rectangle2d, wpi::math::Ellipse2d>;

class Variable {
 public:
  std::string name;
  std::string namespacePath;
  std::string expression;
  VariableType type = VariableType::Quantity;
  std::optional<std::map<std::string, int>> dimension;
  VariableValue value;
  std::map<std::string, bool> allowDrag;
};

void to_json(wpi::util::json& json, const Variable& variable);
void from_json(const wpi::util::json& json, Variable& variable);

}
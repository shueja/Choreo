#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <wpi/math/geometry/Pose2d.hpp>
#include <wpi/math/geometry/Rotation2d.hpp>
#include <wpi/math/geometry/Transform2d.hpp>
#include <wpi/math/geometry/Translation2d.hpp>
#include <wpi/math/geometry/Twist2d.hpp>
#include <wpi/math/shape/Ellipse2d.hpp>
#include <wpi/math/shape/Rectangle2d.hpp>

#include "wpilib-expression/quantity.hpp"

namespace wpilib::expression {

enum class VariableType {
  Quantity,
  Pose2d,
  Translation2d,
  Rotation2d,
  Transform2d,
  Twist2d,
  Rectangle2d,
  Ellipse2d
};

using VariableValue =
    std::variant<units::precise_measurement, wpi::math::Pose2d,
                 wpi::math::Translation2d, wpi::math::Rotation2d,
                 wpi::math::Transform2d, wpi::math::Twist2d,
                 wpi::math::Rectangle2d, wpi::math::Ellipse2d>;

using ExpressionValue =
    std::variant<units::precise_measurement, bool, wpi::math::Pose2d,
                 wpi::math::Translation2d, wpi::math::Rotation2d,
                 wpi::math::Transform2d, wpi::math::Twist2d,
                 wpi::math::Rectangle2d, wpi::math::Ellipse2d>;

struct VariableDefinition {
  std::string id;
  std::string namespacePath;
  std::string name;
  std::string expression;
};

struct VariableInfo {
  VariableDefinition definition;
  VariableType type;
  std::optional<units::precise_unit> dimension;
  VariableValue value;
};

struct ExpressionInfo {
  VariableDefinition definition;
  ExpressionValue value;
};

class EvaluationContext {
 public:
  EvaluationContext();
  EvaluationContext(const EvaluationContext&) = default;
  EvaluationContext& operator=(const EvaluationContext&) = default;
  EvaluationContext(EvaluationContext&& other) noexcept;
  EvaluationContext& operator=(EvaluationContext&& other) noexcept;
  void SetVariables(std::vector<VariableDefinition> variables);
  std::vector<VariableInfo> GetVariables() const;
  std::vector<std::string> FindReferences(std::string_view variableId) const;
  void RenameVariable(std::string_view variableId, std::string newName);
  void SetExpressions(std::vector<VariableDefinition> expressions);
  void SetExpression(std::string_view expressionId, std::string expression);
  std::vector<ExpressionInfo> GetExpressions() const;
  void SetVariable(std::string name, units::precise_measurement value);
  void SetVariable(std::string name, wpi::math::Pose2d value);
  void SetVariable(std::string name, wpi::math::Translation2d value);
  void SetVariable(std::string name, wpi::math::Rotation2d value);
  void SetVariable(std::string name, wpi::math::Transform2d value);
  void SetVariable(std::string name, wpi::math::Twist2d value);
  void SetVariable(std::string name, wpi::math::Rectangle2d value);
  void SetVariable(std::string name, wpi::math::Ellipse2d value);
  units::precise_measurement Evaluate(
      std::string_view source,
      std::optional<units::precise_unit> expectedDimension =
          std::nullopt) const;
  bool EvaluateBoolean(std::string_view source) const;
  wpi::math::Pose2d EvaluatePose2d(std::string_view source) const;
  wpi::math::Translation2d EvaluateTranslation2d(std::string_view source) const;
  wpi::math::Rotation2d EvaluateRotation2d(std::string_view source) const;
  wpi::math::Transform2d EvaluateTransform2d(std::string_view source) const;
  wpi::math::Twist2d EvaluateTwist2d(std::string_view source) const;
  wpi::math::Rectangle2d EvaluateRectangle2d(std::string_view source) const;
  wpi::math::Ellipse2d EvaluateEllipse2d(std::string_view source) const;

 private:
  struct Impl;
  struct EngineCache;
  void SetVariableValue(std::string name, VariableValue value);
  std::shared_ptr<Impl> impl_;
  std::shared_ptr<EngineCache> engineCache_;
};

}  // namespace wpilib::expression
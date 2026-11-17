#pragma once

#include "wpilib-expression/quantity.hpp"

#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <wpi/math/geometry/Pose2d.hpp>
#include <wpi/math/geometry/Polygon2d.hpp>
#include <wpi/math/geometry/Rotation2d.hpp>
#include <wpi/math/geometry/Transform2d.hpp>
#include <wpi/math/geometry/Translation2d.hpp>
#include <wpi/math/geometry/Twist2d.hpp>
#include <wpi/math/shape/Ellipse2d.hpp>
#include <wpi/math/shape/Rectangle2d.hpp>

namespace wpilib::expression {

struct SetupVariable {
  std::string id;
  std::string name;
  std::string initializer;
};

std::string RenameVariableReferences(std::string_view source, std::string_view oldName,
                                     std::string_view newName);
std::vector<SetupVariable> RenameSetupVariable(std::span<const SetupVariable> variables,
                                               std::string_view id, std::string_view newName);

class SetupVariableError : public std::invalid_argument {
 public:
  SetupVariableError(std::string id, std::string message)
      : std::invalid_argument("setup variable '" + id + "': " + message), id_(std::move(id)) {}
  const std::string& Id() const noexcept { return id_; }

 private:
  std::string id_;
};

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
    std::variant<units::precise_measurement, wpi::math::Pose2d, wpi::math::Translation2d,
                 wpi::math::Polygon2d, wpi::math::Rotation2d,
                 wpi::math::Transform2d, wpi::math::Twist2d, wpi::math::Rectangle2d,
                 wpi::math::Ellipse2d>;

struct VariableDefinition {
  std::string id;
  std::string namespacePath;
  std::string name;
  std::string expression;
  std::optional<VariableType> expectedType;
  std::optional<units::precise_unit> expectedDimension;
};

struct VariableInfo {
  VariableDefinition definition;
  VariableType type;
  std::optional<units::precise_unit> dimension;
  VariableValue value;
};

class EvaluationContext {
 public:
  EvaluationContext();
  EvaluationContext(const EvaluationContext&) = default;
  EvaluationContext& operator=(const EvaluationContext&) = default;
  EvaluationContext(EvaluationContext&& other) noexcept;
  EvaluationContext& operator=(EvaluationContext&& other) noexcept;
  void SetSetupScript(std::string_view source);
  void SetSetupVariables(std::span<const SetupVariable> variables);
  void SetVariables(std::vector<VariableDefinition> variables);
  std::vector<VariableInfo> GetVariables() const;
  std::vector<std::string> FindReferences(std::string_view variableId) const;
  void RenameVariable(std::string_view variableId, std::string newName);
  void SetVariable(std::string name, units::precise_measurement value);
  void SetVariable(std::string name, wpi::math::Pose2d value);
  void SetVariable(std::string name, wpi::math::Translation2d value);
  void SetVariable(std::string name, wpi::math::Polygon2d value);
  void SetVariable(std::string name, wpi::math::Rotation2d value);
  void SetVariable(std::string name, wpi::math::Transform2d value);
  void SetVariable(std::string name, wpi::math::Twist2d value);
  void SetVariable(std::string name, wpi::math::Rectangle2d value);
  void SetVariable(std::string name, wpi::math::Ellipse2d value);
  units::precise_measurement Evaluate(
      std::string_view source,
      std::optional<units::precise_unit> expectedDimension = std::nullopt) const;
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
  Impl& MutableImpl();
  void SetSetupScriptImpl(std::string_view source, std::span<const SetupVariable> ordered);
  std::shared_ptr<Impl> impl_;
  std::shared_ptr<EngineCache> engineCache_;
};

}  // namespace wpilib::expression
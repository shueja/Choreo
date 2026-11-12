#pragma once

#include "wpilib-expression/quantity.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <wpi/math/geometry/Pose2d.hpp>
#include <wpi/math/geometry/Rotation2d.hpp>
#include <wpi/math/geometry/Transform2d.hpp>
#include <wpi/math/geometry/Translation2d.hpp>
#include <wpi/math/geometry/Twist2d.hpp>
#include <wpi/math/shape/Ellipse2d.hpp>
#include <wpi/math/shape/Rectangle2d.hpp>

namespace wpilib::expression {

class EvaluationContext {
 public:
  EvaluationContext();
  EvaluationContext(const EvaluationContext&) = default;
  EvaluationContext& operator=(const EvaluationContext&) = default;
  EvaluationContext(EvaluationContext&& other) noexcept;
  EvaluationContext& operator=(EvaluationContext&& other) noexcept;
  void SetSetupScript(std::string_view source);
  void SetVariable(std::string name, Quantity value);
  void SetVariable(std::string name, wpi::math::Pose2d value);
  void SetVariable(std::string name, wpi::math::Translation2d value);
  void SetVariable(std::string name, wpi::math::Rotation2d value);
  void SetVariable(std::string name, wpi::math::Transform2d value);
  void SetVariable(std::string name, wpi::math::Twist2d value);
  void SetVariable(std::string name, wpi::math::Rectangle2d value);
  void SetVariable(std::string name, wpi::math::Ellipse2d value);
  Quantity Evaluate(
      std::string_view source,
      std::optional<Dimension> expectedDimension = std::nullopt) const;
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
  std::shared_ptr<Impl> impl_;
  std::shared_ptr<EngineCache> engineCache_;
};

}  // namespace wpilib::expression
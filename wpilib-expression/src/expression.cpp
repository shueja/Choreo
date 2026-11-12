#include "wpilib-expression/expression.hpp"

#include "expression_engine.hpp"
#include "expression_syntax.hpp"
#include "units.hpp"

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace wpilib::expression {
namespace {

using detail::Pose2d;
using detail::Translation2d;
using detail::Rotation2d;
using detail::Transform2d;
using detail::Twist2d;

template <typename Result>
Result EvaluateResult(const detail::Variables& variables, std::string_view source) {
  chaiscript::ChaiScript engine;
  detail::ConfigureEngine(engine, variables);
  auto result = engine.eval<Result>(detail::ExpandUnitSuffixes(source));
  if constexpr (std::is_same_v<Result, Quantity>) {
    detail::RequireFinite(result);
  }
  return result;
}

}  // namespace

struct EvaluationContext::Impl {
  detail::Variables variables;
};

void EvaluationContext::SetVariable(std::string name, Quantity value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

void EvaluationContext::SetVariable(std::string name, Pose2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

void EvaluationContext::SetVariable(std::string name, Translation2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

void EvaluationContext::SetVariable(std::string name, Rotation2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

void EvaluationContext::SetVariable(std::string name, Transform2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

void EvaluationContext::SetVariable(std::string name, Twist2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

Quantity EvaluationContext::Evaluate(
    std::string_view source, std::optional<Dimension> expectedDimension) const {
  static const detail::Variables emptyVariables;
  auto result = EvaluateResult<Quantity>(impl_ ? impl_->variables : emptyVariables, source);
  if (expectedDimension && result.dimension != *expectedDimension) {
    throw std::invalid_argument("expression result has unexpected dimension");
  }
  return result;
}

bool EvaluationContext::EvaluateBoolean(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<bool>(impl_ ? impl_->variables : emptyVariables, source);
}

Pose2d EvaluationContext::EvaluatePose2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Pose2d>(impl_ ? impl_->variables : emptyVariables, source);
}

Translation2d EvaluationContext::EvaluateTranslation2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Translation2d>(impl_ ? impl_->variables : emptyVariables, source);
}

Rotation2d EvaluationContext::EvaluateRotation2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Rotation2d>(impl_ ? impl_->variables : emptyVariables, source);
}

Transform2d EvaluationContext::EvaluateTransform2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Transform2d>(impl_ ? impl_->variables : emptyVariables, source);
}

Twist2d EvaluationContext::EvaluateTwist2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Twist2d>(impl_ ? impl_->variables : emptyVariables, source);
}

}  // namespace wpilib::expression
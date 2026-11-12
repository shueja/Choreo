#include "wpilib-expression/expression.hpp"

#include "expression_engine.hpp"
#include "expression_syntax.hpp"
#include "units.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
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
using detail::Rectangle2d;
using detail::Ellipse2d;

struct CachedEngine {
  chaiscript::ChaiScript engine{{}, {},
                               {chaiscript::Options::No_Load_Modules,
                                chaiscript::Options::No_External_Scripts},
                               {chaiscript::Library_Options::No_IO}};
  chaiscript::ChaiScript::State initialState;
  std::map<std::string, chaiscript::Boxed_Value> initialLocals;

  CachedEngine() {
    detail::ConfigureEngine(engine, {});
    initialState = engine.get_state();
    initialLocals = engine.get_locals();
  }
};

void ValidateExpression(const chaiscript::AST_Node& node,
                        const detail::Variables& variables) {
  static const std::set<std::string_view> safeFunctions{
      "quantity", "sin", "cos", "tan", "asin", "acos", "atan", "Pose2d",
      "Translation2d", "Rotation2d", "Transform2d", "Twist2d", "Rectangle2d", "Ellipse2d",
      "X", "Y",
      "Rotation", "Translation", "RotateBy", "RelativeTo", "TransformBy",
      "Distance", "Norm", "Radians", "Degrees", "Cos", "Sin", "Tan", "Inverse", "Log",
      "dx", "dy", "dtheta", "Exp", "Center", "XWidth", "YWidth", "XSemiAxis", "YSemiAxis",
      "FocalPoints", "Intersects", "Contains", "Nearest", "RotateAround", "SquaredDistance",
      "SquaredNorm", "Dot", "Cross", "Angle"};
  static const std::set<std::string_view> safeOperators{
      "+", "-", "*", "/", "^", "==", "!=", "<", "<=", ">", ">=", "!"};
  using Type = chaiscript::AST_Node_Type;
  auto children = node.get_children();
  switch (node.identifier) {
    case Type::File:
      if (children.size() != 1) {
        throw std::invalid_argument("expected a single read-only expression");
      }
      break;
    case Type::Id:
      if (!variables.contains(node.text) && !detail::UnitRegistry().contains(node.text) &&
          !safeFunctions.contains(node.text) && node.text != "true" && node.text != "false") {
        throw std::invalid_argument("unknown expression identifier: " + node.text);
      }
      break;
    case Type::Fun_Call:
      if (children.empty() || children.front().get().identifier != Type::Id ||
          !safeFunctions.contains(children.front().get().text)) {
        throw std::invalid_argument("function is not permitted in an expression");
      }
      break;
    case Type::Binary:
    case Type::Prefix:
      if (!safeOperators.contains(node.text)) {
        throw std::invalid_argument("operator is not permitted in an expression");
      }
      break;
    case Type::Constant:
    case Type::Arg_List:
    case Type::Dot_Access:
    case Type::Array_Call:
    case Type::Inline_Array:
    case Type::Logical_And:
    case Type::Logical_Or:
      break;
    default:
      throw std::invalid_argument("only read-only expressions are permitted");
  }
  for (const auto& child : children) {
    ValidateExpression(child.get(), variables);
  }
}

void ValidateSetup(const chaiscript::AST_Node& root,
                   const std::map<std::string, chaiscript::Boxed_Value>& initialObjects) {
  using Type = chaiscript::AST_Node_Type;
  if (root.identifier != Type::File) {
    throw std::invalid_argument("expected setup variable declarations");
  }
  detail::Variables variables;
  for (const auto& statement : root.get_children()) {
    const auto& node = statement.get();
    auto children = node.get_children();
    if (children.size() != 2) {
      throw std::invalid_argument("setup permits only initialized variable declarations");
    }
    const chaiscript::AST_Node* name = &children[0].get();
    if (node.identifier == Type::Equation && node.text == "=" &&
        name->identifier == Type::Global_Decl) {
      auto globalChildren = name->get_children();
      if (globalChildren.size() != 1) {
        throw std::invalid_argument("invalid global variable declaration");
      }
      name = &globalChildren[0].get();
    } else if (node.identifier != Type::Assign_Decl &&
               node.identifier != Type::Const_Assign_Decl) {
      throw std::invalid_argument("setup permits only initialized variable declarations");
    }
    if (name->identifier != Type::Id || variables.contains(name->text) ||
        initialObjects.contains(name->text)) {
      throw std::invalid_argument("invalid or duplicate setup variable: " + name->text);
    }
    ValidateExpression(children[1].get(), variables);
    detail::StoreVariable(variables, name->text, Quantity{});
  }
}

template <typename Result, typename Cache>
Result EvaluateResult(Cache& engineCache, const detail::Variables& variables,
                      std::string_view source) {
  std::lock_guard lock(engineCache.mutex);
  if (!engineCache.cache) {
    engineCache.cache.emplace();
  }
  auto& cache = *engineCache.cache;
  auto expanded = detail::ExpandUnitSuffixes(source);
  auto parsed = cache.engine.parse(expanded);
  ValidateExpression(*parsed, variables);
  cache.engine.set_state(cache.initialState);
  cache.engine.set_locals(cache.initialLocals);
  detail::ConfigureGlobals(cache.engine, variables);
  auto result = cache.engine.template eval<Result>(expanded);
  if constexpr (std::is_same_v<Result, Quantity>) {
    detail::RequireFinite(result);
  }
  return result;
}

}  // namespace

struct EvaluationContext::Impl {
  detail::Variables variables;
};

struct EvaluationContext::EngineCache {
  std::mutex mutex;
  std::optional<CachedEngine> cache;
};

EvaluationContext::EvaluationContext() : engineCache_(std::make_shared<EngineCache>()) {}

EvaluationContext::EvaluationContext(EvaluationContext&& other) noexcept
    : impl_(std::move(other.impl_)), engineCache_(other.engineCache_) {}

EvaluationContext& EvaluationContext::operator=(EvaluationContext&& other) noexcept {
  if (this != &other) {
    impl_ = std::move(other.impl_);
    engineCache_ = other.engineCache_;
  }
  return *this;
}

void EvaluationContext::SetSetupScript(std::string_view source) {
  chaiscript::ChaiScript engine{{}, {},
                               {chaiscript::Options::No_Load_Modules,
                                chaiscript::Options::No_External_Scripts},
                               {chaiscript::Library_Options::No_IO}};
  detail::ConfigureEngine(engine, {});
  auto initialObjects = engine.get_scripting_objects();
  auto initialLocals = engine.get_locals();
  auto expanded = detail::ExpandUnitSuffixes(source);
  auto parsed = engine.parse(expanded);
  ValidateSetup(*parsed, initialObjects);
  engine.eval(expanded);

  auto replacement = std::make_shared<Impl>();
  auto store = [&](const std::string& name, const chaiscript::Boxed_Value& value) {
    auto type = value.get_type_info();
    if (type.bare_equal(chaiscript::user_type<Quantity>())) {
      auto quantity = chaiscript::boxed_cast<Quantity>(value);
      detail::RequireFinite(quantity);
      detail::StoreVariable(replacement->variables, name, quantity);
    } else if (type.bare_equal(chaiscript::user_type<Pose2d>())) {
      detail::StoreVariable(replacement->variables, name, chaiscript::boxed_cast<Pose2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Translation2d>())) {
      detail::StoreVariable(replacement->variables, name, chaiscript::boxed_cast<Translation2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Rotation2d>())) {
      detail::StoreVariable(replacement->variables, name, chaiscript::boxed_cast<Rotation2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Transform2d>())) {
      detail::StoreVariable(replacement->variables, name, chaiscript::boxed_cast<Transform2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Twist2d>())) {
      detail::StoreVariable(replacement->variables, name, chaiscript::boxed_cast<Twist2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Rectangle2d>())) {
      detail::StoreVariable(replacement->variables, name, chaiscript::boxed_cast<Rectangle2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Ellipse2d>())) {
      detail::StoreVariable(replacement->variables, name, chaiscript::boxed_cast<Ellipse2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<double>())) {
      auto quantity = Quantity{chaiscript::boxed_cast<double>(value), {}};
      detail::RequireFinite(quantity);
      detail::StoreVariable(replacement->variables, name, quantity);
    } else if (type.bare_equal(chaiscript::user_type<int>())) {
      detail::StoreVariable(replacement->variables, name,
                            Quantity{static_cast<double>(chaiscript::boxed_cast<int>(value)), {}});
    } else if (type.bare_equal(chaiscript::user_type<std::int64_t>())) {
      detail::StoreVariable(replacement->variables, name,
                            Quantity{static_cast<double>(chaiscript::boxed_cast<std::int64_t>(value)), {}});
    } else {
      Quantity quantity;
      try {
        quantity = engine.boxed_cast<Quantity>(value);
      } catch (const chaiscript::exception::bad_boxed_cast&) {
        throw std::invalid_argument("unsupported setup variable type: " + name);
      }
      detail::RequireFinite(quantity);
      detail::StoreVariable(replacement->variables, name, quantity);
    }
  };
  for (const auto& [name, value] : engine.get_locals()) {
    if (!initialLocals.contains(name)) {
      store(name, value);
    }
  }
  for (const auto& [name, value] : engine.get_scripting_objects()) {
    if (!initialObjects.contains(name) && !replacement->variables.contains(name)) {
      store(name, value);
    }
  }
  impl_ = std::move(replacement);
}

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

void EvaluationContext::SetVariable(std::string name, Rectangle2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

void EvaluationContext::SetVariable(std::string name, Ellipse2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value));
}

Quantity EvaluationContext::Evaluate(
    std::string_view source, std::optional<Dimension> expectedDimension) const {
  static const detail::Variables emptyVariables;
  auto result = EvaluateResult<Quantity>(*engineCache_,
                                         impl_ ? impl_->variables : emptyVariables, source);
  if (expectedDimension && result.dimension != *expectedDimension) {
    throw std::invalid_argument("expression result has unexpected dimension");
  }
  return result;
}

bool EvaluationContext::EvaluateBoolean(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<bool>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Pose2d EvaluationContext::EvaluatePose2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Pose2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Translation2d EvaluationContext::EvaluateTranslation2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Translation2d>(*engineCache_,
                                       impl_ ? impl_->variables : emptyVariables, source);
}

Rotation2d EvaluationContext::EvaluateRotation2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Rotation2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Transform2d EvaluationContext::EvaluateTransform2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Transform2d>(*engineCache_,
                                     impl_ ? impl_->variables : emptyVariables, source);
}

Twist2d EvaluationContext::EvaluateTwist2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Twist2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Rectangle2d EvaluationContext::EvaluateRectangle2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Rectangle2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Ellipse2d EvaluationContext::EvaluateEllipse2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Ellipse2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

}  // namespace wpilib::expression
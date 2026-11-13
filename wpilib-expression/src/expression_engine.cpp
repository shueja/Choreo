#include "expression_engine.hpp"

#include "geometry_bindings.hpp"
#include "scalar_bindings.hpp"
#include "units.hpp"

#include <cctype>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace wpilib::expression::detail {
namespace {

void ValidateVariableName(std::string_view name) {
  if (name.empty() ||
      !(std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_')) {
    throw std::invalid_argument("variable name must be a non-empty identifier");
  }
  for (char character : name) {
    if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '_')) {
      throw std::invalid_argument("variable name must be a non-empty identifier");
    }
  }
  static const std::set<std::string> reservedNames{
      "quantity", "Pose2d", "Translation2d", "Rotation2d", "Transform2d", "Twist2d",
      "Rectangle2d", "Ellipse2d", "EllipseFocalPoints",
      "sin", "cos", "tan", "asin", "acos", "atan", "X", "Y", "Rotation",
      "Translation", "RotateBy", "RelativeTo", "TransformBy", "Distance", "Norm",
      "Radians", "Degrees", "Cos", "Sin", "Tan", "Inverse", "Log", "dx", "dy", "dtheta", "Exp",
      "Center", "XWidth", "YWidth", "XSemiAxis", "YSemiAxis", "FocalPoints", "Intersects",
      "Contains", "Nearest", "RotateAround", "SquaredDistance", "SquaredNorm", "Dot", "Cross", "Angle"};
  if (UnitRegistry().contains(std::string(name)) || reservedNames.contains(std::string(name))) {
    throw std::invalid_argument("variable name is reserved by the expression language: " +
                                std::string(name));
  }
}

}  // namespace

DeclaredType GetDeclaredType(const VariableValue& value) {
  return std::visit([](const auto& variable) -> DeclaredType {
    using Value = std::decay_t<decltype(variable)>;
    if constexpr (std::is_same_v<Value, units::precise_measurement>) {
      return {VariableType::Quantity, variable.units()};
    } else if constexpr (std::is_same_v<Value, Pose2d>) {
      return {VariableType::Pose2d, units::precise::one};
    } else if constexpr (std::is_same_v<Value, Translation2d>) {
      return {VariableType::Translation2d, units::precise::one};
    } else if constexpr (std::is_same_v<Value, Rotation2d>) {
      return {VariableType::Rotation2d, units::precise::one};
    } else if constexpr (std::is_same_v<Value, Transform2d>) {
      return {VariableType::Transform2d, units::precise::one};
    } else if constexpr (std::is_same_v<Value, Twist2d>) {
      return {VariableType::Twist2d, units::precise::one};
    } else if constexpr (std::is_same_v<Value, Rectangle2d>) {
      return {VariableType::Rectangle2d, units::precise::one};
    } else if constexpr (std::is_same_v<Value, Ellipse2d>) {
      return {VariableType::Ellipse2d, units::precise::one};
    } else {
      return {VariableType::Namespace, units::precise::one};
    }
  }, value);
}

bool IsCompatible(const DeclaredType& expected, const DeclaredType& actual) {
  return expected.type == actual.type &&
         (expected.type != VariableType::Quantity ||
          expected.dimension.base_units() == actual.dimension.base_units());
}

chaiscript::Boxed_Value BoxImmutable(const VariableRecord& record) {
  const auto& value = record.value;
  return std::visit([](const auto& variable) -> chaiscript::Boxed_Value {
    using Value = std::decay_t<decltype(variable)>;
    if constexpr (std::is_same_v<Value, NamespacePtr>) {
      chaiscript::Namespace object;
      object.set_explicit(true);
      for (const auto& [name, member] : variable->members) {
        object[name] = BoxImmutable(member);
      }
      return chaiscript::const_var(object);
    } else {
      return chaiscript::const_var(variable);
    }
  }, value);
}

void StoreVariable(Variables& variables, std::string name, VariableValue value,
                   const VariableRecord* existing) {
  ValidateVariableName(name);
  auto declaredType = existing
                          ? existing->declaredType
                          : std::make_shared<const DeclaredType>(GetDeclaredType(value));
  if (!IsCompatible(*declaredType, GetDeclaredType(value))) {
    throw std::invalid_argument("variable value does not match its declared type: " + name);
  }
  variables.insert_or_assign(std::move(name),
                             VariableRecord{std::move(value), std::move(declaredType)});
}

void ConfigureEngine(chaiscript::ChaiScript& engine, const Variables& variables) {
  ConfigureScalarBindings(engine);
  ConfigureGeometryBindings(engine);
  ConfigureGlobals(engine, variables);
}

void ConfigureGlobals(chaiscript::ChaiScript& engine, const Variables& variables) {
  for (const auto& [name, unit] : UnitRegistry()) {
    engine.set_global(chaiscript::var(units::precise_measurement{1.0, unit}),
                     name);
  }
  for (const auto& [name, record] : variables) {
    engine.set_global(BoxImmutable(record), name);
  }
}

}  // namespace wpilib::expression::detail
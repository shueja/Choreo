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

chaiscript::Boxed_Value BoxImmutable(const VariableValue& value) {
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

void StoreVariable(Variables& variables, std::string name, VariableValue value) {
  ValidateVariableName(name);
  variables.insert_or_assign(std::move(name), std::move(value));
}

void ConfigureEngine(chaiscript::ChaiScript& engine, const Variables& variables) {
  ConfigureScalarBindings(engine);
  ConfigureGeometryBindings(engine);
  ConfigureGlobals(engine, variables);
}

void ConfigureGlobals(chaiscript::ChaiScript& engine, const Variables& variables) {
  for (const auto& [name, unit] : UnitRegistry()) {
    engine.set_global(chaiscript::var(Quantity{unit.scale, unit.dimension}), name);
  }
  for (const auto& [name, value] : variables) {
    engine.set_global(BoxImmutable(value), name);
  }
}

}  // namespace wpilib::expression::detail
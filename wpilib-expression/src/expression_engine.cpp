#include "expression_engine.hpp"

#include "geometry_bindings.hpp"
#include "scalar_bindings.hpp"
#include "units.hpp"

#include <cctype>
#include <set>
#include <stdexcept>
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
      "sin", "cos", "tan", "asin", "acos", "atan", "X", "Y", "Rotation",
      "Translation", "RotateBy", "RelativeTo", "TransformBy", "Distance", "Norm",
      "Radians", "Degrees", "Cos", "Sin", "Tan", "Inverse", "Log", "dx", "dy", "dtheta", "Exp"};
  if (UnitRegistry().contains(std::string(name)) || reservedNames.contains(std::string(name))) {
    throw std::invalid_argument("variable name is reserved by the expression language: " +
                                std::string(name));
  }
}

}  // namespace

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
    std::visit([&](const auto& variable) {
      engine.set_global(chaiscript::var(variable), name);
    }, value);
  }
}

}  // namespace wpilib::expression::detail
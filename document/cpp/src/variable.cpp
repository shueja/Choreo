#include "choreo/document/variable.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "choreo/document/expression.hpp"
#include "choreo/document/json_validation.hpp"

namespace choreo::document {
namespace {

using wpi::util::json;

constexpr std::array<std::string_view, 9> kVariableTypes{
    "Quantity",    "Pose2d",  "Translation2d", "Polygon2d", "Rotation2d",
    "Transform2d", "Twist2d", "Rectangle2d",   "Ellipse2d"};

constexpr std::array<std::string_view, 10> kDimensions{
    "meter", "kilogram", "second",   "ampere", "kelvin",
    "mole",  "candela",  "currency", "count",  "radian"};

bool IsIdentifier(std::string_view name) {
  auto isLetter = [](char character) {
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') || character == '_';
  };
  if (name.empty() || !isLetter(name.front())) {
    return false;
  }
  for (char character : name.substr(1)) {
    if (!isLetter(character) && !(character >= '0' && character <= '9')) {
      return false;
    }
  }
  return true;
}

void CheckVariableNames(const Variable& variable) {
  if (!IsIdentifier(variable.name) || variable.expression.empty()) {
    throw std::invalid_argument("invalid variable name or expression");
  }
  for (std::size_t start = 0; start < variable.namespacePath.size();) {
    const auto end = variable.namespacePath.find('.', start);
    if (!IsIdentifier(std::string_view{variable.namespacePath}.substr(
            start, end == std::string::npos ? end : end - start))) {
      throw std::invalid_argument("invalid variable namespace");
    }
    if (end == std::string::npos) {
      return;
    }
    start = end + 1;
    if (start == variable.namespacePath.size()) {
      throw std::invalid_argument("invalid variable namespace");
    }
  }
}

VariableType ReadVariableType(const std::string& name) {
  for (std::size_t index = 0; index < kVariableTypes.size(); ++index) {
    if (kVariableTypes[index] == name) {
      return static_cast<VariableType>(index);
    }
  }
  throw std::invalid_argument("unsupported variable type: " + name);
}

bool IsKnownDimension(std::string_view name) {
  for (auto dimension : kDimensions) {
    if (dimension == name) {
      return true;
    }
  }
  return false;
}

std::map<std::string, int> ReadDimension(const json& value) {
  std::map<std::string, int> dimension;
  for (const auto& [name, exponent] : value.get_object()) {
    if (!IsKnownDimension(name) || !exponent.is_int() ||
        exponent.get_int() < std::numeric_limits<int>::min() ||
        exponent.get_int() > std::numeric_limits<int>::max()) {
      throw std::invalid_argument("invalid scalar dimension exponent: " + name);
    }
    dimension.emplace(name, static_cast<int>(exponent.get_int()));
  }
  return dimension;
}

VariableValue ReadVariableValue(const json& value, VariableType type) {
  switch (type) {
    case VariableType::Quantity: {
      const auto number = value.get_number();
      if (!std::isfinite(number)) {
        throw std::invalid_argument("variable value must be finite");
      }
      return number;
    }
    case VariableType::Pose2d:
      detail::CheckPose(value);
      return value.get<wpi::math::Pose2d>();
    case VariableType::Translation2d:
      detail::CheckTranslation(value);
      return value.get<wpi::math::Translation2d>();
    case VariableType::Polygon2d:
      detail::CheckPolygon(value);
      return value.get<wpi::math::Polygon2d>();
    case VariableType::Rotation2d:
      detail::CheckRotation(value);
      return value.get<wpi::math::Rotation2d>();
    case VariableType::Transform2d:
      detail::CheckProperties(value, {"translation", "rotation"});
      detail::CheckTranslation(value.at("translation"));
      detail::CheckRotation(value.at("rotation"));
      return wpi::math::Transform2d{
          value.at("translation").get<wpi::math::Translation2d>(),
          value.at("rotation").get<wpi::math::Rotation2d>()};
    case VariableType::Twist2d:
      detail::CheckProperties(value, {"dx", "dy", "dtheta"});
      detail::CheckNumber(value.at("dx"));
      detail::CheckNumber(value.at("dy"));
      detail::CheckNumber(value.at("dtheta"));
      return wpi::math::Twist2d{
          wpi::units::meter_t{value.at("dx").get_number()},
          wpi::units::meter_t{value.at("dy").get_number()},
          wpi::units::radian_t{value.at("dtheta").get_number()}};
    case VariableType::Rectangle2d:
      detail::CheckRectangle(value);
      return detail::ReadRectangle(value);
    case VariableType::Ellipse2d:
      detail::CheckEllipse(value);
      return detail::ReadEllipse(value);
  }
  throw std::invalid_argument("unsupported variable type");
}

}

void to_json(json& serialized, const Variable& variable) {
  CheckVariableNames(variable);
  const auto index = static_cast<std::size_t>(variable.type);
  if (index >= kVariableTypes.size() || variable.value.index() != index ||
      (variable.type == VariableType::Quantity) !=
          variable.dimension.has_value()) {
    throw std::invalid_argument(
        "variable type, value, and dimension do not match");
  }
  if (variable.type == VariableType::Quantity &&
      !std::isfinite(std::get<double>(variable.value))) {
    throw std::invalid_argument("variable value must be finite");
  }
  serialized = json::object("name", variable.name, "namespacePath",
                            variable.namespacePath, "expression",
                            variable.expression, "type", kVariableTypes[index]);
  serialized["value"] = std::visit(
      [](const auto& value) -> json {
        using Value = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, wpi::math::Transform2d>) {
          return json::object("translation", value.Translation(), "rotation",
                              value.Rotation());
        } else if constexpr (std::is_same_v<Value, wpi::math::Twist2d>) {
          return json::object("dx", value.dx.value(), "dy", value.dy.value(),
                              "dtheta", value.dtheta.value());
        } else if constexpr (std::is_same_v<Value, wpi::math::Rectangle2d>) {
          return detail::WriteRectangle(value);
        } else if constexpr (std::is_same_v<Value, wpi::math::Ellipse2d>) {
          return detail::WriteEllipse(value);
        } else {
          return json{value};
        }
      },
      variable.value);
  ReadVariableValue(serialized.at("value"), variable.type);
  if (variable.dimension) {
    json dimension = json::object();
    for (const auto& [name, exponent] : *variable.dimension) {
      if (!IsKnownDimension(name)) {
        throw std::invalid_argument("invalid scalar dimension: " + name);
      }
      dimension[name] = exponent;
    }
    serialized["dimension"] = std::move(dimension);
  }
}

void from_json(const json& serialized, Variable& variable) {
  detail::CheckProperties(serialized, {"name", "namespacePath", "expression",
                                       "type", "dimension", "value"});
  Variable result;
  result.name = serialized.at("name").get_string();
  result.namespacePath = serialized.at("namespacePath").get_string();
  result.expression = serialized.at("expression").get_string();
  CheckVariableNames(result);
  result.type = ReadVariableType(serialized.at("type").get_string());
  if (result.type == VariableType::Quantity) {
    result.dimension = ReadDimension(serialized.at("dimension"));
  } else if (serialized.contains("dimension")) {
    throw std::invalid_argument(
        "geometry variables cannot have scalar dimensions");
  }
  result.value = ReadVariableValue(serialized.at("value"), result.type);
  variable = std::move(result);
}

}
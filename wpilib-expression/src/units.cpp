#include "units.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>

namespace wpilib::expression::detail {

Dimension MakeDimension(int length, int mass, int time, int current, int angle) {
  return Dimension{{length, mass, time, current, angle}};
}

Dimension Combine(const Dimension& left, const Dimension& right, int sign) {
  Dimension result;
  for (std::size_t index = 0; index < result.exponents.size(); ++index) {
    result.exponents[index] = left.exponents[index] + sign * right.exponents[index];
  }
  return result;
}

const std::map<std::string, Unit>& UnitRegistry() {
  static const Dimension length = MakeDimension(1, 0, 0, 0, 0);
  static const Dimension mass = MakeDimension(0, 1, 0, 0, 0);
  static const Dimension time = MakeDimension(0, 0, 1, 0, 0);
  static const Dimension current = MakeDimension(0, 0, 0, 1, 0);
  static const Dimension angle = MakeDimension(0, 0, 0, 0, 1);

  static const Dimension area = ScaleDimension(length, 2);
  static const Dimension frequency = ScaleDimension(time, -1);
  static const Dimension force = Combine(Combine(length, mass, 1), ScaleDimension(time, 2), -1);
  static const Dimension torqueAndEnergy = Combine(force, length, 1);
  static const Dimension power = Combine(torqueAndEnergy, time, -1);
  static const Dimension voltage = Combine(power, current, -1);
  static const Dimension charge = Combine(current, time, 1);
  static const Dimension pressure = Combine(force, area, -1);
  static const Dimension resistance = Combine(voltage, current, -1);
  static const Dimension angularSpeed = Combine(angle, time, -1);

  static const std::map<std::string, Unit> units{
      {"m", {1.0, length}},
      {"meter", {1.0, length}},
      {"mm", {1e-3, length}},
      {"cm", {1e-2, length}},
      {"km", {1e3, length}},
      {"in", {0.0254, length}},
      {"inch", {0.0254, length}},
      {"ft", {0.3048, length}},
      {"yd", {0.9144, length}},
      {"mi", {1609.344, length}},
      {"s", {1.0, time}},
      {"ms", {1e-3, time}},
      {"us", {1e-6, time}},
      {"min", {60.0, time}},
      {"hr", {3600.0, time}},
      {"kg", {1.0, mass}},
      {"g", {1e-3, mass}},
      {"lb", {0.45359237, mass}},
      {"A", {1.0, current}},
      {"mA", {1e-3, current}},
      {"rad", {1.0, angle}},
      {"radian", {1.0, angle}},
      {"deg", {std::acos(-1.0) / 180.0, angle}},
      {"degree", {std::acos(-1.0) / 180.0, angle}},
      {"turn", {2.0 * std::acos(-1.0), angle}},
      {"rev", {2.0 * std::acos(-1.0), angle}},
      {"N", {1.0, force}},
      {"lbf", {4.4482216152605, force}},
      {"V", {1.0, voltage}},
      {"mV", {1e-3, voltage}},
      {"kV", {1e3, voltage}},
      {"Hz", {1.0, frequency}},
      {"kHz", {1e3, frequency}},
      {"W", {1.0, power}},
      {"kW", {1e3, power}},
      {"J", {1.0, torqueAndEnergy}},
      {"kJ", {1e3, torqueAndEnergy}},
      {"C", {1.0, charge}},
      {"Pa", {1.0, pressure}},
      {"kPa", {1e3, pressure}},
      {"psi", {6894.757293168, pressure}},
      {"ohm", {1.0, resistance}},
      {"Ohm", {1.0, resistance}},
      {"Nm", {1.0, torqueAndEnergy}},
      {"RPM", {2.0 * std::acos(-1.0) / 60.0, angularSpeed}},
      {"rpm", {2.0 * std::acos(-1.0) / 60.0, angularSpeed}},
  };
  return units;
}

Dimension ScaleDimension(const Dimension& dimension, int exponent) {
  Dimension result;
  for (std::size_t index = 0; index < result.exponents.size(); ++index) {
    result.exponents[index] = dimension.exponents[index] * exponent;
  }
  return result;
}

Unit ParseUnit(std::string_view expression) {
  Unit result{1.0, Dimension{}};
  std::size_t position = 0;
  int sign = 1;
  bool expectUnit = true;

  while (position < expression.size()) {
    while (position < expression.size() &&
           std::isspace(static_cast<unsigned char>(expression[position]))) {
      ++position;
    }
    if (position == expression.size()) {
      break;
    }

    if (!expectUnit) {
      char operation = expression[position++];
      if (operation != '*' && operation != '/') {
        throw std::invalid_argument("malformed unit expression: " + std::string(expression));
      }
      sign = operation == '*' ? 1 : -1;
      expectUnit = true;
      continue;
    }

    std::size_t symbolStart = position;
    while (position < expression.size() &&
           (std::isalnum(static_cast<unsigned char>(expression[position])) ||
            expression[position] == '_')) {
      ++position;
    }
    if (position == symbolStart) {
      throw std::invalid_argument("malformed unit expression: " + std::string(expression));
    }

    auto symbol = expression.substr(symbolStart, position - symbolStart);
    auto found = UnitRegistry().find(std::string(symbol));
    if (found == UnitRegistry().end()) {
      throw std::invalid_argument("unknown unit: " + std::string(symbol));
    }

    int exponent = 1;
    if (position < expression.size() && expression[position] == '^') {
      ++position;
      int exponentSign = 1;
      if (position < expression.size() &&
          (expression[position] == '+' || expression[position] == '-')) {
        exponentSign = expression[position++] == '-' ? -1 : 1;
      }
      std::size_t exponentStart = position;
      exponent = 0;
      while (position < expression.size() &&
             std::isdigit(static_cast<unsigned char>(expression[position]))) {
        exponent = exponent * 10 + (expression[position++] - '0');
      }
      if (position == exponentStart || exponent > 16) {
        throw std::invalid_argument("unit exponents must be integers with magnitude at most 16");
      }
      exponent *= exponentSign;
    }

    exponent *= sign;
    result.scale *= std::pow(found->second.scale, exponent);
    result.dimension = Combine(result.dimension, ScaleDimension(found->second.dimension, exponent), 1);
    expectUnit = false;
  }

  if (expectUnit) {
    throw std::invalid_argument("malformed unit expression: " + std::string(expression));
  }
  return result;
}

void RequireFinite(const Quantity& quantity) {
  if (!std::isfinite(quantity.siValue)) {
    throw std::domain_error("expression result is not finite");
  }
}

}  // namespace wpilib::expression::detail
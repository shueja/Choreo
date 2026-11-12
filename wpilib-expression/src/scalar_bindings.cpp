#include "scalar_bindings.hpp"

#include "units.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace wpilib::expression::detail {
namespace {

units::precise_measurement Power(const units::precise_measurement& value, double exponent) {
  if (!std::isfinite(exponent) || std::trunc(exponent) != exponent ||
      std::abs(exponent) > 16.0) {
    throw std::invalid_argument("quantity powers must be integers with magnitude at most 16");
  }
  int integerExponent = static_cast<int>(exponent);
  return pow(value, integerExponent);
}

units::precise_measurement Add(const units::precise_measurement& left, const units::precise_measurement& right) {
  if (left.units().base_units() != right.units().base_units()) {
    throw std::invalid_argument("addition requires matching dimensions");
  }
  return left + right;
}

units::precise_measurement Subtract(const units::precise_measurement& left, const units::precise_measurement& right) {
  if (left.units().base_units() != right.units().base_units()) {
    throw std::invalid_argument("subtraction requires matching dimensions");
  }
  return left - right;
}

units::precise_measurement Multiply(const units::precise_measurement& left, const units::precise_measurement& right) {
  return left * right;
}

units::precise_measurement Divide(const units::precise_measurement& left, const units::precise_measurement& right) {
  if (SiValue(right) == 0.0) {
    throw std::domain_error("division by zero");
  }
  return left / right;
}

void RequireComparable(const units::precise_measurement& left, const units::precise_measurement& right) {
  if (left.units().base_units() != right.units().base_units()) {
    throw std::invalid_argument("comparison requires matching dimensions");
  }
}

bool Equal(const units::precise_measurement& left, const units::precise_measurement& right) {
  RequireComparable(left, right);
  return left == right;
}

bool Less(const units::precise_measurement& left, const units::precise_measurement& right) {
  RequireComparable(left, right);
  return left < right;
}

bool LessEqual(const units::precise_measurement& left, const units::precise_measurement& right) {
  RequireComparable(left, right);
  return left <= right;
}

bool Greater(const units::precise_measurement& left, const units::precise_measurement& right) {
  RequireComparable(left, right);
  return left > right;
}

bool GreaterEqual(const units::precise_measurement& left, const units::precise_measurement& right) {
  RequireComparable(left, right);
  return left >= right;
}

units::precise_measurement TrigInput(const units::precise_measurement& value, double (*function)(double)) {
  auto dimension = value.units().base_units();
  if (dimension != units::precise::rad.base_units() &&
      dimension != units::precise::one.base_units()) {
    throw std::invalid_argument(
        "trigonometric functions require an angle or dimensionless value");
  }
  return MakeMeasurement(function(SiValue(value)), units::precise::one);
}

units::precise_measurement InverseTrigInput(const units::precise_measurement& value, double (*function)(double)) {
  if (value.units().base_units() != units::precise::one.base_units()) {
    throw std::invalid_argument(
        "inverse trigonometric functions require a dimensionless value");
  }
  return MakeMeasurement(function(SiValue(value)), units::precise::rad);
}

}  // namespace

void ConfigureScalarBindings(chaiscript::ChaiScript& engine) {
  engine.add(chaiscript::user_type<units::precise_measurement>(), "precise_measurement");
  engine.add(chaiscript::fun([](double value, const std::string& unit) {
    return From(value, unit);
  }), "quantity");
  engine.add(chaiscript::fun(&Add), "+");
  engine.add(chaiscript::fun(&Subtract), "-");
  engine.add(chaiscript::fun(&Multiply), "*");
  engine.add(chaiscript::fun(&Divide), "/");
  engine.add(chaiscript::fun(&Power), "^");
  engine.add(chaiscript::fun(&Equal), "==");
  engine.add(chaiscript::fun([](const units::precise_measurement& left, const units::precise_measurement& right) {
    return !Equal(left, right);
  }), "!=");
  engine.add(chaiscript::fun(&Less), "<");
  engine.add(chaiscript::fun(&LessEqual), "<=");
  engine.add(chaiscript::fun(&Greater), ">");
  engine.add(chaiscript::fun(&GreaterEqual), ">=");
  engine.add(chaiscript::fun([](const units::precise_measurement& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::sin));
  }), "sin");
  engine.add(chaiscript::fun([](const units::precise_measurement& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::cos));
  }), "cos");
  engine.add(chaiscript::fun([](const units::precise_measurement& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::tan));
  }), "tan");
  engine.add(chaiscript::fun([](const units::precise_measurement& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::asin));
  }), "asin");
  engine.add(chaiscript::fun([](const units::precise_measurement& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::acos));
  }), "acos");
  engine.add(chaiscript::fun([](const units::precise_measurement& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::atan));
  }), "atan");
  engine.add(chaiscript::fun([](double value, const units::precise_measurement& quantity) {
    return quantity * value;
  }), "*");
  engine.add(chaiscript::fun([](const units::precise_measurement& quantity, double value) {
    return quantity * value;
  }), "*");
  engine.add(chaiscript::fun([](const units::precise_measurement& quantity, double value) {
    if (value == 0.0) {
      throw std::domain_error("division by zero");
    }
    return quantity / value;
  }), "/");
}

}  // namespace wpilib::expression::detail
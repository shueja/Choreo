#include "scalar_bindings.hpp"

#include "units.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace wpilib::expression::detail {
namespace {

Dimension LengthDimension() { return MakeDimension(1, 0, 0, 0, 0); }
Dimension AngleDimension() { return MakeDimension(0, 0, 0, 0, 1); }

Quantity Power(const Quantity& value, double exponent) {
  if (!std::isfinite(exponent) || std::trunc(exponent) != exponent ||
      std::abs(exponent) > 16.0) {
    throw std::invalid_argument("quantity powers must be integers with magnitude at most 16");
  }
  int integerExponent = static_cast<int>(exponent);
  Quantity result{std::pow(value.siValue, exponent),
                  ScaleDimension(value.dimension, integerExponent)};
  RequireFinite(result);
  return result;
}

Quantity Add(const Quantity& left, const Quantity& right) {
  if (left.dimension != right.dimension) {
    throw std::invalid_argument("addition requires matching dimensions");
  }
  Quantity result{left.siValue + right.siValue, left.dimension};
  RequireFinite(result);
  return result;
}

Quantity Subtract(const Quantity& left, const Quantity& right) {
  if (left.dimension != right.dimension) {
    throw std::invalid_argument("subtraction requires matching dimensions");
  }
  Quantity result{left.siValue - right.siValue, left.dimension};
  RequireFinite(result);
  return result;
}

Quantity Multiply(const Quantity& left, const Quantity& right) {
  Quantity result{left.siValue * right.siValue, Combine(left.dimension, right.dimension, 1)};
  RequireFinite(result);
  return result;
}

Quantity Divide(const Quantity& left, const Quantity& right) {
  if (right.siValue == 0.0) {
    throw std::domain_error("division by zero");
  }
  Quantity result{left.siValue / right.siValue, Combine(left.dimension, right.dimension, -1)};
  RequireFinite(result);
  return result;
}

void RequireComparable(const Quantity& left, const Quantity& right) {
  if (left.dimension != right.dimension) {
    throw std::invalid_argument("comparison requires matching dimensions");
  }
}

bool Equal(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.siValue == right.siValue;
}

bool Less(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.siValue < right.siValue;
}

bool LessEqual(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.siValue <= right.siValue;
}

bool Greater(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.siValue > right.siValue;
}

bool GreaterEqual(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.siValue >= right.siValue;
}

Quantity TrigInput(const Quantity& value, double (*function)(double)) {
  if (value.dimension != AngleDimension() && value.dimension != Dimension{}) {
    throw std::invalid_argument(
        "trigonometric functions require an angle or dimensionless value");
  }
  Quantity result{function(value.siValue), Dimension{}};
  RequireFinite(result);
  return result;
}

Quantity InverseTrigInput(const Quantity& value, double (*function)(double)) {
  if (value.dimension != Dimension{}) {
    throw std::invalid_argument(
        "inverse trigonometric functions require a dimensionless value");
  }
  Quantity result{function(value.siValue), AngleDimension()};
  RequireFinite(result);
  return result;
}

}  // namespace

void ConfigureScalarBindings(chaiscript::ChaiScript& engine) {
  engine.add(chaiscript::user_type<Quantity>(), "Quantity");
  engine.add(chaiscript::fun([](double value, const std::string& unit) {
    return Quantity::From(value, unit);
  }), "quantity");
  engine.add(chaiscript::fun(&Add), "+");
  engine.add(chaiscript::fun(&Subtract), "-");
  engine.add(chaiscript::fun(&Multiply), "*");
  engine.add(chaiscript::fun(&Divide), "/");
  engine.add(chaiscript::fun(&Power), "^");
  engine.add(chaiscript::fun(&Equal), "==");
  engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
    return !Equal(left, right);
  }), "!=");
  engine.add(chaiscript::fun(&Less), "<");
  engine.add(chaiscript::fun(&LessEqual), "<=");
  engine.add(chaiscript::fun(&Greater), ">");
  engine.add(chaiscript::fun(&GreaterEqual), ">=");
  engine.add(chaiscript::fun([](const Quantity& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::sin));
  }), "sin");
  engine.add(chaiscript::fun([](const Quantity& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::cos));
  }), "cos");
  engine.add(chaiscript::fun([](const Quantity& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::tan));
  }), "tan");
  engine.add(chaiscript::fun([](const Quantity& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::asin));
  }), "asin");
  engine.add(chaiscript::fun([](const Quantity& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::acos));
  }), "acos");
  engine.add(chaiscript::fun([](const Quantity& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::atan));
  }), "atan");
  engine.add(chaiscript::fun([](double value, const Quantity& quantity) {
    Quantity result{value * quantity.siValue, quantity.dimension};
    RequireFinite(result);
    return result;
  }), "*");
  engine.add(chaiscript::fun([](const Quantity& quantity, double value) {
    Quantity result{quantity.siValue * value, quantity.dimension};
    RequireFinite(result);
    return result;
  }), "*");
  engine.add(chaiscript::fun([](const Quantity& quantity, double value) {
    if (value == 0.0) {
      throw std::domain_error("division by zero");
    }
    Quantity result{quantity.siValue / value, quantity.dimension};
    RequireFinite(result);
    return result;
  }), "/");
}

}  // namespace wpilib::expression::detail
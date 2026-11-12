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
  return Quantity{pow(value.measurement(), integerExponent),
                  ScaleDimension(value.dimension, integerExponent)};
}

Quantity Add(const Quantity& left, const Quantity& right) {
  if (left.dimension != right.dimension) {
    throw std::invalid_argument("addition requires matching dimensions");
  }
  return Quantity{left.measurement() + right.measurement(), left.dimension};
}

Quantity Subtract(const Quantity& left, const Quantity& right) {
  if (left.dimension != right.dimension) {
    throw std::invalid_argument("subtraction requires matching dimensions");
  }
  return Quantity{left.measurement() - right.measurement(), left.dimension};
}

Quantity Multiply(const Quantity& left, const Quantity& right) {
  return Quantity{left.measurement() * right.measurement(),
                  Combine(left.dimension, right.dimension, 1)};
}

Quantity Divide(const Quantity& left, const Quantity& right) {
  if (right.siValue == 0.0) {
    throw std::domain_error("division by zero");
  }
  return Quantity{left.measurement() / right.measurement(),
                  Combine(left.dimension, right.dimension, -1)};
}

void RequireComparable(const Quantity& left, const Quantity& right) {
  if (left.dimension != right.dimension) {
    throw std::invalid_argument("comparison requires matching dimensions");
  }
}

bool Equal(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.measurement() == right.measurement();
}

bool Less(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.measurement() < right.measurement();
}

bool LessEqual(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.measurement() <= right.measurement();
}

bool Greater(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.measurement() > right.measurement();
}

bool GreaterEqual(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return left.measurement() >= right.measurement();
}

Quantity TrigInput(const Quantity& value, double (*function)(double)) {
  if (value.dimension != AngleDimension() && value.dimension != Dimension{}) {
    throw std::invalid_argument(
        "trigonometric functions require an angle or dimensionless value");
  }
  return Quantity{function(value.measurement().value()), Dimension{}};
}

Quantity InverseTrigInput(const Quantity& value, double (*function)(double)) {
  if (value.dimension != Dimension{}) {
    throw std::invalid_argument(
        "inverse trigonometric functions require a dimensionless value");
  }
  return Quantity{function(value.measurement().value()), AngleDimension()};
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
    return Quantity{quantity.measurement() * value, quantity.dimension};
  }), "*");
  engine.add(chaiscript::fun([](const Quantity& quantity, double value) {
    return Quantity{quantity.measurement() * value, quantity.dimension};
  }), "*");
  engine.add(chaiscript::fun([](const Quantity& quantity, double value) {
    if (value == 0.0) {
      throw std::domain_error("division by zero");
    }
    return Quantity{quantity.measurement() / value, quantity.dimension};
  }), "/");
}

}  // namespace wpilib::expression::detail
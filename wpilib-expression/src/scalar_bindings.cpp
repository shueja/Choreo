#include "scalar_bindings.hpp"

#include "units.hpp"

#include <chaiscript/extras/math.hpp>

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

Quantity Negate(const Quantity& value) {
  Quantity result{-value.siValue, value.dimension};
  RequireFinite(result);
  return result;
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

Quantity Absolute(const Quantity& value) {
  Quantity result{std::abs(value.siValue), value.dimension};
  RequireFinite(result);
  return result;
}

Quantity Minimum(const Quantity& left, const Quantity& right) {
  RequireFinite(left);
  RequireFinite(right);
  RequireComparable(left, right);
  return left.siValue <= right.siValue ? left : right;
}

Quantity Maximum(const Quantity& left, const Quantity& right) {
  RequireFinite(left);
  RequireFinite(right);
  RequireComparable(left, right);
  return left.siValue >= right.siValue ? left : right;
}

Quantity DimensionlessMath(const Quantity& value, double (*function)(double)) {
  if (value.dimension != Dimension{}) {
    throw std::invalid_argument("math function requires a dimensionless quantity");
  }
  Quantity result{function(value.siValue), {}};
  RequireFinite(result);
  return result;
}

Quantity IntegerDimensionlessMath(const Quantity& value, int (*function)(double)) {
  if (value.dimension != Dimension{}) {
    throw std::invalid_argument("math function requires a dimensionless quantity");
  }
  return Quantity{static_cast<double>(function(value.siValue)), {}};
}

Quantity PreserveDimensionMath(const Quantity& value, double (*function)(double)) {
  RequireFinite(value);
  Quantity result{function(value.siValue), value.dimension};
  RequireFinite(result);
  return result;
}

Dimension RootDimension(const Dimension& dimension, int degree) {
  Dimension result;
  for (std::size_t index = 0; index < dimension.exponents.size(); ++index) {
    if (dimension.exponents[index] % degree != 0) {
      throw std::invalid_argument("quantity dimension is not divisible by the root degree");
    }
    result.exponents[index] = dimension.exponents[index] / degree;
  }
  return result;
}

Quantity SquareRoot(const Quantity& value) {
  RequireFinite(value);
  Quantity result{std::sqrt(value.siValue), RootDimension(value.dimension, 2)};
  RequireFinite(result);
  return result;
}

Quantity CubeRoot(const Quantity& value) {
  RequireFinite(value);
  Quantity result{std::cbrt(value.siValue), RootDimension(value.dimension, 3)};
  RequireFinite(result);
  return result;
}

Quantity Hypotenuse(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  Quantity result{std::hypot(left.siValue, right.siValue), left.dimension};
  RequireFinite(result);
  return result;
}

Quantity Atan2(const Quantity& y, const Quantity& x) {
  RequireComparable(y, x);
  Quantity result{std::atan2(y.siValue, x.siValue), AngleDimension()};
  RequireFinite(result);
  return result;
}

Quantity MathPower(const Quantity& value, const Quantity& exponent) {
  if (exponent.dimension != Dimension{}) {
    throw std::invalid_argument("power exponent must be dimensionless");
  }
  if (value.dimension != Dimension{}) {
    return Power(value, exponent.siValue);
  }
  Quantity result{std::pow(value.siValue, exponent.siValue), {}};
  RequireFinite(result);
  return result;
}

Quantity MathFma(const Quantity& left, const Quantity& right, const Quantity& addend) {
  if (Combine(left.dimension, right.dimension, 1) != addend.dimension) {
    throw std::invalid_argument("fma product and addend dimensions must match");
  }
  Quantity result{std::fma(left.siValue, right.siValue, addend.siValue), addend.dimension};
  RequireFinite(result);
  return result;
}

Quantity CopySign(const Quantity& value, const Quantity& sign) {
  RequireFinite(value);
  RequireFinite(sign);
  Quantity result{std::copysign(value.siValue, sign.siValue), value.dimension};
  RequireFinite(result);
  return result;
}

Quantity ScalePowerOfTwo(const Quantity& value, int exponent) {
  RequireFinite(value);
  Quantity result{std::scalbn(value.siValue, exponent), value.dimension};
  RequireFinite(result);
  return result;
}

Quantity ScalePowerOfTwoLong(const Quantity& value, long exponent) {
  RequireFinite(value);
  Quantity result{std::scalbln(value.siValue, exponent), value.dimension};
  RequireFinite(result);
  return result;
}

Quantity NextToward(const Quantity& value, const Quantity& direction) {
  RequireComparable(value, direction);
  Quantity result{std::nexttoward(value.siValue, static_cast<long double>(direction.siValue)),
                  value.dimension};
  RequireFinite(result);
  return result;
}

Quantity IntegerRounding(const Quantity& value, long long (*function)(double)) {
  if (value.dimension != Dimension{}) {
    throw std::invalid_argument("integer rounding requires a dimensionless quantity");
  }
  return Quantity{static_cast<double>(function(value.siValue)), {}};
}

Quantity LongRounding(const Quantity& value, long (*function)(double)) {
  if (value.dimension != Dimension{}) {
    throw std::invalid_argument("integer rounding requires a dimensionless quantity");
  }
  return Quantity{static_cast<double>(function(value.siValue)), {}};
}

Quantity Classify(const Quantity& value) {
  return Quantity{static_cast<double>(std::fpclassify(value.siValue)), {}};
}

bool IsFinite(const Quantity& value) { return std::isfinite(value.siValue); }
bool IsInf(const Quantity& value) { return std::isinf(value.siValue); }
bool IsNan(const Quantity& value) { return std::isnan(value.siValue); }
bool IsNormal(const Quantity& value) { return std::isnormal(value.siValue); }
bool SignBit(const Quantity& value) { return std::signbit(value.siValue); }

bool IsGreater(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return std::isgreater(left.siValue, right.siValue);
}

bool IsGreaterEqual(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return std::isgreaterequal(left.siValue, right.siValue);
}

bool IsLess(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return std::isless(left.siValue, right.siValue);
}

bool IsLessEqual(const Quantity& left, const Quantity& right) {
  RequireComparable(left, right);
  return std::islessequal(left.siValue, right.siValue);
}

Quantity SameDimensionMath(const Quantity& left, const Quantity& right,
                           double (*function)(double, double)) {
  RequireComparable(left, right);
  Quantity result{function(left.siValue, right.siValue), left.dimension};
  RequireFinite(result);
  return result;
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
  engine.add(chaiscript::extras::math::bootstrap());
  engine.add(chaiscript::user_type<Quantity>(), "Quantity");
  engine.add(chaiscript::type_conversion<double, Quantity>([](double value) {
    return Quantity{value, {}};
  }));
  engine.add(chaiscript::type_conversion<int, Quantity>([](int value) {
    return Quantity{static_cast<double>(value), {}};
  }));
  engine.add(chaiscript::type_conversion<long, Quantity>([](long value) {
    return Quantity{static_cast<double>(value), {}};
  }));
  engine.add(chaiscript::type_conversion<long long, Quantity>([](long long value) {
    return Quantity{static_cast<double>(value), {}};
  }));
  engine.add(chaiscript::fun([](double value, const std::string& unit) {
    return Quantity::From(value, unit);
  }), "quantity");
  engine.add(chaiscript::fun(&Add), "+");
  engine.add(chaiscript::fun(&Subtract), "-");
  engine.add(chaiscript::fun(&Negate), "-");
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
  engine.add(chaiscript::fun(&Absolute), "abs");
  engine.add(chaiscript::fun([](int value) {
    return Absolute(Quantity{static_cast<double>(value), {}});
  }), "abs");
  engine.add(chaiscript::fun([](double left, double right) {
    return Minimum(Quantity{left, {}}, Quantity{right, {}});
  }), "minimum");
  engine.add(chaiscript::fun(&Minimum), "minimum");
  engine.add(chaiscript::fun([](double left, double right) {
    return Maximum(Quantity{left, {}}, Quantity{right, {}});
  }), "maximum");
  engine.add(chaiscript::fun(&Maximum), "maximum");
#define ADD_DIMENSIONLESS_MATH(name) \
  engine.add(chaiscript::fun([](const Quantity& value) { \
    return DimensionlessMath(value, static_cast<double (*)(double)>(std::name)); \
  }), #name)
#define ADD_PRESERVING_MATH(name) \
  engine.add(chaiscript::fun([](const Quantity& value) { \
    return PreserveDimensionMath(value, static_cast<double (*)(double)>(std::name)); \
  }), #name)
  ADD_DIMENSIONLESS_MATH(sinh);
  ADD_DIMENSIONLESS_MATH(cosh);
  ADD_DIMENSIONLESS_MATH(tanh);
  ADD_DIMENSIONLESS_MATH(asinh);
  ADD_DIMENSIONLESS_MATH(acosh);
  ADD_DIMENSIONLESS_MATH(atanh);
  ADD_DIMENSIONLESS_MATH(exp);
  ADD_DIMENSIONLESS_MATH(exp2);
  ADD_DIMENSIONLESS_MATH(expm1);
  ADD_DIMENSIONLESS_MATH(log);
  ADD_DIMENSIONLESS_MATH(log10);
  ADD_DIMENSIONLESS_MATH(log1p);
  ADD_DIMENSIONLESS_MATH(log2);
  ADD_DIMENSIONLESS_MATH(logb);
  ADD_DIMENSIONLESS_MATH(erf);
  ADD_DIMENSIONLESS_MATH(erfc);
  ADD_DIMENSIONLESS_MATH(tgamma);
  ADD_DIMENSIONLESS_MATH(lgamma);
  ADD_PRESERVING_MATH(fabs);
  ADD_PRESERVING_MATH(floor);
  ADD_PRESERVING_MATH(ceil);
  ADD_PRESERVING_MATH(round);
  ADD_PRESERVING_MATH(trunc);
  ADD_PRESERVING_MATH(rint);
  ADD_PRESERVING_MATH(nearbyint);
#undef ADD_DIMENSIONLESS_MATH
#undef ADD_PRESERVING_MATH
  engine.add(chaiscript::fun([](const Quantity& value) {
    return IntegerDimensionlessMath(value, static_cast<int (*)(double)>(std::ilogb));
  }), "ilogb");
  engine.add(chaiscript::fun(&SquareRoot), "sqrt");
  engine.add(chaiscript::fun([](int value) {
    return SquareRoot(Quantity{static_cast<double>(value), {}});
  }), "sqrt");
  engine.add(chaiscript::fun(&CubeRoot), "cbrt");
  engine.add(chaiscript::fun(&Hypotenuse), "hypot");
  engine.add(chaiscript::fun(&Atan2), "atan2");
  engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fmod));
  }), "fmod");
  engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::remainder));
  }), "remainder");
  engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fmin));
  }), "fmin");
  engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fmax));
  }), "fmax");
  engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fdim));
  }), "fdim");
  engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::nextafter));
  }), "nextafter");
  engine.add(chaiscript::fun(&NextToward), "nexttoward");
  engine.add(chaiscript::fun([](const Quantity& value, int exponent) {
    return ScalePowerOfTwo(value, exponent);
  }), "ldexp");
  engine.add(chaiscript::fun([](const Quantity& value, int exponent) {
    return ScalePowerOfTwo(value, exponent);
  }), "scalbn");
  engine.add(chaiscript::fun([](const Quantity& value, long exponent) {
    return ScalePowerOfTwoLong(value, exponent);
  }), "scalbln");
  engine.add(chaiscript::fun(&CopySign), "copysign");
  engine.add(chaiscript::fun(&MathFma), "fma");
  engine.add(chaiscript::fun([](const Quantity& value, const Quantity& exponent) {
    return MathPower(value, exponent);
  }), "pow");
  engine.add(chaiscript::fun([](const Quantity& value, double exponent) {
    return MathPower(value, Quantity{exponent, {}});
  }), "pow");
  engine.add(chaiscript::fun(&IsFinite), "isfinite");
  engine.add(chaiscript::fun(&IsInf), "isinf");
  engine.add(chaiscript::fun(&IsNan), "isnan");
  engine.add(chaiscript::fun(&IsNormal), "isnormal");
  engine.add(chaiscript::fun(&SignBit), "signbit");
  engine.add(chaiscript::fun(&IsGreater), "isgreater");
  engine.add(chaiscript::fun(&IsGreaterEqual), "isgreaterequal");
  engine.add(chaiscript::fun(&IsLess), "isless");
  engine.add(chaiscript::fun(&IsLessEqual), "islessequal");
    engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
      RequireComparable(left, right);
      return std::islessgreater(left.siValue, right.siValue);
    }), "islessgreater");
    engine.add(chaiscript::fun([](const Quantity& left, const Quantity& right) {
      RequireComparable(left, right);
      return std::isunordered(left.siValue, right.siValue);
    }), "isunordered");
    engine.add(chaiscript::fun(&Classify), "fpclassify");
    engine.add(chaiscript::fun([](const Quantity& value) {
      return LongRounding(value, static_cast<long (*)(double)>(std::lrint));
    }), "lrint");
    engine.add(chaiscript::fun([](const Quantity& value) {
      return LongRounding(value, static_cast<long (*)(double)>(std::lround));
    }), "lround");
    engine.add(chaiscript::fun([](const Quantity& value) {
      return IntegerRounding(value, static_cast<long long (*)(double)>(std::llrint));
    }), "llrint");
    engine.add(chaiscript::fun([](const Quantity& value) {
      return IntegerRounding(value, static_cast<long long (*)(double)>(std::llround));
    }), "llround");
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
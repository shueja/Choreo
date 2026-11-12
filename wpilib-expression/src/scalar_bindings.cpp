#include "scalar_bindings.hpp"

#include "units.hpp"

#include <chaiscript/extras/math.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace wpilib::expression::detail {
namespace {

using Measurement = units::precise_measurement;

Dimension AngleDimension() { return MakeDimension(0, 0, 0, 0, 1); }

Measurement Power(const Measurement& value, double exponent) {
  if (!std::isfinite(exponent) || std::trunc(exponent) != exponent ||
      std::abs(exponent) > 16.0) {
    throw std::invalid_argument("quantity powers must be integers with magnitude at most 16");
  }
  return pow(value, static_cast<int>(exponent));
}

Measurement Add(const Measurement& left, const Measurement& right) {
  if (MeasurementDimension(left) != MeasurementDimension(right)) {
    throw std::invalid_argument("addition requires matching dimensions");
  }
  return left + right;
}

Measurement Subtract(const Measurement& left, const Measurement& right) {
  if (MeasurementDimension(left) != MeasurementDimension(right)) {
    throw std::invalid_argument("subtraction requires matching dimensions");
  }
  return left - right;
}

Measurement Negate(const Measurement& value) { return -value; }
Measurement Multiply(const Measurement& left, const Measurement& right) { return left * right; }

Measurement Divide(const Measurement& left, const Measurement& right) {
  if (SiValue(right) == 0.0) {
    throw std::domain_error("division by zero");
  }
  return left / right;
}

void RequireComparable(const Measurement& left, const Measurement& right) {
  if (MeasurementDimension(left) != MeasurementDimension(right)) {
    throw std::invalid_argument("comparison requires matching dimensions");
  }
}

bool Equal(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return SiValue(left) == SiValue(right);
}
bool Less(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return SiValue(left) < SiValue(right);
}
bool LessEqual(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return SiValue(left) <= SiValue(right);
}
bool Greater(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return SiValue(left) > SiValue(right);
}
bool GreaterEqual(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return SiValue(left) >= SiValue(right);
}

Measurement Absolute(const Measurement& value) {
  return MakeMeasurement(std::abs(SiValue(value)), MeasurementDimension(value));
}
Measurement Minimum(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return MakeMeasurement(std::min(SiValue(left), SiValue(right)), MeasurementDimension(left));
}
Measurement Maximum(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return MakeMeasurement(std::max(SiValue(left), SiValue(right)), MeasurementDimension(left));
}

Measurement DimensionlessMath(const Measurement& value, double (*function)(double)) {
  if (MeasurementDimension(value) != Dimension{}) {
    throw std::invalid_argument("math function requires a dimensionless quantity");
  }
  return MakeMeasurement(function(SiValue(value)), {});
}
Measurement PreserveDimensionMath(const Measurement& value, double (*function)(double)) {
  return MakeMeasurement(function(SiValue(value)), MeasurementDimension(value));
}
Measurement IntegerDimensionlessMath(const Measurement& value, int (*function)(double)) {
  if (MeasurementDimension(value) != Dimension{}) {
    throw std::invalid_argument("math function requires a dimensionless quantity");
  }
  return MakeMeasurement(static_cast<double>(function(SiValue(value))), {});
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
Measurement SquareRoot(const Measurement& value) {
  return MakeMeasurement(std::sqrt(SiValue(value)), RootDimension(MeasurementDimension(value), 2));
}
Measurement CubeRoot(const Measurement& value) {
  return MakeMeasurement(std::cbrt(SiValue(value)), RootDimension(MeasurementDimension(value), 3));
}
Measurement Hypotenuse(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return MakeMeasurement(std::hypot(SiValue(left), SiValue(right)), MeasurementDimension(left));
}
Measurement Atan2(const Measurement& y, const Measurement& x) {
  RequireComparable(y, x);
  return MakeMeasurement(std::atan2(SiValue(y), SiValue(x)), AngleDimension());
}
Measurement MathPower(const Measurement& value, const Measurement& exponent) {
  if (MeasurementDimension(exponent) != Dimension{}) {
    throw std::invalid_argument("power exponent must be dimensionless");
  }
  if (MeasurementDimension(value) != Dimension{}) {
    return Power(value, SiValue(exponent));
  }
  return MakeMeasurement(std::pow(SiValue(value), SiValue(exponent)), {});
}
Measurement MathFma(const Measurement& left, const Measurement& right, const Measurement& addend) {
  auto dimension = Combine(MeasurementDimension(left), MeasurementDimension(right), 1);
  if (dimension != MeasurementDimension(addend)) {
    throw std::invalid_argument("fma product and addend dimensions must match");
  }
  return MakeMeasurement(std::fma(SiValue(left), SiValue(right), SiValue(addend)), dimension);
}
Measurement CopySign(const Measurement& value, const Measurement& sign) {
  RequireComparable(value, sign);
  return MakeMeasurement(std::copysign(SiValue(value), SiValue(sign)), MeasurementDimension(value));
}
Measurement ScalePowerOfTwo(const Measurement& value, int exponent) {
  return MakeMeasurement(std::scalbn(SiValue(value), exponent), MeasurementDimension(value));
}
Measurement ScalePowerOfTwoLong(const Measurement& value, long exponent) {
  return MakeMeasurement(std::scalbln(SiValue(value), exponent), MeasurementDimension(value));
}
Measurement NextToward(const Measurement& value, const Measurement& direction) {
  RequireComparable(value, direction);
  return MakeMeasurement(
      std::nexttoward(SiValue(value), static_cast<long double>(SiValue(direction))),
      MeasurementDimension(value));
}
Measurement SameDimensionMath(const Measurement& left, const Measurement& right,
                              double (*function)(double, double)) {
  RequireComparable(left, right);
  return MakeMeasurement(function(SiValue(left), SiValue(right)), MeasurementDimension(left));
}
Measurement IntegerRounding(const Measurement& value, long long (*function)(double)) {
  if (MeasurementDimension(value) != Dimension{}) {
    throw std::invalid_argument("integer rounding requires a dimensionless quantity");
  }
  return MakeMeasurement(static_cast<double>(function(SiValue(value))), {});
}
Measurement LongRounding(const Measurement& value, long (*function)(double)) {
  if (MeasurementDimension(value) != Dimension{}) {
    throw std::invalid_argument("integer rounding requires a dimensionless quantity");
  }
  return MakeMeasurement(static_cast<double>(function(SiValue(value))), {});
}
Measurement Classify(const Measurement& value) {
  return MakeMeasurement(static_cast<double>(std::fpclassify(SiValue(value))), {});
}

bool IsFinite(const Measurement& value) { return std::isfinite(SiValue(value)); }
bool IsInf(const Measurement& value) { return std::isinf(SiValue(value)); }
bool IsNan(const Measurement& value) { return std::isnan(SiValue(value)); }
bool IsNormal(const Measurement& value) { return std::isnormal(SiValue(value)); }
bool SignBit(const Measurement& value) { return std::signbit(SiValue(value)); }
bool IsGreater(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return std::isgreater(SiValue(left), SiValue(right));
}
bool IsGreaterEqual(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return std::isgreaterequal(SiValue(left), SiValue(right));
}
bool IsLess(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return std::isless(SiValue(left), SiValue(right));
}
bool IsLessEqual(const Measurement& left, const Measurement& right) {
  RequireComparable(left, right);
  return std::islessequal(SiValue(left), SiValue(right));
}
Measurement TrigInput(const Measurement& value, double (*function)(double)) {
  auto dimension = MeasurementDimension(value);
  if (dimension != AngleDimension() && dimension != Dimension{}) {
    throw std::invalid_argument("trigonometric functions require an angle or dimensionless value");
  }
  return MakeMeasurement(function(SiValue(value)), {});
}
Measurement InverseTrigInput(const Measurement& value, double (*function)(double)) {
  if (MeasurementDimension(value) != Dimension{}) {
    throw std::invalid_argument("inverse trigonometric functions require a dimensionless value");
  }
  return MakeMeasurement(function(SiValue(value)), AngleDimension());
}

}  // namespace

void ConfigureScalarBindings(chaiscript::ChaiScript& engine) {
  engine.add(chaiscript::extras::math::bootstrap());
  engine.add(chaiscript::user_type<Measurement>(), "precise_measurement");
  engine.add(chaiscript::type_conversion<double, Measurement>([](double value) {
    return Measurement{value, units::precise::one};
  }));
  engine.add(chaiscript::type_conversion<int, Measurement>([](int value) {
    return Measurement{static_cast<double>(value), units::precise::one};
  }));
  engine.add(chaiscript::type_conversion<long, Measurement>([](long value) {
    return Measurement{static_cast<double>(value), units::precise::one};
  }));
  engine.add(chaiscript::type_conversion<long long, Measurement>([](long long value) {
    return Measurement{static_cast<double>(value), units::precise::one};
  }));
  engine.add(chaiscript::type_conversion<float, Measurement>([](float value) {
    return Measurement{static_cast<double>(value), units::precise::one};
  }));
  engine.add(chaiscript::fun([](double value, const std::string& unit) {
    return From(value, unit);
  }), "quantity");
  engine.add(chaiscript::fun(&Add), "+");
  engine.add(chaiscript::fun(&Subtract), "-");
  engine.add(chaiscript::fun(&Negate), "-");
  engine.add(chaiscript::fun(&Multiply), "*");
  engine.add(chaiscript::fun(&Divide), "/");
  engine.add(chaiscript::fun(&Power), "^");
  engine.add(chaiscript::fun(&Equal), "==");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    return !Equal(left, right);
  }), "!=");
  engine.add(chaiscript::fun(&Less), "<");
  engine.add(chaiscript::fun(&LessEqual), "<=");
  engine.add(chaiscript::fun(&Greater), ">");
  engine.add(chaiscript::fun(&GreaterEqual), ">=");
  engine.add(chaiscript::fun(&Absolute), "abs");
  engine.add(chaiscript::fun(&Absolute), "fabs");
  engine.add(chaiscript::fun(&Minimum), "min");
  engine.add(chaiscript::fun(&Minimum), "minimum");
  engine.add(chaiscript::fun(&Maximum), "max");
  engine.add(chaiscript::fun(&Maximum), "maximum");
#define ADD_DIMENSIONLESS_MATH(name) \
  engine.add(chaiscript::fun([](const Measurement& value) { \
    return DimensionlessMath(value, static_cast<double (*)(double)>(std::name)); \
  }), #name)
#define ADD_PRESERVING_MATH(name) \
  engine.add(chaiscript::fun([](const Measurement& value) { \
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
  ADD_PRESERVING_MATH(floor);
  ADD_PRESERVING_MATH(ceil);
  ADD_PRESERVING_MATH(round);
  ADD_PRESERVING_MATH(trunc);
  ADD_PRESERVING_MATH(rint);
  ADD_PRESERVING_MATH(nearbyint);
#undef ADD_DIMENSIONLESS_MATH
#undef ADD_PRESERVING_MATH
  engine.add(chaiscript::fun([](const Measurement& value) {
    return IntegerDimensionlessMath(value, static_cast<int (*)(double)>(std::ilogb));
  }), "ilogb");
  engine.add(chaiscript::fun(&SquareRoot), "sqrt");
  engine.add(chaiscript::fun(&CubeRoot), "cbrt");
  engine.add(chaiscript::fun(&Hypotenuse), "hypot");
  engine.add(chaiscript::fun(&Atan2), "atan2");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fmod));
  }), "fmod");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::remainder));
  }), "remainder");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fmin));
  }), "fmin");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fmax));
  }), "fmax");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::fdim));
  }), "fdim");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    return SameDimensionMath(left, right, static_cast<double (*)(double, double)>(std::nextafter));
  }), "nextafter");
  engine.add(chaiscript::fun(&NextToward), "nexttoward");
  engine.add(chaiscript::fun([](const Measurement& value, int exponent) {
    return ScalePowerOfTwo(value, exponent);
  }), "ldexp");
  engine.add(chaiscript::fun([](const Measurement& value, int exponent) {
    return ScalePowerOfTwo(value, exponent);
  }), "scalbn");
  engine.add(chaiscript::fun([](const Measurement& value, long exponent) {
    return ScalePowerOfTwoLong(value, exponent);
  }), "scalbln");
  engine.add(chaiscript::fun(&CopySign), "copysign");
  engine.add(chaiscript::fun(&MathFma), "fma");
  engine.add(chaiscript::fun(&MathPower), "pow");
  engine.add(chaiscript::fun([](const Measurement& value, double exponent) {
    return MathPower(value, Measurement{exponent, units::precise::one});
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
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    RequireComparable(left, right);
    return std::islessgreater(SiValue(left), SiValue(right));
  }), "islessgreater");
  engine.add(chaiscript::fun([](const Measurement& left, const Measurement& right) {
    RequireComparable(left, right);
    return std::isunordered(SiValue(left), SiValue(right));
  }), "isunordered");
  engine.add(chaiscript::fun(&Classify), "fpclassify");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return LongRounding(value, static_cast<long (*)(double)>(std::lrint));
  }), "lrint");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return LongRounding(value, static_cast<long (*)(double)>(std::lround));
  }), "lround");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return IntegerRounding(value, static_cast<long long (*)(double)>(std::llrint));
  }), "llrint");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return IntegerRounding(value, static_cast<long long (*)(double)>(std::llround));
  }), "llround");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::sin));
  }), "sin");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::cos));
  }), "cos");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return TrigInput(value, static_cast<double (*)(double)>(std::tan));
  }), "tan");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::asin));
  }), "asin");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::acos));
  }), "acos");
  engine.add(chaiscript::fun([](const Measurement& value) {
    return InverseTrigInput(value, static_cast<double (*)(double)>(std::atan));
  }), "atan");
  engine.add(chaiscript::fun([](double value, const Measurement& quantity) {
    return quantity * value;
  }), "*");
  engine.add(chaiscript::fun([](const Measurement& quantity, double value) {
    return quantity * value;
  }), "*");
  engine.add(chaiscript::fun([](const Measurement& quantity, double value) {
    if (value == 0.0) {
      throw std::domain_error("division by zero");
    }
    return quantity / value;
  }), "/");
}

}  // namespace wpilib::expression::detail

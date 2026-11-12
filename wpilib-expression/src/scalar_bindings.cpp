#include "scalar_bindings.hpp"

#include "units.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace wpilib::expression::detail {
namespace {

template <auto Function, bool Inverse>
units::precise_measurement BindTrig(const units::precise_measurement& value) {
  const auto dimension = value.units().base_units();
  if constexpr (Inverse) {
    if (dimension != units::precise::one.base_units()) {
      throw std::invalid_argument(
          "inverse trigonometric functions require a dimensionless value");
    }
  } else if (dimension != units::precise::rad.base_units() &&
             dimension != units::precise::one.base_units()) {
    throw std::invalid_argument(
        "trigonometric functions require an angle or dimensionless value");
  }
  const auto resultUnit = Inverse ? units::precise::rad : units::precise::one;
  return units::precise_measurement{Function(SiValue(value)), resultUnit};
}

units::precise_measurement Power(const units::precise_measurement& value, double exponent) {
  if (!std::isfinite(exponent) || std::trunc(exponent) != exponent ||
      std::abs(exponent) > 16.0) {
    throw std::invalid_argument("quantity powers must be integers with magnitude at most 16");
  }
  int integerExponent = static_cast<int>(exponent);
  return pow(value, integerExponent);
}

units::precise_measurement DivideMeasurements(
    const units::precise_measurement& numerator,
    const units::precise_measurement& denominator) {
  if (SiValue(denominator) == 0.0) {
    throw std::domain_error("division by zero");
  }
  return numerator / denominator;
}

}  // namespace

void ConfigureScalarBindings(chaiscript::ChaiScript& engine) {
#define BIND_MEASUREMENT_OPERATOR(name, operation) \
  engine.add(chaiscript::fun([](const units::precise_measurement& left, \
                                const units::precise_measurement& right) { \
    return left operation right; \
  }), name)
  engine.add(chaiscript::user_type<units::precise_measurement>(), "precise_measurement");
  engine.add(chaiscript::fun([](double value, const std::string& unit) {
    auto parsed = detail::ParseUnit(unit);
    if (!parsed) {
      throw chaiscript::exception::eval_error(ErrorMessage(parsed.error()));
    }
    return From(value, *parsed);
  }), "quantity");
  BIND_MEASUREMENT_OPERATOR("+", +);
  BIND_MEASUREMENT_OPERATOR("-", -);
  BIND_MEASUREMENT_OPERATOR("*", *);
  engine.add(chaiscript::fun(&DivideMeasurements), "/");
  engine.add(chaiscript::fun(&Power), "^");
  BIND_MEASUREMENT_OPERATOR("==", ==);
  BIND_MEASUREMENT_OPERATOR("!=", !=);
  BIND_MEASUREMENT_OPERATOR("<", <);
  BIND_MEASUREMENT_OPERATOR("<=", <=);
  BIND_MEASUREMENT_OPERATOR(">", >);
  BIND_MEASUREMENT_OPERATOR(">=", >=);
  engine.add(chaiscript::fun(&BindTrig<static_cast<double (*)(double)>(std::sin), false>), "sin");
  engine.add(chaiscript::fun(&BindTrig<static_cast<double (*)(double)>(std::cos), false>), "cos");
  engine.add(chaiscript::fun(&BindTrig<static_cast<double (*)(double)>(std::tan), false>), "tan");
  engine.add(chaiscript::fun(&BindTrig<static_cast<double (*)(double)>(std::asin), true>), "asin");
  engine.add(chaiscript::fun(&BindTrig<static_cast<double (*)(double)>(std::acos), true>), "acos");
  engine.add(chaiscript::fun(&BindTrig<static_cast<double (*)(double)>(std::atan), true>), "atan");
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
#undef BIND_MEASUREMENT_OPERATOR
}

}  // namespace wpilib::expression::detail
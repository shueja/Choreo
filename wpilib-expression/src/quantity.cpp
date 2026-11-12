#include "wpilib-expression/quantity.hpp"

#include "units.hpp"

#include <stdexcept>
#include <utility>

namespace wpilib::expression {
namespace {

units::precise_unit UnitForDimension(const Dimension& dimension) {
  auto unit = units::precise::one;
  unit = unit * units::precise::m.pow(dimension.exponents[0]);
  unit = unit * units::precise::kg.pow(dimension.exponents[1]);
  unit = unit * units::precise::s.pow(dimension.exponents[2]);
  unit = unit * units::precise::A.pow(dimension.exponents[3]);
  unit = unit * units::precise::radian.pow(dimension.exponents[4]);
  return unit;
}

}  // namespace

units::precise_measurement From(double value, std::string_view unit) {
  auto parsed = detail::ParseUnit(unit);
  return units::precise_measurement{value, parsed.unit};
}

double In(const units::precise_measurement& measurement, std::string_view unit) {
  auto parsed = detail::ParseUnit(unit);
  if (MeasurementDimension(measurement) != parsed.dimension) {
    throw std::invalid_argument("requested unit has incompatible dimension");
  }
  return measurement.value_as(parsed.unit);
}

Dimension MeasurementDimension(const units::precise_measurement& measurement) {
  return detail::DimensionFor(measurement.units());
}

double SiValue(const units::precise_measurement& measurement) {
  return measurement.value() * measurement.units().multiplier();
}

units::precise_measurement MakeMeasurement(double value, const Dimension& dimension) {
  return units::precise_measurement{value, UnitForDimension(dimension)};
}

}  // namespace wpilib::expression
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

Quantity Quantity::From(double value, std::string_view unit) {
  auto parsed = detail::ParseUnit(unit);
  return Quantity{units::precise_measurement{value, parsed.unit}, parsed.dimension};
}

double Quantity::In(std::string_view unit) const {
  auto parsed = detail::ParseUnit(unit);
  if (dimension != parsed.dimension) {
    throw std::invalid_argument("requested unit has incompatible dimension");
  }
  return measurement_.value_as(parsed.unit);
}

Quantity::Quantity(double value, Dimension dimension)
    : siValue(value), dimension(dimension),
      measurement_{value, UnitForDimension(dimension)} {}

Quantity::Quantity(units::precise_measurement measurement, Dimension dimension)
    : siValue(measurement.value() * measurement.units().multiplier()),
      dimension(dimension),
      measurement_(std::move(measurement)) {}

}  // namespace wpilib::expression
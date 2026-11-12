#include "wpilib-expression/quantity.hpp"

#include "units.hpp"

#include <stdexcept>
#include <utility>

namespace wpilib::expression {
units::precise_measurement From(double value, std::string_view unit) {
  auto parsed = detail::ParseUnit(unit);
  return units::precise_measurement{value, parsed};
}

double In(const units::precise_measurement& measurement, std::string_view unit) {
  auto parsed = detail::ParseUnit(unit);
  if (measurement.units().base_units() != parsed.base_units()) {
    throw std::invalid_argument("requested unit has incompatible dimension");
  }
  return measurement.value_as(parsed);
}

double SiValue(const units::precise_measurement& measurement) {
  return measurement.value() * measurement.units().multiplier();
}

units::precise_measurement MakeMeasurement(double value, const units::precise_unit& unit) {
  return units::precise_measurement{value, unit};
}

}  // namespace wpilib::expression
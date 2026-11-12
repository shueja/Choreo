#include "wpilib-expression/quantity.hpp"

#include "units.hpp"

#include <stdexcept>

namespace wpilib::expression {
units::precise_measurement From(double value, const units::precise_unit& unit) {
  return units::precise_measurement{value, unit};
}

double In(const units::precise_measurement& measurement, const units::precise_unit& unit) {
  if (measurement.units().base_units() != unit.base_units()) {
    throw std::invalid_argument("requested unit has incompatible dimension");
  }
  return measurement.value_as(unit);
}

}  // namespace wpilib::expression
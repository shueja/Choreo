#include "wpilib-expression/quantity.hpp"

#include "units.hpp"

namespace wpilib::expression {
units::precise_measurement From(double value, const units::precise_unit& unit) {
  return units::precise_measurement{value, unit};
}

std::expected<double, Error> In(const units::precise_measurement& measurement,
                                const units::precise_unit& unit) {
  if (measurement.units().base_units() != unit.base_units()) {
    return std::unexpected(IncompatibleDimensionError{});
  }
  return measurement.value_as(unit);
}

}  // namespace wpilib::expression
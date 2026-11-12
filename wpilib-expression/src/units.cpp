#include "units.hpp"

#include <cctype>
#include <stdexcept>

namespace wpilib::expression::detail {

Dimension MakeDimension(int length, int mass, int time, int current, int angle) {
  return Dimension{{length, mass, time, current, angle}};
}

Dimension DimensionFor(const units::precise_unit& unit) {
  const auto base = unit.base_units();
  return MakeDimension(base.meter(), base.kg(), base.second(), base.ampere(), base.radian());
}

Dimension Combine(const Dimension& left, const Dimension& right, int sign) {
  Dimension result;
  for (std::size_t index = 0; index < result.exponents.size(); ++index) {
    result.exponents[index] = left.exponents[index] + sign * right.exponents[index];
  }
  return result;
}

const std::map<std::string, Unit>& UnitRegistry() {
  static const Dimension length = MakeDimension(1, 0, 0, 0, 0);
  static const Dimension mass = MakeDimension(0, 1, 0, 0, 0);
  static const Dimension time = MakeDimension(0, 0, 1, 0, 0);
  static const Dimension current = MakeDimension(0, 0, 0, 1, 0);
  static const Dimension angle = MakeDimension(0, 0, 0, 0, 1);

  static const Dimension area = ScaleDimension(length, 2);
  static const Dimension frequency = ScaleDimension(time, -1);
  static const Dimension force = Combine(Combine(length, mass, 1), ScaleDimension(time, 2), -1);
  static const Dimension torqueAndEnergy = Combine(force, length, 1);
  static const Dimension power = Combine(torqueAndEnergy, time, -1);
  static const Dimension voltage = Combine(power, current, -1);
  static const Dimension charge = Combine(current, time, 1);
  static const Dimension pressure = Combine(force, area, -1);
  static const Dimension resistance = Combine(voltage, current, -1);
  static const Dimension angularSpeed = Combine(angle, time, -1);

  static const std::map<std::string, Unit> units = [] {
    const std::map<std::string, units::precise_unit> definitions{
        {"m", units::precise::m}, {"meter", units::precise::m},
        {"mm", units::milli * units::precise::m}, {"cm", units::centi * units::precise::m},
        {"km", units::kilo * units::precise::m}, {"in", units::precise::in},
        {"inch", units::precise::in}, {"ft", units::precise::ft}, {"yd", units::precise::yd},
        {"mi", units::precise::mi}, {"s", units::precise::s},
        {"ms", units::milli * units::precise::s}, {"us", units::micro * units::precise::s},
        {"min", units::precise::min}, {"hr", units::precise::hr}, {"kg", units::precise::kg},
        {"g", units::gram}, {"lb", units::precise::lb}, {"A", units::precise::A},
        {"mA", units::milli * units::precise::A}, {"rad", units::precise::radian},
        {"radian", units::precise::radian},
        {"deg", units::precise::degree}, {"degree", units::precise::degree},
        {"turn", units::precise::turn}, {"rev", units::precise::turn},
        {"N", units::precise::N}, {"lbf", units::precise::lbf}, {"V", units::precise::V},
        {"mV", units::milli * units::precise::V}, {"kV", units::kilo * units::precise::V},
        {"Hz", units::precise::Hz}, {"kHz", units::kilo * units::precise::Hz},
        {"W", units::precise::W}, {"kW", units::kilo * units::precise::W},
        {"J", units::precise::J}, {"kJ", units::kilo * units::precise::J},
        {"C", units::precise::C}, {"Pa", units::precise::Pa},
        {"kPa", units::kilo * units::precise::Pa}, {"psi", units::precise::psi},
        {"ohm", units::precise::ohm}, {"Ohm", units::precise::ohm},
        {"Nm", units::precise::N * units::precise::m},
        {"RPM", units::precise::rpm}, {"rpm", units::precise::rpm}};
    std::map<std::string, Unit> result;
    for (const auto& [name, unit] : definitions) {
      result.emplace(name, Unit{unit, DimensionFor(unit)});
    }
    return result;
  }();
  return units;
}

Dimension ScaleDimension(const Dimension& dimension, int exponent) {
  Dimension result;
  for (std::size_t index = 0; index < result.exponents.size(); ++index) {
    result.exponents[index] = dimension.exponents[index] * exponent;
  }
  return result;
}

Unit ParseUnit(std::string_view expression) {
  auto unit = units::unit_from_string(std::string(expression));
  if (units::is_error(unit)) {
    throw std::invalid_argument("unknown or malformed unit: " + std::string(expression));
  }
  return Unit{unit, DimensionFor(unit)};
}

}  // namespace wpilib::expression::detail
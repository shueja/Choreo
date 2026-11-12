#include "units.hpp"

#include <cctype>
#include <stdexcept>

namespace wpilib::expression::detail {

const std::map<std::string, Unit>& UnitRegistry() {
  static const std::map<std::string, Unit> units = [] {
    const std::map<std::string, std::string> definitions{
        {"m", "m"}, {"meter", "m"}, {"mm", "mm"}, {"cm", "cm"}, {"km", "km"},
        {"in", "in"}, {"inch", "in"}, {"ft", "ft"}, {"yd", "yd"}, {"mi", "mi"},
        {"s", "s"}, {"ms", "ms"}, {"us", "us"}, {"min", "min"}, {"hr", "hr"},
        {"kg", "kg"}, {"g", "g"}, {"lb", "lb"}, {"A", "A"}, {"mA", "mA"},
        {"rad", "rad"}, {"radian", "rad"}, {"deg", "deg"}, {"degree", "deg"},
        {"turn", "turn"}, {"rev", "turn"}, {"N", "N"}, {"lbf", "lbf"}, {"V", "V"},
        {"mV", "mV"}, {"kV", "kV"}, {"Hz", "Hz"}, {"kHz", "kHz"}, {"W", "W"},
        {"kW", "kW"}, {"J", "J"}, {"kJ", "kJ"}, {"C", "C"}, {"Pa", "Pa"},
        {"kPa", "kPa"}, {"psi", "psi"}, {"ohm", "ohm"}, {"Ohm", "ohm"},
        {"Nm", "N*m"}, {"RPM", "rpm"}, {"rpm", "rpm"}};
    std::map<std::string, Unit> result;
    for (const auto& [name, spelling] : definitions) {
      auto unit = units::unit_from_string(spelling);
      result.emplace(name, Unit{unit});
    }
    return result;
  }();
  return units;
}

Unit ParseUnit(std::string_view expression) {
  auto unit = units::unit_from_string(std::string(expression));
  if (units::is_error(unit)) {
    throw std::invalid_argument("unknown or malformed unit: " + std::string(expression));
  }
  return Unit{unit};
}

}  // namespace wpilib::expression::detail
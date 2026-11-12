#include "wpilib-expression/quantity.hpp"

#include "units.hpp"

#include <stdexcept>

namespace wpilib::expression {

Quantity Quantity::From(double value, std::string_view unit) {
  auto parsed = detail::ParseUnit(unit);
  Quantity result{value * parsed.scale, parsed.dimension};
  detail::RequireFinite(result);
  return result;
}

double Quantity::In(std::string_view unit) const {
  auto parsed = detail::ParseUnit(unit);
  if (dimension != parsed.dimension) {
    throw std::invalid_argument("requested unit has incompatible dimension");
  }
  return siValue / parsed.scale;
}

}  // namespace wpilib::expression
#pragma once

#include "wpilib-expression/quantity.hpp"

#include <map>
#include <expected>
#include <string>
#include <string_view>

namespace wpilib::expression::detail {

const std::map<std::string, units::precise_unit>& UnitRegistry();
std::expected<units::precise_unit, Error> ParseUnit(std::string_view expression);

}  // namespace wpilib::expression::detail
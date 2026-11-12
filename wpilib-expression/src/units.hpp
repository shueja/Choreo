#pragma once

#include "wpilib-expression/quantity.hpp"

#include <map>
#include <string>
#include <string_view>

namespace wpilib::expression::detail {

struct Unit {
  units::precise_unit unit;
};

const std::map<std::string, Unit>& UnitRegistry();
Unit ParseUnit(std::string_view expression);

}  // namespace wpilib::expression::detail
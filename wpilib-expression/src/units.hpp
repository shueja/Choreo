#pragma once

#include "wpilib-expression/quantity.hpp"

#include <map>
#include <string>
#include <string_view>

namespace wpilib::expression::detail {

struct Unit {
  double scale;
  Dimension dimension;
};

Dimension MakeDimension(int length, int mass, int time, int current, int angle);
Dimension Combine(const Dimension& left, const Dimension& right, int sign);
Dimension ScaleDimension(const Dimension& dimension, int exponent);
const std::map<std::string, Unit>& UnitRegistry();
Unit ParseUnit(std::string_view expression);
void RequireFinite(const Quantity& quantity);

}  // namespace wpilib::expression::detail
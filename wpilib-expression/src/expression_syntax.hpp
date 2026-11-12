#pragma once

#include <string>
#include <string_view>

namespace wpilib::expression::detail {

std::string ExpandUnitSuffixes(std::string_view source);

}  // namespace wpilib::expression::detail
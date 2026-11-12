#pragma once

#include <string>
#include <string_view>

namespace wpilib::expression::detail {

std::string ExpandUnitSuffixes(std::string_view source);
std::string RenameVariableReferences(std::string_view source, std::string_view oldName,
									 std::string_view newName);

}  // namespace wpilib::expression::detail
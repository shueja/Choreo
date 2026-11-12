#pragma once

#include <string>
#include <type_traits>
#include <variant>

namespace wpilib::expression {

struct IncompatibleDimensionError {};
struct InvalidUnitError {
  std::string expression;
};

using Error = std::variant<IncompatibleDimensionError, InvalidUnitError>;

inline std::string ErrorMessage(const Error& error) {
  return std::visit(
      [](const auto& value) -> std::string {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, IncompatibleDimensionError>) {
          return "incompatible unit dimension";
        } else {
          return "unknown or malformed unit: " + value.expression;
        }
      },
      error);
}

}  // namespace wpilib::expression

#include "expression_syntax.hpp"

#include "units.hpp"

#include <cctype>
#include <stdexcept>
#include <string>

namespace wpilib::expression::detail {

std::string ExpandUnitSuffixes(std::string_view source) {
  std::string expanded;
  expanded.reserve(source.size() + 16);

  for (std::size_t position = 0; position < source.size();) {
    if (source[position] == '#' || source.substr(position, 2) == "//") {
      auto end = source.find('\n', position);
      if (end == std::string_view::npos) {
        end = source.size();
      }
      expanded.append(source.substr(position, end - position));
      position = end;
      continue;
    }
    if (source.substr(position, 2) == "/*") {
      auto end = source.find("*/", position + 2);
      end = end == std::string_view::npos ? source.size() : end + 2;
      expanded.append(source.substr(position, end - position));
      position = end;
      continue;
    }
    if (source.substr(position, 2) == "R\"") {
      auto open = source.find('(', position + 2);
      if (open != std::string_view::npos) {
        auto closing = ")" + std::string(source.substr(position + 2, open - position - 2)) + "\"";
        auto end = source.find(closing, open + 1);
        end = end == std::string_view::npos ? source.size() : end + closing.size();
        expanded.append(source.substr(position, end - position));
        position = end;
        continue;
      }
    }
    if (source[position] == '"' || source[position] == '\'') {
      auto start = position;
      char quote = source[position++];
      bool escaped = false;
      while (position < source.size()) {
        char character = source[position++];
        if (!escaped && character == quote) {
          break;
        }
        if (!escaped && character == '\\') {
          escaped = true;
        } else {
          escaped = false;
        }
      }
      expanded.append(source.substr(start, position - start));
      continue;
    }
    unsigned char current = static_cast<unsigned char>(source[position]);
    bool followsIdentifier = position > 0 &&
                             (std::isalnum(static_cast<unsigned char>(source[position - 1])) ||
                              source[position - 1] == '_');
    bool startsNumber = !followsIdentifier &&
                        (std::isdigit(current) ||
                         (source[position] == '.' && position + 1 < source.size() &&
                          std::isdigit(static_cast<unsigned char>(source[position + 1]))));
    if (!startsNumber) {
      expanded.push_back(source[position++]);
      continue;
    }

    std::size_t numberEnd = position;
    while (numberEnd < source.size() &&
           std::isdigit(static_cast<unsigned char>(source[numberEnd]))) {
      ++numberEnd;
    }
    if (numberEnd < source.size() && source[numberEnd] == '.') {
      ++numberEnd;
      while (numberEnd < source.size() &&
             std::isdigit(static_cast<unsigned char>(source[numberEnd]))) {
        ++numberEnd;
      }
    }
    if (numberEnd < source.size() &&
        (source[numberEnd] == 'e' || source[numberEnd] == 'E')) {
      std::size_t exponentEnd = numberEnd + 1;
      if (exponentEnd < source.size() &&
          (source[exponentEnd] == '+' || source[exponentEnd] == '-')) {
        ++exponentEnd;
      }
      std::size_t digitsStart = exponentEnd;
      while (exponentEnd < source.size() &&
             std::isdigit(static_cast<unsigned char>(source[exponentEnd]))) {
        ++exponentEnd;
      }
      if (exponentEnd > digitsStart) {
        numberEnd = exponentEnd;
      }
    }

    auto number = source.substr(position, numberEnd - position);
    std::size_t suffixStart = numberEnd;
    while (suffixStart < source.size() &&
           std::isspace(static_cast<unsigned char>(source[suffixStart]))) {
      ++suffixStart;
    }
    if (suffixStart < source.size() &&
        (std::isalpha(static_cast<unsigned char>(source[suffixStart])) ||
         source[suffixStart] == '_')) {
      std::size_t suffixEnd = suffixStart + 1;
      while (suffixEnd < source.size() &&
             (std::isalnum(static_cast<unsigned char>(source[suffixEnd])) ||
              source[suffixEnd] == '_')) {
        ++suffixEnd;
      }
      auto suffix = source.substr(suffixStart, suffixEnd - suffixStart);
      if (!UnitRegistry().contains(std::string(suffix))) {
        throw std::invalid_argument("unknown unit suffix '" + std::string(suffix) +
                                    "' at character " + std::to_string(suffixStart));
      }

      std::size_t unitEnd = suffixEnd;
      while (unitEnd < source.size()) {
        std::size_t next = unitEnd;
        while (next < source.size() &&
               std::isspace(static_cast<unsigned char>(source[next]))) {
          ++next;
        }
        if (next == source.size() || (source[next] != '*' && source[next] != '/')) {
          break;
        }

        ++next;
        while (next < source.size() &&
               std::isspace(static_cast<unsigned char>(source[next]))) {
          ++next;
        }
        std::size_t symbolEnd = next;
        while (symbolEnd < source.size() &&
               (std::isalnum(static_cast<unsigned char>(source[symbolEnd])) ||
                source[symbolEnd] == '_')) {
          ++symbolEnd;
        }
        if (symbolEnd == next ||
            !UnitRegistry().contains(std::string(source.substr(next, symbolEnd - next)))) {
          break;
        }

        if (symbolEnd < source.size() && source[symbolEnd] == '^') {
          ++symbolEnd;
          if (symbolEnd < source.size() &&
              (source[symbolEnd] == '+' || source[symbolEnd] == '-')) {
            ++symbolEnd;
          }
          while (symbolEnd < source.size() &&
                 std::isdigit(static_cast<unsigned char>(source[symbolEnd]))) {
            ++symbolEnd;
          }
        }
        unitEnd = symbolEnd;
      }

      auto unitExpression = source.substr(suffixStart, unitEnd - suffixStart);
      ParseUnit(unitExpression);
      expanded += "quantity(";
      expanded.append(number);
      expanded += ", \"";
      expanded.append(unitExpression);
      expanded += "\")";
      position = unitEnd;
    } else {
      expanded.append(number);
      expanded.append(source.substr(numberEnd, suffixStart - numberEnd));
      position = suffixStart;
    }
  }

  return expanded;
}

}  // namespace wpilib::expression::detail
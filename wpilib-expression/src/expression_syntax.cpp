#include "expression_syntax.hpp"

#include "units.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <vector>

namespace wpilib::expression::detail {

std::string RenameQualifiedVariableReferences(std::string_view source,
                                              std::string_view qualifiedName,
                                              std::string_view newName) {
  auto isStart = [](char character) {
    return std::isalpha(static_cast<unsigned char>(character)) ||
           character == '_';
  };
  auto isPart = [](char character) {
    return std::isalnum(static_cast<unsigned char>(character)) ||
           character == '_';
  };
  auto isIdentifier = [&](std::string_view name) {
    if (name.empty() || !isStart(name.front())) {
      return false;
    }
    for (char character : name.substr(1)) {
      if (!isPart(character)) {
        return false;
      }
    }
    return true;
  };
  if (!isIdentifier(newName)) {
    throw std::invalid_argument("invalid variable name");
  }
  std::vector<std::string_view> parts;
  for (std::size_t start = 0; start <= qualifiedName.size();) {
    auto end = qualifiedName.find('.', start);
    auto part = qualifiedName.substr(
        start, end == std::string_view::npos ? end : end - start);
    if (!isIdentifier(part)) {
      throw std::invalid_argument("invalid qualified variable name");
    }
    parts.push_back(part);
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }

  struct Token {
    std::size_t start;
    std::size_t length;
    std::string_view text;
  };
  std::vector<Token> tokens;
  for (std::size_t position = 0; position < source.size();) {
    const auto start = position;
    if (std::isspace(static_cast<unsigned char>(source[position]))) {
      ++position;
      continue;
    }
    if (source[position] == '#' || source.substr(position, 2) == "//") {
      position = source.find('\n', position);
      if (position == std::string_view::npos) {
        position = source.size();
      }
      continue;
    }
    if (source.substr(position, 2) == "/*") {
      auto end = source.find("*/", position + 2);
      position = end == std::string_view::npos ? source.size() : end + 2;
      continue;
    }
    if (source.substr(position, 2) == "R\"") {
      auto open = source.find('(', position + 2);
      if (open == std::string_view::npos) {
        position = source.size();
      } else {
        auto closing =
            ")" +
            std::string(source.substr(position + 2, open - position - 2)) +
            "\"";
        auto end = source.find(closing, open + 1);
        position = end == std::string_view::npos ? source.size()
                                                 : end + closing.size();
      }
    } else if (source[position] == '"' || source[position] == '\'') {
      const char quote = source[position++];
      bool escaped = false;
      while (position < source.size()) {
        const char character = source[position++];
        if (!escaped && character == quote) {
          break;
        }
        escaped = !escaped && character == '\\';
      }
    } else if (std::isdigit(static_cast<unsigned char>(source[position])) ||
               (source[position] == '.' && position + 1 < source.size() &&
                std::isdigit(
                    static_cast<unsigned char>(source[position + 1])))) {
      while (position < source.size() &&
             std::isdigit(static_cast<unsigned char>(source[position]))) {
        ++position;
      }
      if (position < source.size() && source[position] == '.') {
        ++position;
        while (position < source.size() &&
               std::isdigit(static_cast<unsigned char>(source[position]))) {
          ++position;
        }
      }
      if (position < source.size() &&
          (source[position] == 'e' || source[position] == 'E')) {
        auto exponent = position + 1;
        if (exponent < source.size() &&
            (source[exponent] == '+' || source[exponent] == '-')) {
          ++exponent;
        }
        if (exponent < source.size() &&
            std::isdigit(static_cast<unsigned char>(source[exponent]))) {
          position = exponent + 1;
          while (position < source.size() &&
                 std::isdigit(static_cast<unsigned char>(source[position]))) {
            ++position;
          }
        }
      }
    } else if (isStart(source[position])) {
      ++position;
      while (position < source.size() && isPart(source[position])) {
        ++position;
      }
    } else if (source.substr(position, 2) == "::") {
      position += 2;
    } else {
      ++position;
    }
    tokens.push_back(
        {start, position - start, source.substr(start, position - start)});
  }

  std::string result;
  std::size_t copied = 0;
  for (std::size_t index = 0; index < tokens.size(); ++index) {
    if (tokens[index].text != parts.front() ||
        (index > 0 &&
         (tokens[index - 1].text == "." || tokens[index - 1].text == "::"))) {
      continue;
    }
    std::size_t match = index;
    bool found = true;
    for (std::size_t part = 1; part < parts.size(); ++part) {
      if (match + 2 >= tokens.size() ||
          (tokens[match + 1].text != "." && tokens[match + 1].text != "::") ||
          tokens[match + 2].text != parts[part]) {
        found = false;
        break;
      }
      match += 2;
    }
    if (!found ||
        (match + 1 < tokens.size() && tokens[match + 1].text == "(")) {
      continue;
    }
    result.append(source.substr(copied, tokens[match].start - copied));
    result.append(newName);
    copied = tokens[match].start + tokens[match].length;
    index = match;
  }
  result.append(source.substr(copied));
  return result;
}

std::string RenameVariableReferences(std::string_view source,
                                     std::string_view oldName,
                                     std::string_view newName) {
  std::string result;
  result.reserve(source.size());
  bool memberAccess = false;
  for (std::size_t position = 0; position < source.size();) {
    auto start = position;
    if (source[position] == '#' || source.substr(position, 2) == "//") {
      position = source.find('\n', position);
      if (position == std::string_view::npos) {
        position = source.size();
      }
    } else if (source.substr(position, 2) == "/*") {
      position = source.find("*/", position + 2);
      position = position == std::string_view::npos ? source.size() : position + 2;
    } else if (source.substr(position, 2) == "R\"") {
      auto open = source.find('(', position + 2);
      if (open == std::string_view::npos) {
        position = source.size();
      } else {
        auto closing = ")" + std::string(source.substr(position + 2, open - position - 2)) + "\"";
        auto end = source.find(closing, open + 1);
        position = end == std::string_view::npos ? source.size() : end + closing.size();
      }
      memberAccess = false;
    } else if (source[position] == '"' || source[position] == '\'') {
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
      memberAccess = false;
    } else if ((std::isalpha(static_cast<unsigned char>(source[position])) ||
                source[position] == '_') &&
               (position == 0 ||
                !(std::isalnum(static_cast<unsigned char>(source[position - 1])) ||
                  source[position - 1] == '_'))) {
      ++position;
      while (position < source.size() &&
             (std::isalnum(static_cast<unsigned char>(source[position])) ||
              source[position] == '_')) {
        ++position;
      }
      auto next = position;
      while (next < source.size()) {
        if (std::isspace(static_cast<unsigned char>(source[next]))) {
          ++next;
        } else if (source[next] == '#' || source.substr(next, 2) == "//") {
          next = source.find('\n', next);
          if (next == std::string_view::npos) {
            next = source.size();
          }
        } else if (source.substr(next, 2) == "/*") {
          auto end = source.find("*/", next + 2);
          next = end == std::string_view::npos ? source.size() : end + 2;
        } else {
          break;
        }
      }
      if (!memberAccess && source.substr(start, position - start) == oldName &&
          (next == source.size() || source[next] != '(')) {
        result.append(newName);
        memberAccess = false;
        continue;
      }
      memberAccess = false;
    } else if (std::isspace(static_cast<unsigned char>(source[position]))) {
      ++position;
    } else if (source[position] == '.' || source.substr(position, 2) == "::") {
      position += source[position] == '.' ? 1 : 2;
      memberAccess = true;
    } else {
      ++position;
      memberAccess = false;
    }
    result.append(source.substr(start, position - start));
  }
  return result;
}

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
    // Support underscores in identifiers.
    if (std::isalpha(current) || source[position] == '_') {
      std::size_t identifierEnd = position + 1;
      while (identifierEnd < source.size() &&
             (std::isalnum(static_cast<unsigned char>(source[identifierEnd])) ||
              source[identifierEnd] == '_')) {
        ++identifierEnd;
      }
      // Expand known identifiers like min and max to their full forms to avoid conflicts with user-defined functions.
      auto identifier = source.substr(position, identifierEnd - position);
      std::size_t next = identifierEnd;
      while (next < source.size() &&
             std::isspace(static_cast<unsigned char>(source[next]))) {
        ++next;
      }
      if (next < source.size() && source[next] == '(' && identifier == "min") {
        expanded += "minimum";
      } else if (next < source.size() && source[next] == '(' && identifier == "max") {
        expanded += "maximum";
      } else {
        expanded.append(identifier);
      }
      position = identifierEnd;
      continue;
    }
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
      if (unitEnd < source.size() && source[unitEnd] == '^') {
        std::size_t exponentEnd = unitEnd + 1;
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
          unitEnd = exponentEnd;
        }
      }
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
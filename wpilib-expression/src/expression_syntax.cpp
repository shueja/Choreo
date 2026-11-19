#include "expression_syntax.hpp"

#include "units.hpp"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace wpilib::expression::detail {

namespace {
namespace variable_rename {

/// Returns whether a character can begin an identifier.
/// @param character Character to check.
/// @return True when it may begin an identifier.
bool IsIdentifierStart(char character) {
  return std::isalpha(static_cast<unsigned char>(character)) ||
         character == '_';
}

/// Returns whether a character can continue an identifier.
/// @param character Character to check.
/// @return True when it may continue an identifier.
bool IsIdentifierPart(char character) {
  return std::isalnum(static_cast<unsigned char>(character)) ||
         character == '_';
}

/// Checks that a complete string follows the expression identifier rules.
/// @param name Candidate identifier.
/// @return True when the complete string is a valid identifier.
bool IsIdentifier(std::string_view name) {
  if (name.empty() || !IsIdentifierStart(name.front())) {
    return false;
  }
  for (char character : name.substr(1)) {
    if (!IsIdentifierPart(character)) {
      return false;
    }
  }
  return true;
}

/// Validates the replacement and splits a dotted target name into identifiers.
/// Source references may use either dots or scope operators between parts.
/// @param qualifiedName Dotted name of the variable to find.
/// @param newName Replacement identifier.
/// @return The target name's identifier components.
std::vector<std::string_view> SplitQualifiedName(
    std::string_view qualifiedName, std::string_view newName) {
  if (!IsIdentifier(newName)) {
    throw std::invalid_argument("invalid variable name");
  }

  std::vector<std::string_view> parts;
  for (std::size_t start = 0; start <= qualifiedName.size();) {
    auto end = qualifiedName.find('.', start);
    auto part = qualifiedName.substr(
        start, end == std::string_view::npos ? end : end - start);
    if (!IsIdentifier(part)) {
      throw std::invalid_argument("invalid qualified variable name");
    }
    parts.push_back(part);
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return parts;
}

/// Stores a token's source span and spelling for matching and exact rewriting.
struct Token {
  std::size_t start;
  std::size_t length;
  std::string_view text;
};

/// Returns the first offset after a quoted string or character literal.
/// @param source Source text containing the literal.
/// @param position Offset of the opening quote.
/// @return Offset immediately after the literal or at the end of source.
std::size_t SkipQuotedLiteral(std::string_view source, std::size_t position) {
  const char quote = source[position++];
  bool escaped = false;
  while (position < source.size()) {
    const char character = source[position++];
    if (!escaped && character == quote) {
      break;
    }
    escaped = !escaped && character == '\\';
  }
  return position;
}

/// Returns the first offset after a raw string with its matching delimiter.
/// @param source Source text containing the raw string.
/// @param position Offset of the raw string prefix.
/// @return Offset immediately after the literal or at the end of source.
std::size_t SkipRawString(std::string_view source, std::size_t position) {
  auto open = source.find('(', position + 2);
  if (open == std::string_view::npos) {
    return source.size();
  }
  auto closing = ")" +
                 std::string(source.substr(position + 2, open - position - 2)) +
                 "\"";
  auto end = source.find(closing, open + 1);
  return end == std::string_view::npos ? source.size() : end + closing.size();
}

/// Returns the first offset after a decimal or scientific-notation number.
/// @param source Source text containing the number.
/// @param position Offset of the first digit or decimal point.
/// @return Offset immediately after the number.
std::size_t SkipNumber(std::string_view source, std::size_t position) {
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
  return position;
}

/// Tokenizes identifiers and separators while excluding comments and literals.
/// Retained source spans let replacements preserve all intervening text.
/// @param source Expression text to scan.
/// @return Tokens with offsets into the original source.
std::vector<Token> TokenizeSource(std::string_view source) {
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
      position = SkipRawString(source, position);
    } else if (source[position] == '"' || source[position] == '\'') {
      position = SkipQuotedLiteral(source, position);
    } else if (std::isdigit(static_cast<unsigned char>(source[position])) ||
               (source[position] == '.' && position + 1 < source.size() &&
                std::isdigit(
                    static_cast<unsigned char>(source[position + 1])))) {
      position = SkipNumber(source, position);
    } else if (IsIdentifierStart(source[position])) {
      ++position;
      while (position < source.size() && IsIdentifierPart(source[position])) {
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
  return tokens;
}

/// Finds a whole qualified variable reference and returns its final token.
/// Rejects member accesses such as `object.name` and callable names like `name(...)`.
/// @param tokens Tokenized source.
/// @param start Candidate first-token index.
/// @param parts Identifier components of the target name.
/// @param end Receives the final-token index when a match is found.
/// @return True if the tokens at start form a whole variable reference.
bool FindQualifiedMatch(const std::vector<Token>& tokens,
                        std::size_t start,
                        const std::vector<std::string_view>& parts,
                        std::size_t& end) {
  if (tokens[start].text != parts.front() ||
      (start > 0 &&
       (tokens[start - 1].text == "." || tokens[start - 1].text == "::"))) {
    return false;
  }
  end = start;
  for (std::size_t part = 1; part < parts.size(); ++part) {
    if (end + 2 >= tokens.size() ||
        (tokens[end + 1].text != "." && tokens[end + 1].text != "::") ||
        tokens[end + 2].text != parts[part]) {
      return false;
    }
    end += 2;
  }
  return end + 1 >= tokens.size() || tokens[end + 1].text != "(";
}

/// Determines whether replacement needs a space to avoid forming a number.
/// @param tokens Tokenized source.
/// @param start First-token index of the match.
/// @param end Final-token index of the match.
/// @param newName Replacement identifier.
/// @return True when a separating space is required.
bool NeedsSeparator(const std::vector<Token>& tokens, std::size_t start,
                    std::size_t end, std::string_view newName) {
  if (start != end || start == 0 || tokens[start - 1].text.empty() ||
      tokens[start - 1].start + tokens[start - 1].length !=
          tokens[start].start) {
    return false;
  }
  const auto previous = tokens[start - 1].text.front();
  return (std::isdigit(static_cast<unsigned char>(previous)) ||
          previous == '.') &&
         newName.size() > 1 &&
         (newName.front() == 'e' || newName.front() == 'E') &&
         std::isdigit(static_cast<unsigned char>(newName[1]));
}

/// Replaces each matched reference while copying every unmatched source span.
/// @param source Original expression text.
/// @param tokens Tokenized source with original offsets.
/// @param parts Identifier components of the target name.
/// @param newName Replacement identifier.
/// @return Source with whole matching references replaced.
std::string ReplaceMatches(
  std::string_view source, const std::vector<Token>& tokens,
    const std::vector<std::string_view>& parts, std::string_view newName) {
  std::string result;
  std::size_t copied = 0;
  for (std::size_t start = 0; start < tokens.size(); ++start) {
    std::size_t end = start;
    if (!FindQualifiedMatch(tokens, start, parts, end)) {
      continue;
    }
    result.append(source.substr(copied, tokens[end].start - copied));
    if (NeedsSeparator(tokens, start, end, newName)) {
      result += ' ';
    }
    result.append(newName);
    copied = tokens[end].start + tokens[end].length;
    start = end;
  }
  result.append(source.substr(copied));
  return result;
}

}  // namespace variable_rename
}  // namespace

std::string RenameQualifiedVariableReferences(std::string_view source,
                                              std::string_view qualifiedName,
                                              std::string_view newName) {
  const auto parts = variable_rename::SplitQualifiedName(qualifiedName, newName);
  const auto tokens = variable_rename::TokenizeSource(source);
  return variable_rename::ReplaceMatches(source, tokens, parts, newName);
}

std::string RenameVariableReferences(std::string_view source,
                                     std::string_view oldName,
                                     std::string_view newName) {
  return RenameQualifiedVariableReferences(source, oldName, newName);
}

namespace {

enum class TokenKind { kSymbol, kNumber, kLiteral, kPunctuation, kBoundary, kEnd };

struct Token {
  TokenKind kind;
  std::string_view prefix;
  std::string_view text;
};

bool IsDigit(char character) {
  return std::isdigit(static_cast<unsigned char>(character));
}

bool IsSymbolStart(char character) {
  return std::isalpha(static_cast<unsigned char>(character)) || character == '_';
}

std::vector<Token> Tokenize(std::string_view source) {
  std::vector<Token> tokens;
  std::size_t position = 0;
  for (;;) {
    auto prefixStart = position;
    while (position < source.size() && source[position] != '\n' &&
           std::isspace(static_cast<unsigned char>(source[position]))) {
      ++position;
    }
    auto prefix = source.substr(prefixStart, position - prefixStart);
    if (position == source.size()) {
      tokens.push_back({TokenKind::kEnd, prefix, {}});
      return tokens;
    }
    auto start = position;
    auto kind = TokenKind::kPunctuation;
    if (source[position] == '#' || source.substr(position, 2) == "//") {
      kind = TokenKind::kBoundary;
      position = source.find('\n', position);
      if (position == std::string_view::npos) {
        position = source.size();
      }
    } else if (source.substr(position, 2) == "/*") {
      kind = TokenKind::kBoundary;
      auto end = source.find("*/", position + 2);
      position = end == std::string_view::npos ? source.size() : end + 2;
    } else if (source.substr(position, 2) == "R\"") {
      kind = TokenKind::kLiteral;
      auto open = source.find('(', position + 2);
      if (open == std::string_view::npos) {
        position = source.size();
      } else {
        auto closing = ")" + std::string(source.substr(position + 2, open - position - 2)) + "\"";
        auto end = source.find(closing, open + 1);
        position = end == std::string_view::npos ? source.size() : end + closing.size();
      }
    } else if (source[position] == '"' || source[position] == '\'') {
      kind = TokenKind::kLiteral;
      char quote = source[position++];
      while (position < source.size()) {
        char character = source[position++];
        if (character == quote) {
          break;
        }
        if (character == '\\' && position < source.size()) {
          ++position;
        }
      }
    } else if (IsSymbolStart(source[position])) {
      kind = TokenKind::kSymbol;
      do {
        ++position;
      } while (position < source.size() &&
               (IsSymbolStart(source[position]) || IsDigit(source[position])));
    } else if (IsDigit(source[position]) ||
               (source[position] == '.' && position + 1 < source.size() &&
                IsDigit(source[position + 1]))) {
      kind = TokenKind::kNumber;
      while (position < source.size() && IsDigit(source[position])) {
        ++position;
      }
      if (position < source.size() && source[position] == '.') {
        ++position;
        while (position < source.size() && IsDigit(source[position])) {
          ++position;
        }
      }
      if (position < source.size() && (source[position] == 'e' || source[position] == 'E')) {
        auto exponent = position + 1;
        if (exponent < source.size() && (source[exponent] == '+' || source[exponent] == '-')) {
          ++exponent;
        }
        if (exponent < source.size() && IsDigit(source[exponent])) {
          position = exponent + 1;
          while (position < source.size() && IsDigit(source[position])) {
            ++position;
          }
        }
      }
    } else {
      if (source[position] == '\n') {
        kind = TokenKind::kBoundary;
      }
      auto pair = source.substr(position, 2);
      position += pair == "++" || pair == "--" ? 2 : 1;
    }
    tokens.push_back({kind, prefix, source.substr(start, position - start)});
  }
}

bool IsSymbol(const Token& token) {
  if (token.kind != TokenKind::kSymbol) {
    return false;
  }
  for (auto keyword : {"var", "auto", "global", "def", "fun", "return", "if", "else",
                       "for", "while", "switch", "case", "default", "break", "continue",
                       "try", "catch", "finally", "class", "attr"}) {
    if (token.text == keyword) {
      return false;
    }
  }
  return true;
}

enum class OperandKind { kNumber, kSignedNumber, kSymbol, kOther, kOperator };

struct Operand {
  std::string text;
  OperandKind kind;
};

class ImplicitMultiplication {
 public:
  explicit ImplicitMultiplication(std::string_view source) : m_tokens{Tokenize(source)} {}

  std::string Expand() {
    auto expression = Sequence({});
    expression.append(Peek().prefix);
    return expression;
  }

 private:
  const Token& Peek(std::size_t offset = 0) const {
    return m_tokens[std::min(m_position + offset, m_tokens.size() - 1)];
  }

  std::string Take(std::string_view replacement = {}) {
    const auto& token = m_tokens[m_position++];
    return std::string(token.prefix) + std::string(replacement.empty() ? token.text : replacement);
  }

  bool StartsAtom(const Token& token) const {
    return IsSymbol(token) || token.kind == TokenKind::kNumber ||
           token.kind == TokenKind::kLiteral || token.text == "(" || token.text == "[";
  }

  bool StartsUnary(const Token& token) const {
    return StartsAtom(token) || token.text == "+" || token.text == "-" || token.text == "!";
  }

  std::string Sequence(std::string_view closing) {
    std::string result;
    bool expectOperand = true;
    while (Peek().kind != TokenKind::kEnd && Peek().text != closing) {
      if (expectOperand && StartsUnary(Peek())) {
        result += Product();
        expectOperand = false;
      } else if (Peek().text == "(" || Peek().text == "[" || Peek().text == "{") {
        result += Group(Peek().text == "(" ? ")" : Peek().text == "[" ? "]" : "}");
        expectOperand = false;
      } else {
        expectOperand = !StartsAtom(Peek());
        result += Take();
      }
    }
    return result;
  }

  std::string Group(std::string_view closing) {
    auto result = Take();
    result += Sequence(closing);
    if (Peek().text == closing) {
      result += Take();
    }
    return result;
  }

  std::string Product() {
    auto first = Factor();
    auto last = first.kind;
    auto text = std::move(first.text);
    bool implicit = false;
    while (IsSymbol(Peek()) || Peek().text == "(" ||
           (Peek().kind == TokenKind::kNumber && last != OperandKind::kNumber &&
            last != OperandKind::kSignedNumber && last != OperandKind::kOperator)) {
      auto next = Factor();
      text += " * " + next.text;
      last = next.kind;
      implicit = true;
    }
    return implicit ? "(" + text + ")" : text;
  }

  Operand Factor() {
    auto result = Unary();
    bool fraction = false;
    while ((result.kind == OperandKind::kNumber || result.kind == OperandKind::kSignedNumber ||
            fraction) && Peek().text == "/" && Peek(1).kind == TokenKind::kNumber &&
           (IsSymbol(Peek(2)) || Peek(2).text == "(")) {
      result.text = "((1.0 * (" + result.text + "))" + Take() + " ";
      result.text += Take() + ")";
      result.kind = OperandKind::kOperator;
      fraction = true;
    }
    return result;
  }

  Operand Unary() {
    if ((Peek().text == "+" || Peek().text == "-" || Peek().text == "!") &&
        StartsUnary(Peek(1))) {
      bool signedNumber = Peek().text != "!";
      auto prefix = Take();
      auto operand = Unary();
      return {prefix + operand.text, signedNumber && operand.kind == OperandKind::kNumber
                                        ? OperandKind::kSignedNumber : OperandKind::kOperator};
    }
    auto base = Atom();
    if (Peek().text == "^" && StartsUnary(Peek(1))) {
      Take();
      return {"pow(" + base.text + ", " + Unary().text + ")", OperandKind::kOperator};
    }
    return base;
  }

  Operand Atom() {
    auto token = Peek();
    auto kind = OperandKind::kOther;
    bool callable = false;
    std::string text;
    if (token.kind == TokenKind::kSymbol) {
      kind = OperandKind::kSymbol;
      callable = true;
      if (Peek(1).text == "(" && (token.text == "min" || token.text == "max")) {
        text = Take(token.text == "min" ? "minimum" : "maximum");
      } else if (Peek(1).text != "(" && UnitRegistry().contains(std::string(token.text))) {
        text = Take("quantity(1, \"" + std::string(token.text) + "\")");
        callable = false;
      } else {
        text = Take();
      }
    } else if (token.text == "(" || token.text == "[") {
      text = Group(token.text == "(" ? ")" : "]");
    } else {
      if (token.kind == TokenKind::kNumber) {
        kind = OperandKind::kNumber;
      }
      text = Take();
    }
    for (;;) {
      if (Peek().text == "(" && callable) {
        text += Group(")");
        callable = false;
      } else if (Peek().text == "[") {
        text += Group("]");
        callable = true;
      } else if (Peek().text == "." && Peek(1).kind == TokenKind::kSymbol) {
        text += Take();
        text += Take();
        callable = true;
      } else {
        break;
      }
      kind = OperandKind::kOther;
    }
    return {std::move(text), kind};
  }

  std::vector<Token> m_tokens;
  std::size_t m_position = 0;
};

}  // namespace

std::string ExpandUnitSuffixes(std::string_view source) {
  return ImplicitMultiplication{source}.Expand();
}

}  // namespace wpilib::expression::detail
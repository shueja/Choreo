#include "wpilib-expression/expression.hpp"

#include "expression_engine.hpp"
#include "expression_syntax.hpp"
#include "units.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace wpilib::expression {
namespace {

using detail::Pose2d;
using detail::Translation2d;
using detail::Rotation2d;
using detail::Transform2d;
using detail::Twist2d;
using detail::Rectangle2d;
using detail::Ellipse2d;

struct CachedEngine {
  chaiscript::ChaiScript engine{{}, {},
                               {chaiscript::Options::No_Load_Modules,
                                chaiscript::Options::No_External_Scripts},
                               {chaiscript::Library_Options::No_IO}};
  chaiscript::ChaiScript::State initialState;
  std::map<std::string, chaiscript::Boxed_Value> initialLocals;

  CachedEngine() {
    detail::ConfigureEngine(engine, {});
    initialState = engine.get_state();
    initialLocals = engine.get_locals();
  }
};

using NamespacePaths = std::set<std::string>;

std::invalid_argument SyntaxError(const chaiscript::exception::eval_error& error) {
  constexpr std::string_view dispatchPrefix = "Error with function dispatch for function '";
  std::string reason = error.reason;
  if (reason.starts_with(dispatchPrefix)) {
    auto end = reason.find('\'', dispatchPrefix.size());
    if (end != std::string::npos) {
      reason = "invalid arguments for '" + reason.substr(dispatchPrefix.size(),
                                                          end - dispatchPrefix.size()) + "'";
    }
  }
  return std::invalid_argument("Syntax error: " + reason);
}

bool GetMemberPath(const chaiscript::AST_Node& node, std::string& path) {
  using Type = chaiscript::AST_Node_Type;
  // Recognize only identifiers and dotted member chains, not method calls or indexing.
  if (node.identifier == Type::Id) {
    path = node.text;
    return true;
  }
  auto children = node.get_children();
  if (node.identifier != Type::Dot_Access || children.size() != 2 ||
      children[1].get().identifier != Type::Id || !GetMemberPath(children[0].get(), path)) {
    return false;
  }
  path += "." + children[1].get().text;
  return true;
}

const chaiscript::AST_Node* SetupDeclarationName(const chaiscript::AST_Node& node) {
  using Type = chaiscript::AST_Node_Type;
  auto children = node.get_children();
  if (children.size() != 2) {
    return nullptr;
  }
  const auto* name = &children[0].get();
  if (node.identifier == Type::Equation && node.text == "=" &&
      name->identifier == Type::Global_Decl) {
    auto globalChildren = name->get_children();
    return globalChildren.size() == 1 && globalChildren[0].get().identifier == Type::Id
               ? &globalChildren[0].get()
               : nullptr;
  }
  if ((node.identifier == Type::Assign_Decl ||
       node.identifier == Type::Const_Assign_Decl) &&
      name->identifier == Type::Id) {
    return name;
  }
  return nullptr;
}

void CollectVariableReferences(const chaiscript::AST_Node& node,
                               const std::map<std::string, std::size_t>& declarationIndices,
                               std::set<std::size_t>& dependencies) {
  if (node.identifier == chaiscript::AST_Node_Type::Id) {
    if (auto found = declarationIndices.find(node.text); found != declarationIndices.end()) {
      dependencies.insert(found->second);
    }
  }
  for (const auto& child : node.get_children()) {
    CollectVariableReferences(child.get(), declarationIndices, dependencies);
  }
}

void CollectNamespacePaths(const detail::Variables& variables, const std::string& prefix,
                           NamespacePaths& paths) {
  // Record namespace roots and their members from the frozen context snapshot.
  for (const auto& [name, value] : variables) {
    auto path = prefix.empty() ? name : prefix + "." + name;
    if (!prefix.empty() || std::holds_alternative<detail::NamespacePtr>(value.value)) {
      paths.insert(path);
    }
    if (const auto* nested = std::get_if<detail::NamespacePtr>(&value.value)) {
      CollectNamespacePaths((*nested)->members, path, paths);
    }
  }
}

const detail::VariableRecord* FindVariableById(const detail::Variables& variables,
                                               std::string_view id) {
  for (const auto& [_, record] : variables) {
    if (record.id == id && !id.empty()) {
      return &record;
    }
    if (const auto* nested = std::get_if<detail::NamespacePtr>(&record.value);
        nested != nullptr && *nested) {
      if (auto* found = FindVariableById((*nested)->members, id)) {
        return found;
      }
    }
  }
  return nullptr;
}

const detail::VariableRecord* FindVariableRecord(const detail::Variables& variables,
                                                 std::string_view path) {
  const detail::Variables* current = &variables;
  const detail::VariableRecord* record = nullptr;
  std::size_t start = 0;
  while (start < path.size()) {
    auto separator = path.find('.', start);
    auto name = path.substr(start, separator == std::string_view::npos
                                       ? path.size() - start
                                       : separator - start);
    auto found = current->find(std::string(name));
    if (found == current->end()) {
      return nullptr;
    }
    record = &found->second;
    if (separator == std::string_view::npos) {
      return record;
    }
    const auto* nested = std::get_if<detail::NamespacePtr>(&record->value);
    if (nested == nullptr || !*nested) {
      return nullptr;
    }
    current = &(*nested)->members;
    start = separator + 1;
  }
  return nullptr;
}

void CollectVariableInfo(const detail::Variables& variables, const std::string& namespacePath,
                         std::vector<VariableInfo>& result) {
  for (const auto& [name, record] : variables) {
    if (const auto* nested = std::get_if<detail::NamespacePtr>(&record.value);
        nested != nullptr && *nested) {
      auto childPath = namespacePath.empty() ? name : namespacePath + "." + name;
      CollectVariableInfo((*nested)->members, childPath, result);
      continue;
    }
    if (record.id.empty()) {
      continue;
    }
    VariableInfo info;
    info.definition = {record.id, namespacePath, name, record.expression};
    info.type = static_cast<VariableType>(record.declaredType->type);
    info.definition.expectedType = info.type;
    if (record.declaredType->type == detail::VariableType::Quantity) {
      info.dimension = record.declaredType->dimension;
      info.definition.expectedDimension = info.dimension;
    }
    std::visit(
        [&](const auto& value) {
          using Value = std::decay_t<decltype(value)>;
          if constexpr (!std::is_same_v<Value, detail::NamespacePtr>) {
            info.value = value;
          }
        },
        record.value);
    result.push_back(std::move(info));
  }
}

void CollectDeclaredPaths(const std::vector<VariableDefinition>& definitions,
                          std::map<std::string, std::size_t>& indices,
                          NamespacePaths& namespacePaths,
                          std::set<std::string>& declaredVariables) {
  std::set<std::string> namespacePrefixes;
  std::set<std::string> ids;
  detail::Variables identifiers;
  for (std::size_t index = 0; index < definitions.size(); ++index) {
    const auto& definition = definitions[index];
    if (definition.id.empty() || definition.name.empty() || definition.expression.empty()) {
      throw std::invalid_argument("variable id, name, and expression must not be empty");
    }
    if (!ids.insert(definition.id).second) {
      throw std::invalid_argument("duplicate variable id: " + definition.id);
    }
    detail::StoreVariable(identifiers, definition.name, units::precise_measurement{}, nullptr, {},
                          {}, !definition.namespacePath.empty());
    identifiers.clear();
    if (!definition.namespacePath.empty() && definition.namespacePath.back() == '.') {
      throw std::invalid_argument("invalid namespace path");
    }
    std::string path;
    std::size_t start = 0;
    while (start < definition.namespacePath.size()) {
      auto end = definition.namespacePath.find('.', start);
      auto part = definition.namespacePath.substr(
          start, end == std::string::npos ? std::string::npos : end - start);
      if (part.empty()) {
        throw std::invalid_argument("invalid namespace path");
      }
      detail::StoreVariable(identifiers, part, units::precise_measurement{}, nullptr, {}, {}, true);
      identifiers.clear();
      path = path.empty() ? part : path + "." + part;
      namespacePrefixes.insert(path);
      namespacePaths.insert(path);
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
    auto qualifiedName = definition.namespacePath.empty()
                             ? definition.name
                             : definition.namespacePath + "." + definition.name;
    if (!indices.emplace(qualifiedName, index).second) {
      throw std::invalid_argument("duplicate variable name: " + qualifiedName);
    }
    namespacePaths.insert(qualifiedName);
    if (definition.namespacePath.empty()) {
      declaredVariables.insert(definition.name);
    }
  }
  for (const auto& [path, index] : indices) {
    if (namespacePrefixes.contains(path)) {
      throw std::invalid_argument("variable conflicts with namespace: " + path);
    }
  }
}

void CollectReferences(const chaiscript::AST_Node& node,
                       const std::map<std::string, std::size_t>& indices,
                       std::set<std::size_t>& dependencies) {
  if (node.identifier == chaiscript::AST_Node_Type::Id) {
    if (auto found = indices.find(node.text); found != indices.end()) {
      dependencies.insert(found->second);
    }
  } else if (node.identifier == chaiscript::AST_Node_Type::Dot_Access) {
    std::string path;
    if (GetMemberPath(node, path)) {
      if (auto found = indices.find(path); found != indices.end()) {
        dependencies.insert(found->second);
      }
    }
  }
  for (const auto& child : node.get_children()) {
    CollectReferences(child.get(), indices, dependencies);
  }
}

std::size_t SourceOffset(std::string_view source, const chaiscript::File_Position& position);

void CollectReferenceEdits(const chaiscript::AST_Node& node, std::string_view source,
                           std::string_view target, std::string_view replacement,
                           std::vector<std::pair<std::size_t, std::string>>& edits) {
  using Type = chaiscript::AST_Node_Type;
  std::string path;
  if (node.identifier == Type::Id && node.text == target) {
    edits.emplace_back(SourceOffset(source, node.start()), replacement);
  } else if (node.identifier == Type::Dot_Access && GetMemberPath(node, path) &&
             path == target) {
    auto children = node.get_children();
    edits.emplace_back(SourceOffset(source, children[1].get().start()), replacement);
  }
  for (const auto& child : node.get_children()) {
    CollectReferenceEdits(child.get(), source, target, replacement, edits);
  }
}

bool HasVariableReference(std::string_view source, std::string_view target) {
  auto expanded = detail::ExpandUnitSuffixes(source);
  chaiscript::ChaiScript parser{{}, {},
                                {chaiscript::Options::No_Load_Modules,
                                 chaiscript::Options::No_External_Scripts},
                                {chaiscript::Library_Options::No_IO}};
  chaiscript::AST_NodePtr parsed;
  try {
    parsed = parser.parse(expanded);
  } catch (const chaiscript::exception::eval_error& error) {
    throw SyntaxError(error);
  }
  std::vector<std::pair<std::size_t, std::string>> edits;
  CollectReferenceEdits(*parsed, expanded, target, {}, edits);
  return !edits.empty();
}

detail::VariableValue UnboxVariableValue(chaiscript::ChaiScript& engine,
                                         const chaiscript::Boxed_Value& value,
                                         std::string_view name) {
  auto type = value.get_type_info();
  if (type.bare_equal(chaiscript::user_type<units::precise_measurement>())) {
    return chaiscript::boxed_cast<units::precise_measurement>(value);
  }
  if (type.bare_equal(chaiscript::user_type<Pose2d>())) {
    return chaiscript::boxed_cast<Pose2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<Translation2d>())) {
    return chaiscript::boxed_cast<Translation2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<detail::Polygon2d>())) {
    return chaiscript::boxed_cast<detail::Polygon2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<std::vector<chaiscript::Boxed_Value>>())) {
    std::vector<Translation2d> vertices;
    const auto& values =
        chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(value);
    vertices.reserve(values.size());
    for (const auto& item : values) {
      try {
        vertices.push_back(chaiscript::boxed_cast<Translation2d>(item));
      } catch (const chaiscript::exception::bad_boxed_cast&) {
        throw std::invalid_argument("polygon vertices must be Translation2d values: " +
                                    std::string(name));
      }
    }
    return detail::Polygon2d{std::move(vertices)};
  }
  if (type.bare_equal(chaiscript::user_type<Rotation2d>())) {
    return chaiscript::boxed_cast<Rotation2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<Transform2d>())) {
    return chaiscript::boxed_cast<Transform2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<Twist2d>())) {
    return chaiscript::boxed_cast<Twist2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<Rectangle2d>())) {
    return chaiscript::boxed_cast<Rectangle2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<Ellipse2d>())) {
    return chaiscript::boxed_cast<Ellipse2d>(value);
  }
  if (type.bare_equal(chaiscript::user_type<double>())) {
    return units::precise_measurement{chaiscript::boxed_cast<double>(value), {}};
  }
  if (type.bare_equal(chaiscript::user_type<int>())) {
    return units::precise_measurement{
        static_cast<double>(chaiscript::boxed_cast<int>(value)), {}};
  }
  if (type.bare_equal(chaiscript::user_type<std::int64_t>())) {
    return units::precise_measurement{
        static_cast<double>(chaiscript::boxed_cast<std::int64_t>(value)), {}};
  }
  try {
    return engine.boxed_cast<units::precise_measurement>(value);
  } catch (const chaiscript::exception::bad_boxed_cast&) {
    throw std::invalid_argument("unsupported variable value type: " + std::string(name));
  }
}

void ValidateExpression(const chaiscript::AST_Node& node,
                        const detail::Variables& variables, const NamespacePaths& namespacePaths,
                        const std::set<std::string>& declaredVariables) {
  // Calls and operators are restricted to the library's read-only expression surface.
  static const std::set<std::string_view> safeFunctions{
      "quantity", "abs", "acos", "acosh", "asin", "asinh", "atan", "atan2", "atanh",
      "cbrt", "ceil", "copysign", "cos", "cosh", "erf", "erfc", "exp", "exp2",
      "expm1", "fabs", "fdim", "fma", "floor", "fmax", "fmin", "fmod", "hypot", "ilogb",
      "isfinite", "isgreater", "isgreaterequal", "isinf", "isless", "islessequal",
      "islessgreater", "isnan", "isnormal", "isunordered", "ldexp", "lgamma", "llrint",
      "llround", "log",
      "log10", "log1p", "log2", "logb", "lrint", "lround", "max", "maximum", "min",
      "minimum", "nearbyint", "nextafter", "nexttoward", "pow", "quantity", "remainder",
      "remquo", "rint", "round", "scalbln", "scalbn", "signbit", "sin", "sinh", "sqrt",
        "rint", "round", "scalbln", "scalbn", "signbit", "sin", "sinh", "sqrt",
        "lround", "llrint", "llround", "max", "maximum", "min", "minimum", "nearbyint", "nextafter", "nexttoward",
      "tan", "tanh", "tgamma", "trunc", "Pose2d",
      "Translation2d", "Rotation2d", "Transform2d", "Twist2d", "Rectangle2d", "Ellipse2d",
      "X", "Y",
      "Rotation", "Translation", "RotateBy", "RelativeTo", "TransformBy",
      "Distance", "Norm", "Radians", "Degrees", "Cos", "Sin", "Tan", "Inverse", "Log",
      "dx", "dy", "dtheta", "Exp", "Center", "XWidth", "YWidth", "XSemiAxis", "YSemiAxis",
      "FocalPoints", "Intersects", "Contains", "Nearest", "RotateAround", "SquaredDistance",
      "SquaredNorm", "Dot", "Cross", "Angle", "Size"};
  static const std::set<std::string_view> safeOperators{
      "+", "-", "*", "/", "^", "==", "!=", "<", "<=", ">", ">=", "!"};
  using Type = chaiscript::AST_Node_Type;
  auto children = node.get_children();
  std::string path;
  // Resolve namespace member chains against declared paths; method calls are checked below.
  if (node.identifier == Type::Dot_Access && GetMemberPath(node, path)) {
    auto separator = path.find('.');
    if (separator != std::string::npos && namespacePaths.contains(path.substr(0, separator))) {
      if (!namespacePaths.contains(path)) {
        throw std::invalid_argument("unknown namespace member: " + path);
      }
      return;
    }
  }
  switch (node.identifier) {
    case Type::File:
      // Multiple top-level statements would permit scripts instead of one expression.
      if (children.size() != 1) {
        throw std::invalid_argument("expected a single read-only expression");
      }
      break;
    case Type::Id:
      // Bare identifiers must be context values, units, allowed functions, or booleans.
      if (!variables.contains(node.text) && !declaredVariables.contains(node.text) &&
          !detail::UnitRegistry().contains(node.text) &&
          !safeFunctions.contains(node.text) && node.text != "true" && node.text != "false") {
        throw std::invalid_argument("unknown expression identifier: " + node.text);
      }
      break;
    case Type::Fun_Call:
      // Reject indirect calls and built-ins such as eval, use, and namespace.
      if (children.empty() || children.front().get().identifier != Type::Id ||
          !safeFunctions.contains(children.front().get().text)) {
        throw std::invalid_argument("function is not permitted in an expression");
      }
      break;
    case Type::Binary:
    case Type::Prefix:
      // Exclude mutation operators such as increment and compound assignment.
      if (!safeOperators.contains(node.text)) {
        throw std::invalid_argument("operator is not permitted in an expression");
      }
      break;
    case Type::Constant:
    case Type::Arg_List:
    case Type::Dot_Access:
    case Type::Array_Call:
    case Type::Inline_Array:
    case Type::Logical_And:
    case Type::Logical_Or:
      // These expression nodes are allowed only after all of their children are validated.
      break;
    default:
      // Reject assignments, declarations, control flow, lambdas, and other script nodes.
      throw std::invalid_argument("only read-only expressions are permitted");
  }
  for (const auto& child : children) {
    ValidateExpression(child.get(), variables, namespacePaths, declaredVariables);
  }
}

std::size_t SourceOffset(std::string_view source, const chaiscript::File_Position& position) {
  if (position.line <= 0 || position.column <= 0) {
    throw std::invalid_argument("invalid setup source location");
  }
  std::size_t offset = 0;
  for (int line = 1; line < position.line; ++line) {
    auto newline = source.find('\n', offset);
    if (newline == std::string_view::npos) {
      throw std::invalid_argument("invalid setup source location");
    }
    offset = newline + 1;
  }
  offset += static_cast<std::size_t>(position.column - 1);
  if (offset > source.size()) {
    throw std::invalid_argument("invalid setup source location");
  }
  return offset;
}

std::size_t DeclarationStartOffset(std::string_view source, std::size_t identifierOffset) {
  for (std::string_view prefix : {"const var", "const auto", "global", "var", "auto"}) {
    auto candidate = source.rfind(prefix, identifierOffset);
    while (candidate != std::string_view::npos) {
      auto prefixEnd = candidate + prefix.size();
      bool whitespaceOnly = prefixEnd <= identifierOffset &&
                            std::all_of(source.begin() + prefixEnd,
                                        source.begin() + identifierOffset,
                                        [](char character) {
                                          return std::isspace(static_cast<unsigned char>(character));
                                        });
      bool tokenBoundary = candidate == 0 ||
                           std::isspace(static_cast<unsigned char>(source[candidate - 1])) ||
                           source[candidate - 1] == ';' || source[candidate - 1] == '}';
      if (whitespaceOnly && tokenBoundary) {
        return candidate;
      }
      if (candidate == 0) {
        break;
      }
      candidate = source.rfind(prefix, candidate - 1);
    }
  }
  return identifierOffset;
}

std::string OrderSetupDeclarations(const chaiscript::AST_Node& root, std::string_view source) {
  const auto statements = root.get_children();
  if (statements.size() < 2) {
    return std::string(source);
  }

  std::map<std::string, std::size_t> declarationIndices;
  for (std::size_t index = 0; index < statements.size(); ++index) {
    if (const auto* name = SetupDeclarationName(statements[index].get())) {
      declarationIndices.try_emplace(name->text, index);
    }
  }

  std::vector<std::size_t> offsets;
  offsets.reserve(statements.size());
  for (const auto& statement : statements) {
    auto offset = SourceOffset(source, statement.get().start());
    if (SetupDeclarationName(statement.get())) {
      offset = DeclarationStartOffset(source, offset);
    }
    offsets.push_back(offset);
  }
  std::vector<std::size_t> sourceOrder(statements.size());
  for (std::size_t index = 0; index < sourceOrder.size(); ++index) {
    sourceOrder[index] = index;
  }
  std::sort(sourceOrder.begin(), sourceOrder.end(),
            [&](std::size_t left, std::size_t right) { return offsets[left] < offsets[right]; });

  std::vector<std::set<std::size_t>> dependents(statements.size());
  std::vector<std::size_t> dependencyCounts(statements.size());
  for (std::size_t index = 0; index < statements.size(); ++index) {
    auto children = statements[index].get().get_children();
    if (!SetupDeclarationName(statements[index].get()) || children.size() != 2) {
      continue;
    }
    std::set<std::size_t> dependencies;
    CollectVariableReferences(children[1].get(), declarationIndices, dependencies);
    dependencyCounts[index] = dependencies.size();
    for (auto dependency : dependencies) {
      dependents[dependency].insert(index);
    }
  }

  std::set<std::size_t> ready;
  for (auto index : sourceOrder) {
    if (dependencyCounts[index] == 0) {
      ready.insert(index);
    }
  }
  std::vector<std::size_t> evaluationOrder;
  evaluationOrder.reserve(statements.size());
  while (!ready.empty()) {
    auto index = *ready.begin();
    ready.erase(ready.begin());
    evaluationOrder.push_back(index);
    for (auto dependent : dependents[index]) {
      if (--dependencyCounts[dependent] == 0) {
        ready.insert(dependent);
      }
    }
  }
  if (evaluationOrder.size() != statements.size()) {
    throw std::invalid_argument("cyclic setup variable dependency");
  }

  std::string ordered;
  ordered.reserve(source.size());
  ordered.append(source.substr(0, offsets[sourceOrder.front()]));
  for (std::size_t position = 0; position < sourceOrder.size(); ++position) {
    auto index = evaluationOrder[position];
    auto sourcePosition = std::find(sourceOrder.begin(), sourceOrder.end(), index) -
                          sourceOrder.begin();
    auto begin = offsets[index];
    auto end = sourcePosition + 1 < sourceOrder.size()
                   ? offsets[sourceOrder[sourcePosition + 1]]
                   : source.size();
    ordered.append(source.substr(begin, end - begin));
  }
  return ordered;
}

void ValidateSetup(const chaiscript::AST_Node& root,
           const std::map<std::string, chaiscript::Boxed_Value>& initialObjects) {
  using Type = chaiscript::AST_Node_Type;
  // Setup is a file of declarations, not an arbitrary executable AST.
  if (root.identifier != Type::File) {
    throw std::invalid_argument("expected setup variable declarations");
  }
  detail::Variables variables;
  NamespacePaths namespacePaths;
  NamespacePaths namespaces;
  std::set<std::string> declaredVariables;
  for (const auto& statement : root.get_children()) {
    if (const auto* name = SetupDeclarationName(statement.get())) {
      declaredVariables.insert(name->text);
    }
  }
  auto declareNamespace = [&](std::string_view qualifiedName) {
    // Validate each component and prevent namespaces from replacing existing values.
    std::string path;
    std::size_t start = 0;
    while (start < qualifiedName.size()) {
      auto end = qualifiedName.find("::", start);
      auto part = qualifiedName.substr(start, end == std::string_view::npos ? end : end - start);
      if (part.empty() || !(std::isalpha(static_cast<unsigned char>(part.front())) || part.front() == '_')) {
        throw std::invalid_argument("invalid namespace name");
      }
      for (char character : part) {
        if (!(std::isalnum(static_cast<unsigned char>(character)) || character == '_')) {
          throw std::invalid_argument("invalid namespace name");
        }
      }
      path = path.empty() ? std::string(part) : path + "." + std::string(part);
      if (namespacePaths.contains(path) && !namespaces.contains(path)) {
        throw std::invalid_argument("namespace conflicts with a value: " + path);
      }
      if (path.find('.') == std::string::npos && !namespaces.contains(path)) {
        // A new namespace root must not collide with units, built-ins, or prior variables.
        if (variables.contains(path) || initialObjects.contains(path)) {
          throw std::invalid_argument("invalid or duplicate namespace: " + path);
        }
        detail::StoreVariable(variables, path, std::make_shared<detail::NamespaceValues>());
      }
      namespacePaths.insert(path);
      namespaces.insert(path);
      if (end == std::string_view::npos) {
        return path;
      }
      start = end + 2;
    }
    throw std::invalid_argument("invalid namespace name");
  };
  for (const auto& statement : root.get_children()) {
    const auto& node = statement.get();
    auto children = node.get_children();
    // Permit only a literal name in the legacy namespace("name") declaration.
    if (node.identifier == Type::Fun_Call && children.size() == 2 &&
        children[0].get().identifier == Type::Id && children[0].get().text == "namespace") {
      auto arguments = children[1].get().get_children();
      if (arguments.size() != 1 || arguments[0].get().identifier != Type::Constant) {
        throw std::invalid_argument("namespace requires a literal name");
      }
      declareNamespace(arguments[0].get().text);
      continue;
    }
    // Namespace blocks may contain initialized variables, but not functions or control flow.
    if (node.identifier == Type::Namespace_Block && children.size() == 2 &&
        children[0].get().identifier == Type::Id) {
      auto path = declareNamespace(children[0].get().text);
      const auto& body = children[1].get();
      if (body.identifier != Type::Block && body.identifier != Type::Scopeless_Block) {
        throw std::invalid_argument("invalid namespace block");
      }
      for (const auto& statement : body.get_children()) {
        const auto& declaration = statement.get();
        auto parts = declaration.get_children();
        if ((declaration.identifier != Type::Assign_Decl &&
             declaration.identifier != Type::Const_Assign_Decl) ||
            parts.size() != 2 || parts[0].get().identifier != Type::Id) {
          throw std::invalid_argument("namespace permits only initialized variable declarations");
        }
        auto member = path + "." + parts[0].get().text;
        if (namespaces.contains(member)) {
          throw std::invalid_argument("namespace conflicts with a value: " + member);
        }
        // Initializers may refer only to values declared earlier in the setup.
        ValidateExpression(parts[1].get(), variables, namespacePaths, declaredVariables);
        namespacePaths.insert(member);
      }
      continue;
    }
    if (children.size() != 2) {
      throw std::invalid_argument("setup permits only initialized variable declarations");
    }
    const chaiscript::AST_Node* name = &children[0].get();
    std::string memberPath;
    // Legacy dotted assignments must target a declared namespace, not a value or its fields.
    if (node.identifier == Type::Equation && node.text == "=" &&
        name->identifier == Type::Dot_Access && GetMemberPath(*name, memberPath)) {
      auto separator = memberPath.rfind('.');
      if (separator == std::string::npos || !namespaces.contains(memberPath.substr(0, separator)) ||
          namespaces.contains(memberPath)) {
        throw std::invalid_argument("invalid namespace member: " + memberPath);
      }
      ValidateExpression(children[1].get(), variables, namespacePaths, declaredVariables);
      namespacePaths.insert(memberPath);
      continue;
    }
    // Top-level declarations include initialized locals, constants, and global variables.
    if (node.identifier == Type::Equation && node.text == "=" &&
        name->identifier == Type::Global_Decl) {
      auto globalChildren = name->get_children();
      if (globalChildren.size() != 1) {
        throw std::invalid_argument("invalid global variable declaration");
      }
      name = &globalChildren[0].get();
    } else if (node.identifier != Type::Assign_Decl &&
               node.identifier != Type::Const_Assign_Decl) {
      throw std::invalid_argument("setup permits only initialized variable declarations");
    }
    if (name->identifier != Type::Id || variables.contains(name->text) ||
        initialObjects.contains(name->text)) {
      throw std::invalid_argument("invalid or duplicate setup variable: " + name->text);
    }
    ValidateExpression(children[1].get(), variables, namespacePaths, declaredVariables);
    detail::StoreVariable(variables, name->text, units::precise_measurement{});
  }
}

template <typename Result, typename Cache>
Result EvaluateResult(Cache& engineCache, const detail::Variables& variables,
                      std::string_view source) {
  std::lock_guard lock(engineCache.mutex);
  if (!engineCache.cache) {
    engineCache.cache.emplace();
  }
  auto& cache = *engineCache.cache;
  auto expanded = detail::ExpandUnitSuffixes(source);
  chaiscript::AST_NodePtr parsed;
  try {
    parsed = cache.engine.parse(expanded);
  } catch (const chaiscript::exception::eval_error& error) {
    throw SyntaxError(error);
  }
  NamespacePaths namespacePaths;
  CollectNamespacePaths(variables, "", namespacePaths);
  std::set<std::string> declaredVariables;
  for (const auto& [name, _] : variables) {
    declaredVariables.insert(name);
  }
  ValidateExpression(*parsed, variables, namespacePaths, declaredVariables);
  cache.engine.set_state(cache.initialState);
  cache.engine.set_locals(cache.initialLocals);
  detail::ConfigureGlobals(cache.engine, variables);
  auto result = [&]() -> Result {
    try {
      if constexpr (std::is_same_v<Result, detail::Polygon2d>) {
        auto value = UnboxVariableValue(cache.engine, cache.engine.eval(expanded), "expression");
        if (auto* polygon = std::get_if<detail::Polygon2d>(&value)) {
          return std::move(*polygon);
        }
        throw std::invalid_argument("expression result is not a polygon");
      } else {
        return cache.engine.template eval<Result>(expanded);
      }
    } catch (const chaiscript::exception::eval_error& error) {
      throw SyntaxError(error);
    }
  }();
  return result;
}

}  // namespace

struct EvaluationContext::Impl {
  detail::Variables variables;
};

struct EvaluationContext::EngineCache {
  std::mutex mutex;
  std::optional<CachedEngine> cache;
};

EvaluationContext::EvaluationContext() : engineCache_(std::make_shared<EngineCache>()) {}

EvaluationContext::EvaluationContext(EvaluationContext&& other) noexcept
    : impl_(std::move(other.impl_)), engineCache_(other.engineCache_) {}

EvaluationContext& EvaluationContext::operator=(EvaluationContext&& other) noexcept {
  if (this != &other) {
    impl_ = std::move(other.impl_);
    engineCache_ = other.engineCache_;
  }
  return *this;
}

std::string RenameVariableReferences(std::string_view source, std::string_view oldName,
                                     std::string_view newName) {
  static const std::set<std::string> reservedNames = [] {
    CachedEngine engine;
    std::set<std::string> names;
    for (const auto& [name, value] : engine.engine.get_scripting_objects()) {
      names.insert(name);
    }
    return names;
  }();
  for (auto name : {oldName, newName}) {
    detail::Variables variable;
    detail::StoreVariable(variable, std::string(name), units::precise_measurement{});
    if (!chaiscript::Name_Validator::valid_object_name(name) ||
        reservedNames.contains(std::string(name))) {
      throw std::invalid_argument("invalid setup variable name: " + std::string(name));
    }
  }
  return detail::RenameVariableReferences(source, oldName, newName);
}

std::vector<SetupVariable> RenameSetupVariable(std::span<const SetupVariable> variables,
                                               std::string_view id, std::string_view newName) {
  std::set<std::string> ids;
  std::set<std::string> names;
  const SetupVariable* target = nullptr;
  for (const auto& variable : variables) {
    if (variable.id.empty() || !ids.insert(variable.id).second) {
      throw SetupVariableError(variable.id, "empty or duplicate setup variable ID");
    }
    if (!names.insert(variable.name).second) {
      throw SetupVariableError(variable.id, "duplicate setup variable name");
    }
    if (variable.id == id) {
      target = &variable;
    }
  }
  if (!target) {
    throw std::invalid_argument("unknown setup variable ID: " + std::string(id));
  }
  if (target->name != newName && names.contains(std::string(newName))) {
    throw SetupVariableError(target->id, "duplicate setup variable name: " + std::string(newName));
  }
  try {
    RenameVariableReferences({}, target->name, newName);
  } catch (const std::invalid_argument& error) {
    throw SetupVariableError(target->id, error.what());
  }
  std::vector<SetupVariable> result(variables.begin(), variables.end());
  for (auto& variable : result) {
    variable.initializer = detail::RenameVariableReferences(variable.initializer, target->name, newName);
    if (variable.id == id) {
      variable.name = newName;
    }
  }
  return result;
}

void EvaluationContext::SetSetupVariables(std::span<const SetupVariable> variables) {
  if (variables.empty()) {
    impl_ = std::make_shared<Impl>();
    return;
  }
  chaiscript::ChaiScript engine{{}, {},
                               {chaiscript::Options::No_Load_Modules,
                                chaiscript::Options::No_External_Scripts},
                               {chaiscript::Library_Options::No_IO}};
  detail::ConfigureEngine(engine, {});
  const auto initialObjects = engine.get_scripting_objects();
  detail::Variables names;
  std::map<std::string, std::size_t> indices;
  std::set<std::string> ids;
  for (std::size_t index = 0; index < variables.size(); ++index) {
    const auto& variable = variables[index];
    try {
      if (variable.id.empty() || !ids.insert(variable.id).second) {
        throw std::invalid_argument("empty or duplicate setup variable ID");
      }
      if (indices.contains(variable.name) || initialObjects.contains(variable.name)) {
        throw std::invalid_argument("invalid or duplicate setup variable name: " + variable.name);
      }
      detail::StoreVariable(names, variable.name, units::precise_measurement{});
      auto declaration = engine.parse("var " + variable.name + " = 0;");
      ValidateSetup(*declaration, initialObjects);
      indices.emplace(variable.name, index);
    } catch (const chaiscript::exception::eval_error& error) {
      throw SetupVariableError(variable.id, SyntaxError(error).what());
    } catch (const std::exception& error) {
      throw SetupVariableError(variable.id, error.what());
    }
  }

  std::set<std::string> declaredNames;
  for (const auto& [name, _] : names) {
    declaredNames.insert(name);
  }
  std::vector<std::vector<std::size_t>> dependencies(variables.size());
  for (std::size_t index = 0; index < variables.size(); ++index) {
    const auto& variable = variables[index];
    try {
      auto expanded = detail::ExpandUnitSuffixes(variable.initializer);
      auto parsed = engine.parse(expanded);
      ValidateExpression(*parsed, names, {}, declaredNames);
      std::function<void(const chaiscript::AST_Node&)> collect;
      collect = [&](const chaiscript::AST_Node& node) {
        if (node.identifier == chaiscript::AST_Node_Type::Id) {
          auto found = indices.find(node.text);
          if (found != indices.end()) {
            dependencies[index].push_back(found->second);
          }
        }
        for (const auto& child : node.get_children()) {
          collect(child.get());
        }
      };
      collect(*parsed);
    } catch (const chaiscript::exception::eval_error& error) {
      throw SetupVariableError(variable.id, SyntaxError(error).what());
    } catch (const std::exception& error) {
      throw SetupVariableError(variable.id, error.what());
    }
  }

  std::vector<int> state(variables.size());
  std::vector<std::size_t> order;
  std::function<void(std::size_t)> visit = [&](std::size_t index) {
    if (state[index] == 1) {
      throw SetupVariableError(variables[index].id, "dependency cycle");
    }
    if (state[index] == 2) {
      return;
    }
    state[index] = 1;
    for (auto dependency : dependencies[index]) {
      visit(dependency);
    }
    state[index] = 2;
    order.push_back(index);
  };
  for (std::size_t index = 0; index < variables.size(); ++index) {
    visit(index);
  }

  std::string script;
  std::vector<SetupVariable> ordered;
  ordered.reserve(order.size());
  for (auto index : order) {
    const auto& variable = variables[index];
    script += "var " + variable.name + " = " + variable.initializer + ";\n";
    ordered.push_back(variable);
  }
  SetSetupScriptImpl(script, ordered);
}

void EvaluationContext::SetSetupScript(std::string_view source) {
  SetSetupScriptImpl(source, {});
}

void EvaluationContext::SetSetupScriptImpl(std::string_view source,
                                           std::span<const SetupVariable> ordered) {
  chaiscript::ChaiScript engine{{}, {},
                               {chaiscript::Options::No_Load_Modules,
                                chaiscript::Options::No_External_Scripts},
                               {chaiscript::Library_Options::No_IO}};
  detail::ConfigureEngine(engine, {});
  auto initialObjects = engine.get_scripting_objects();
  auto initialLocals = engine.get_locals();
  auto expanded = detail::ExpandUnitSuffixes(source);
  chaiscript::AST_NodePtr parsed;
  try {
    parsed = engine.parse(expanded);
  } catch (const chaiscript::exception::eval_error& error) {
    throw SyntaxError(error);
  }
  ValidateSetup(*parsed, initialObjects);
  if (ordered.empty()) {
    auto orderedSource = OrderSetupDeclarations(*parsed, expanded);
    try {
      engine.eval(orderedSource);
    } catch (const chaiscript::exception::eval_error& error) {
      throw SyntaxError(error);
    }
  } else {
    for (const auto& variable : ordered) {
      try {
        engine.eval(detail::ExpandUnitSuffixes("var " + variable.name + " = " +
                                               variable.initializer + ";\n"));
      } catch (const chaiscript::exception::eval_error& error) {
        throw SetupVariableError(variable.id, SyntaxError(error).what());
      } catch (const std::exception& error) {
        throw SetupVariableError(variable.id, error.what());
      }
    }
  }

  auto replacement = std::make_shared<Impl>();
  std::set<const chaiscript::Namespace*> activeNamespaces;
  static const detail::Variables emptyVariables;
  const auto& previousVariables = impl_ ? impl_->variables : emptyVariables;
  std::function<void(detail::Variables&, const std::string&, const std::string&,
                     const chaiscript::Boxed_Value&)> store;
  store = [&](detail::Variables& destination, const std::string& name,
              const std::string& qualifiedName, const chaiscript::Boxed_Value& value) {
    auto save = [&](detail::VariableValue variable) {
      const auto* existing = FindVariableRecord(previousVariables, qualifiedName);
      detail::StoreVariable(destination, name, std::move(variable), existing, {}, {},
                &destination != &replacement->variables);
    };
    auto type = value.get_type_info();
    if (type.bare_equal(chaiscript::user_type<chaiscript::Namespace>())) {
      const auto& object = chaiscript::boxed_cast<const chaiscript::Namespace&>(value);
      if (!activeNamespaces.insert(&object).second) {
        throw std::invalid_argument("cyclic namespace: " + name);
      }
      auto result = std::make_shared<detail::NamespaceValues>();
      for (const auto& [memberName, member] : object.get_attrs()) {
        store(result->members, memberName, qualifiedName + "." + memberName, member);
      }
      activeNamespaces.erase(&object);
      save(std::move(result));
    } else if (type.bare_equal(chaiscript::user_type<units::precise_measurement>())) {
      auto quantity = chaiscript::boxed_cast<units::precise_measurement>(value);
      save(quantity);
    } else if (type.bare_equal(chaiscript::user_type<Pose2d>())) {
      save(chaiscript::boxed_cast<Pose2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Translation2d>())) {
      save(chaiscript::boxed_cast<Translation2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<std::vector<chaiscript::Boxed_Value>>())) {
      std::vector<Translation2d> vertices;
      const auto& values = chaiscript::boxed_cast<const std::vector<chaiscript::Boxed_Value>&>(value);
      vertices.reserve(values.size());
      for (const auto& item : values) {
        try {
          vertices.push_back(chaiscript::boxed_cast<Translation2d>(item));
        } catch (const chaiscript::exception::bad_boxed_cast&) {
          throw std::invalid_argument("setup polygon vertices must be Translation2d values: " + name);
        }
      }
      save(detail::Polygon2d{std::move(vertices)});
    } else if (type.bare_equal(chaiscript::user_type<Rotation2d>())) {
      save(chaiscript::boxed_cast<Rotation2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Transform2d>())) {
      save(chaiscript::boxed_cast<Transform2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Twist2d>())) {
      save(chaiscript::boxed_cast<Twist2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Rectangle2d>())) {
      save(chaiscript::boxed_cast<Rectangle2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<Ellipse2d>())) {
      save(chaiscript::boxed_cast<Ellipse2d>(value));
    } else if (type.bare_equal(chaiscript::user_type<double>())) {
      auto quantity = units::precise_measurement{chaiscript::boxed_cast<double>(value), {}};
      save(quantity);
    } else if (type.bare_equal(chaiscript::user_type<int>())) {
      save(units::precise_measurement{static_cast<double>(chaiscript::boxed_cast<int>(value)), {}});
    } else if (type.bare_equal(chaiscript::user_type<std::int64_t>())) {
      save(units::precise_measurement{static_cast<double>(chaiscript::boxed_cast<std::int64_t>(value)), {}});
    } else {
      units::precise_measurement quantity;
      try {
        quantity = engine.boxed_cast<units::precise_measurement>(value);
      } catch (const chaiscript::exception::bad_boxed_cast&) {
        throw std::invalid_argument("unsupported setup variable type: " + name);
      }
      save(quantity);
    }
  };
  for (const auto& [name, value] : engine.get_locals()) {
    if (!initialLocals.contains(name)) {
      try {
        store(replacement->variables, name, name, value);
      } catch (const std::exception& error) {
        for (const auto& variable : ordered) {
          if (variable.name == name) {
            throw SetupVariableError(variable.id, error.what());
          }
        }
        throw;
      }
    }
  }
  impl_ = std::move(replacement);
}

void EvaluationContext::SetVariables(std::vector<VariableDefinition> definitions) {
  std::map<std::string, std::size_t> indices;
  NamespacePaths namespacePaths;
  std::set<std::string> declaredVariables;
  CollectDeclaredPaths(definitions, indices, namespacePaths, declaredVariables);

  chaiscript::ChaiScript parser{{}, {},
                                {chaiscript::Options::No_Load_Modules,
                                 chaiscript::Options::No_External_Scripts},
                                {chaiscript::Library_Options::No_IO}};
  std::vector<std::string> expanded;
  std::vector<chaiscript::AST_NodePtr> expressions;
  expanded.reserve(definitions.size());
  expressions.reserve(definitions.size());
  for (const auto& definition : definitions) {
    try {
      expanded.push_back(detail::ExpandUnitSuffixes(definition.expression));
      expressions.push_back(parser.parse(expanded.back()));
    } catch (const chaiscript::exception::eval_error& error) {
      throw VariableEvaluationError{definition.id, SyntaxError(error).what()};
    } catch (const std::logic_error& error) {
      throw VariableEvaluationError{definition.id, error.what()};
    }
  }

  std::vector<std::set<std::size_t>> dependents(definitions.size());
  std::vector<std::size_t> dependencyCounts(definitions.size());
  for (std::size_t index = 0; index < definitions.size(); ++index) {
    std::set<std::size_t> dependencies;
    CollectReferences(*expressions[index], indices, dependencies);
    dependencyCounts[index] = dependencies.size();
    for (auto dependency : dependencies) {
      dependents[dependency].insert(index);
    }
  }
  std::set<std::size_t> ready;
  for (std::size_t index = 0; index < definitions.size(); ++index) {
    if (dependencyCounts[index] == 0) {
      ready.insert(index);
    }
  }
  std::vector<std::size_t> order;
  order.reserve(definitions.size());
  while (!ready.empty()) {
    auto index = *ready.begin();
    ready.erase(ready.begin());
    order.push_back(index);
    for (auto dependent : dependents[index]) {
      if (--dependencyCounts[dependent] == 0) {
        ready.insert(dependent);
      }
    }
  }
  if (order.size() != definitions.size()) {
    throw std::invalid_argument("cyclic variable dependency");
  }

  static const detail::Variables emptyVariables;
  const auto& oldVariables = impl_ ? impl_->variables : emptyVariables;
  auto replacement = std::make_shared<Impl>();
  for (const auto& [name, record] : oldVariables) {
    if (record.id.empty() &&
        !std::holds_alternative<detail::NamespacePtr>(record.value)) {
      replacement->variables.emplace(name, record);
      declaredVariables.insert(name);
    }
  }
  std::map<std::string, std::shared_ptr<detail::NamespaceValues>> namespaces;
  std::function<void(const std::string&)> ensureNamespace;
  ensureNamespace = [&](const std::string& path) {
    auto existing = namespaces.find(path);
    if (existing != namespaces.end()) {
      return;
    }
    auto separator = path.rfind('.');
    auto parentPath = separator == std::string::npos ? std::string{} : path.substr(0, separator);
    auto name = separator == std::string::npos ? path : path.substr(separator + 1);
    detail::Variables* members = &replacement->variables;
    if (!parentPath.empty()) {
      ensureNamespace(parentPath);
      members = &namespaces.at(parentPath)->members;
    }
    auto value = std::make_shared<detail::NamespaceValues>();
    detail::StoreVariable(*members, name, detail::NamespacePtr{value}, nullptr, {}, {}, true);
    namespaces.emplace(path, std::move(value));
  };
  for (const auto& definition : definitions) {
    std::size_t start = 0;
    while (start < definition.namespacePath.size()) {
      auto end = definition.namespacePath.find('.', start);
      auto path = definition.namespacePath.substr(
          0, end == std::string::npos ? definition.namespacePath.size() : end);
      ensureNamespace(path);
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
  }

  chaiscript::ChaiScript evaluator{{},
                                   {},
                                   {chaiscript::Options::No_Load_Modules,
                                    chaiscript::Options::No_External_Scripts},
                                   {chaiscript::Library_Options::No_IO}};
  detail::ConfigureEngine(evaluator, replacement->variables);
  for (auto index : order) {
    const auto& definition = definitions[index];
    try {
      auto qualifiedName =
          definition.namespacePath.empty()
              ? definition.name
              : definition.namespacePath + "." + definition.name;
      ValidateExpression(*expressions[index], replacement->variables,
                         namespacePaths, declaredVariables);
      detail::ConfigureGlobals(evaluator, replacement->variables);
      chaiscript::Boxed_Value boxed;
      try {
        boxed = evaluator.eval(expanded[index]);
      } catch (const chaiscript::exception::eval_error& error) {
        throw SyntaxError(error);
      }
      auto value = UnboxVariableValue(evaluator, boxed, qualifiedName);
      const auto actualType = detail::GetDeclaredType(value);
      if (definition.expectedType) {
        const auto expectedType =
            static_cast<detail::VariableType>(*definition.expectedType);
        if (expectedType != actualType.type) {
          throw std::invalid_argument(
              "variable type does not match its persisted declaration: " +
              qualifiedName);
        }
        if (expectedType == detail::VariableType::Quantity) {
          if (!definition.expectedDimension ||
              definition.expectedDimension->base_units() !=
                  actualType.dimension.base_units()) {
            throw std::invalid_argument(
                "variable dimension does not match its persisted "
                "declaration: " +
                qualifiedName);
          }
        } else if (definition.expectedDimension) {
          throw std::invalid_argument(
              "geometry variable cannot declare a scalar dimension: " +
              qualifiedName);
        }
      } else if (definition.expectedDimension) {
        throw std::invalid_argument(
            "variable dimension requires a declared quantity type: " +
            qualifiedName);
      }
      const auto* previous = FindVariableById(oldVariables, definition.id);
      detail::Variables* destination = &replacement->variables;
      if (!definition.namespacePath.empty()) {
        destination = &namespaces.at(definition.namespacePath)->members;
      }
      detail::StoreVariable(*destination, definition.name, std::move(value),
                            previous, definition.id, definition.expression,
                            !definition.namespacePath.empty());
    } catch (const std::logic_error& error) {
      throw VariableEvaluationError{definition.id, error.what()};
    }
  }
  impl_ = std::move(replacement);
}

std::vector<VariableInfo> EvaluationContext::GetVariables() const {
  std::vector<VariableInfo> result;
  if (impl_) {
    CollectVariableInfo(impl_->variables, "", result);
  }
  return result;
}

std::vector<std::string> EvaluationContext::FindReferences(std::string_view variableId) const {
  const auto variables = GetVariables();
  auto target = std::find_if(variables.begin(), variables.end(),
                             [&](const auto& variable) {
                               return variable.definition.id == variableId;
                             });
  if (target == variables.end()) {
    throw std::invalid_argument("unknown variable id: " + std::string(variableId));
  }
  auto targetName = target->definition.namespacePath.empty()
                        ? target->definition.name
                        : target->definition.namespacePath + "." + target->definition.name;
  std::vector<std::string> references;
  for (const auto& variable : variables) {
    if (variable.definition.id != variableId &&
        HasVariableReference(variable.definition.expression, targetName)) {
      references.push_back(variable.definition.id);
    }
  }
  return references;
}

std::string RenameQualifiedVariableReferences(std::string_view source,
                                              std::string_view qualifiedName,
                                              std::string_view newName) {
  return detail::RenameQualifiedVariableReferences(source, qualifiedName, newName);
}

void EvaluationContext::RenameVariable(std::string_view variableId, std::string newName) {
  auto variables = GetVariables();
  auto target = std::find_if(variables.begin(), variables.end(),
                             [&](const auto& variable) {
                               return variable.definition.id == variableId;
                             });
  if (target == variables.end()) {
    throw std::invalid_argument("unknown variable id: " + std::string(variableId));
  }
  auto oldName = target->definition.namespacePath.empty()
                     ? target->definition.name
                     : target->definition.namespacePath + "." + target->definition.name;
  detail::Variables validateName;
  detail::StoreVariable(validateName, newName, units::precise_measurement{}, nullptr, {}, {},
                        !target->definition.namespacePath.empty());
  target->definition.name = std::move(newName);
  for (auto& variable : variables) {
    variable.definition.expression =
        detail::RenameQualifiedVariableReferences(variable.definition.expression, oldName,
                    target->definition.name);
  }
  std::vector<VariableDefinition> definitions;
  definitions.reserve(variables.size());
  for (auto& variable : variables) {
    definitions.push_back(std::move(variable.definition));
  }
  SetVariables(std::move(definitions));
}

EvaluationContext::Impl& EvaluationContext::MutableImpl() {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  } else if (impl_.use_count() > 1) {
    impl_ = std::make_shared<Impl>(*impl_);
  }
  return *impl_;
}

void EvaluationContext::SetVariable(std::string name, units::precise_measurement value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Pose2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Translation2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(
    std::string name, wpi::math::Polygon2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Rotation2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Transform2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Twist2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Rectangle2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Ellipse2d value) {
  auto& variables = MutableImpl().variables;
  auto existing = variables.find(name);
  detail::StoreVariable(variables, std::move(name), std::move(value),
                        existing == variables.end() ? nullptr : &existing->second);
}

units::precise_measurement EvaluationContext::Evaluate(
    std::string_view source, std::optional<units::precise_unit> expectedDimension) const {
  static const detail::Variables emptyVariables;
  auto result = EvaluateResult<units::precise_measurement>(*engineCache_,
                                         impl_ ? impl_->variables : emptyVariables, source);
  if (expectedDimension &&
      result.units().base_units() != expectedDimension->base_units()) {
    throw std::invalid_argument("expression result has unexpected dimension");
  }
  return result;
}

bool EvaluationContext::EvaluateBoolean(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<bool>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Pose2d EvaluationContext::EvaluatePose2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Pose2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Translation2d EvaluationContext::EvaluateTranslation2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Translation2d>(*engineCache_,
                                       impl_ ? impl_->variables : emptyVariables, source);
}

wpi::math::Polygon2d EvaluationContext::EvaluatePolygon2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<detail::Polygon2d>(*engineCache_,
                                           impl_ ? impl_->variables : emptyVariables, source);
}

Rotation2d EvaluationContext::EvaluateRotation2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Rotation2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Transform2d EvaluationContext::EvaluateTransform2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Transform2d>(*engineCache_,
                                     impl_ ? impl_->variables : emptyVariables, source);
}

Twist2d EvaluationContext::EvaluateTwist2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Twist2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Rectangle2d EvaluationContext::EvaluateRectangle2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Rectangle2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

Ellipse2d EvaluationContext::EvaluateEllipse2d(std::string_view source) const {
  static const detail::Variables emptyVariables;
  return EvaluateResult<Ellipse2d>(*engineCache_, impl_ ? impl_->variables : emptyVariables, source);
}

}  // namespace wpilib::expression
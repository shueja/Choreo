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
    if (record.declaredType->type == detail::VariableType::Quantity) {
      info.dimension = record.declaredType->dimension;
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

std::string RewriteVariableReferences(std::string_view source, std::string_view target,
                                      std::string_view replacement) {
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
  CollectReferenceEdits(*parsed, expanded, target, replacement, edits);
  if (edits.empty()) {
    return std::string(source);
  }
  std::sort(edits.begin(), edits.end(),
            [](const auto& left, const auto& right) { return left.first > right.first; });
  for (const auto& [offset, text] : edits) {
    expanded.replace(offset, target.find('.') == std::string_view::npos
                                 ? target.size()
                                 : target.substr(target.rfind('.') + 1).size(),
                     text);
  }
  return expanded;
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
      "quantity", "sin", "cos", "tan", "asin", "acos", "atan", "Pose2d",
      "Translation2d", "Rotation2d", "Transform2d", "Twist2d", "Rectangle2d", "Ellipse2d",
      "X", "Y",
      "Rotation", "Translation", "RotateBy", "RelativeTo", "TransformBy",
      "Distance", "Norm", "Radians", "Degrees", "Cos", "Sin", "Tan", "Inverse", "Log",
      "dx", "dy", "dtheta", "Exp", "Center", "XWidth", "YWidth", "XSemiAxis", "YSemiAxis",
      "FocalPoints", "Intersects", "Contains", "Nearest", "RotateAround", "SquaredDistance",
      "SquaredNorm", "Dot", "Cross", "Angle"};
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
      return cache.engine.template eval<Result>(expanded);
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
    expanded.push_back(detail::ExpandUnitSuffixes(definition.expression));
    try {
      expressions.push_back(parser.parse(expanded.back()));
    } catch (const chaiscript::exception::eval_error& error) {
      throw SyntaxError(error);
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

  chaiscript::ChaiScript evaluator{{}, {},
                                  {chaiscript::Options::No_Load_Modules,
                                   chaiscript::Options::No_External_Scripts},
                                  {chaiscript::Library_Options::No_IO}};
  detail::ConfigureEngine(evaluator, replacement->variables);
  for (auto index : order) {
    const auto& definition = definitions[index];
    auto qualifiedName = definition.namespacePath.empty()
                             ? definition.name
                             : definition.namespacePath + "." + definition.name;
    ValidateExpression(*expressions[index], replacement->variables, namespacePaths,
                       declaredVariables);
    detail::ConfigureGlobals(evaluator, replacement->variables);
    chaiscript::Boxed_Value boxed;
    try {
      boxed = evaluator.eval(expanded[index]);
    } catch (const chaiscript::exception::eval_error& error) {
      throw SyntaxError(error);
    }
    auto value = UnboxVariableValue(evaluator, boxed, qualifiedName);
    const auto* previous = FindVariableById(oldVariables, definition.id);
    detail::Variables* destination = &replacement->variables;
    if (!definition.namespacePath.empty()) {
      destination = &namespaces.at(definition.namespacePath)->members;
    }
    detail::StoreVariable(*destination, definition.name, std::move(value), previous,
                          definition.id, definition.expression,
                          !definition.namespacePath.empty());
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
        RewriteVariableReferences(variable.definition.expression, oldName,
                                  target->definition.name);
  }
  std::vector<VariableDefinition> definitions;
  definitions.reserve(variables.size());
  for (auto& variable : variables) {
    definitions.push_back(std::move(variable.definition));
  }
  SetVariables(std::move(definitions));
}

void EvaluationContext::SetVariable(std::string name, units::precise_measurement value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Pose2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Translation2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Rotation2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Transform2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Twist2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Rectangle2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
}

void EvaluationContext::SetVariable(std::string name, Ellipse2d value) {
  if (!impl_) {
    impl_ = std::make_shared<Impl>();
  }
  auto existing = impl_->variables.find(name);
  detail::StoreVariable(impl_->variables, std::move(name), std::move(value),
                        existing == impl_->variables.end() ? nullptr : &existing->second);
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
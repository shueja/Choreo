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
    offsets.push_back(SourceOffset(source, statement.get().start()));
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

void EvaluationContext::SetSetupScript(std::string_view source) {
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
  auto ordered = OrderSetupDeclarations(*parsed, expanded);
  try {
    engine.eval(ordered);
  } catch (const chaiscript::exception::eval_error& error) {
    throw SyntaxError(error);
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
      detail::StoreVariable(destination, name, std::move(variable), existing);
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
      store(replacement->variables, name, name, value);
    }
  }
  for (const auto& [name, value] : engine.get_scripting_objects()) {
    if (!initialObjects.contains(name) && !replacement->variables.contains(name)) {
      store(replacement->variables, name, name, value);
    }
  }
  impl_ = std::move(replacement);
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
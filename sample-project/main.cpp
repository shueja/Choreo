#include <wpilib-expression/expression.hpp>

#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using wpilib::expression::EvaluationContext;
using wpilib::expression::From;
using wpilib::expression::In;
using wpilib::expression::VariableDefinition;

namespace {

int RunSmokeTest() {
  EvaluationContext context;
  context.SetVariables({{"offset", "", "offset", "3 ft"}});
  auto length = From(1.0, units::precise::m).units();
  auto result = context.Evaluate("2 m + offset", length);
  if (auto meters = In(result, units::precise::m);
      !meters || std::abs(*meters - 2.9144) > 1e-10) {
    std::println(stderr, "expression library smoke test failed");
    return 1;
  }

  auto pose = context.EvaluatePose2d("Pose2d(3 m, 4 m, 90 deg)");
  if (std::abs(pose.X().value() - 3.0) > 1e-10 ||
      std::abs(pose.Y().value() - 4.0) > 1e-10) {
    std::println(stderr, "WPILib geometry smoke test failed");
    return 1;
  }
  return 0;
}

bool LoadVariables(EvaluationContext& context, const char* variablesPath) {
  std::ifstream setupFile(variablesPath, std::ios::binary);
  if (!setupFile) {
    std::println(stderr, "Could not open variables file: {}", variablesPath);
    return false;
  }
  try {
    std::vector<VariableDefinition> definitions;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(setupFile, line)) {
      ++lineNumber;
      if (line.empty() || line.front() == '#') {
        continue;
      }
      auto separator = line.find('=');
      if (separator == std::string::npos) {
        throw std::invalid_argument("expected '=' on line " + std::to_string(lineNumber));
      }
      std::istringstream fields(line.substr(0, separator));
      VariableDefinition definition;
      fields >> definition.id >> definition.namespacePath >> definition.name;
      std::string extra;
      if (!fields || (fields >> extra)) {
        throw std::invalid_argument("expected id, namespace, and name on line " +
                                    std::to_string(lineNumber));
      }
      if (definition.namespacePath == "-") {
        definition.namespacePath.clear();
      }
      auto expression = line.substr(separator + 1);
      auto first = expression.find_first_not_of(" \t");
      if (first == std::string::npos) {
        throw std::invalid_argument("missing expression on line " +
                                    std::to_string(lineNumber));
      }
      definition.expression = expression.substr(first);
      definitions.push_back(std::move(definition));
    }
    if (setupFile.bad()) {
      throw std::runtime_error("could not read variables file");
    }
    context.SetVariables(std::move(definitions));
  } catch (const std::exception& error) {
    std::println(stderr, "Invalid variables file {}: {}", variablesPath, error.what());
    return false;
  }
  return true;
}

int RunRepl(const char* variablesPath) {
  EvaluationContext context;
  if (variablesPath != nullptr && !LoadVariables(context, variablesPath)) {
    return 1;
  }
  std::string source;
  while (std::print("> "), std::getline(std::cin, source)) {
    if (source == ":quit") {
      break;
    }
    if (source == ":reload") {
      if (variablesPath != nullptr && LoadVariables(context, variablesPath)) {
        std::println("Variables reloaded.");
      } else if (variablesPath == nullptr) {
        std::println(stderr, "No variables file specified.");
      }
      continue;
    }
    try {
      auto result = context.Evaluate(source);
      std::println("{} {}", result.value() * result.units().multiplier(),
                   units::to_string(result.units()));
    } catch (const std::exception& error) {
      std::println(stderr, "{}", error.what());
    }
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--smoke") {
    return RunSmokeTest();
  }
  if (argc == 1) {
    return RunRepl(nullptr);
  }
  if (argc == 3 && std::string_view(argv[1]) == "--variables") {
    return RunRepl(argv[2]);
  }
  std::println(stderr, "Usage: expression_sample [--variables <file> | --smoke]");
  return 1;
}
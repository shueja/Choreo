#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "wpilib-expression/expression.hpp"

using wpilib::expression::EvaluationContext;
using wpilib::expression::From;
using wpilib::expression::In;
using wpilib::expression::VariableDefinition;

namespace {

std::vector<VariableDefinition> MakeVariables(std::size_t expressionCount, int offset) {
  std::vector<VariableDefinition> variables;
  variables.reserve(expressionCount);
  for (std::size_t i = 0; i < expressionCount; ++i) {
    auto name = "value" + std::to_string(i);
    variables.push_back({name, "", name,
                         std::to_string(offset + static_cast<int>(i)) + " m"});
  }
  return variables;
}

void Benchmark(std::size_t expressionCount) {
  constexpr int rounds = 3;
  EvaluationContext context;
  std::vector<std::string> expressions;
  expressions.reserve(expressionCount);
  for (std::size_t i = 0; i < expressionCount; ++i) {
    expressions.push_back("value" + std::to_string(i));
  }
  const auto length = From(1.0, units::precise::m).units();

  auto setupTime = std::chrono::steady_clock::duration::zero();
  auto evaluationTime = std::chrono::steady_clock::duration::zero();
  double checksum = 0.0;
  for (int round = 0; round < rounds; ++round) {
    auto start = std::chrono::steady_clock::now();
    context.SetVariables(
        MakeVariables(expressionCount, round * static_cast<int>(expressionCount)));
    setupTime += std::chrono::steady_clock::now() - start;

    start = std::chrono::steady_clock::now();
    for (const auto& expression : expressions) {
      auto result = In(context.Evaluate(expression, length), units::precise::m);
      if (!result) {
        throw std::runtime_error(
            "benchmark expression returned an incompatible dimension");
      }
      checksum += *result;
    }
    evaluationTime += std::chrono::steady_clock::now() - start;
  }

  const auto milliseconds = [](auto duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
  };
  std::cout << expressionCount << " expressions, " << rounds
            << " setup replacements: setup " << milliseconds(setupTime)
            << " ms, evaluation " << milliseconds(evaluationTime)
            << " ms, checksum " << checksum << '\n';
}

}  // namespace

int main() {
  try {
    constexpr std::array expressionCounts{100, 500, 1000};
    for (const auto expressionCount : expressionCounts) {
      Benchmark(expressionCount);
    }
  } catch (const std::exception& error) {
    std::cerr << "Benchmark failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}

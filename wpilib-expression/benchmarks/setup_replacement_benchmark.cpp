#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "wpilib-expression/expression.hpp"

using wpilib::expression::EvaluationContext;
using wpilib::expression::ExpressionDefinition;
using wpilib::expression::ExpressionInfo;
using wpilib::expression::From;
using wpilib::expression::In;

namespace {

constexpr std::size_t kVariableCount = 50;
constexpr int kRounds = 5;

void SetInitialVariables(EvaluationContext& context) {
  for (std::size_t index = 0; index < kVariableCount; ++index) {
    context.SetVariable(
        "input" + std::to_string(index),
        From(static_cast<double>(index + 1), units::precise::m));
  }
}

std::vector<std::string> MakeExpressions(std::size_t expressionCount,
                                         bool dense) {
  std::vector<std::string> expressions;
  expressions.reserve(expressionCount);
  for (std::size_t index = 0; index < expressionCount; ++index) {
    const auto first = dense ? 0 : index % kVariableCount;
    const auto second = (first + 1) % kVariableCount;
    expressions.push_back("input" + std::to_string(first) + " + input" +
                          std::to_string(second));
  }
  return expressions;
}

double SumCachedExpressions(const std::vector<ExpressionInfo>& expressions) {
  double sum = 0.0;
  for (const auto& expression : expressions) {
    const auto* value =
        std::get_if<units::precise_measurement>(&expression.value);
    if (value == nullptr) {
      throw std::runtime_error("benchmark expression returned a non-quantity");
    }
    sum += In(*value, units::precise::m).value();
  }
  return sum;
}

double SumEvaluatedExpressions(const EvaluationContext& context,
                               const std::vector<std::string>& expressions) {
  double sum = 0.0;
  for (const auto& expression : expressions) {
    auto result = In(context.Evaluate(expression), units::precise::m);
    if (!result) {
      throw std::runtime_error(
          "benchmark expression returned an incompatible dimension");
    }
    sum += *result;
  }
  return sum;
}

void Benchmark(std::size_t expressionCount, bool dense) {
  EvaluationContext incremental;
  EvaluationContext full;
  SetInitialVariables(incremental);
  SetInitialVariables(full);
  auto sources = MakeExpressions(expressionCount, dense);
  std::vector<ExpressionDefinition> definitions;
  definitions.reserve(expressionCount);
  for (std::size_t index = 0; index < expressionCount; ++index) {
    auto name = "result" + std::to_string(index);
    definitions.push_back({name, name, sources[index]});
  }
  incremental.SetExpressions(std::move(definitions));

  double incrementalChecksum = 0.0;
  auto incrementalStart = std::chrono::steady_clock::now();
  for (int round = 0; round < kRounds; ++round) {
    incremental.SetVariable(
        "input0", From(static_cast<double>(100 + round), units::precise::m));
    incrementalChecksum += SumCachedExpressions(incremental.GetExpressions());
  }
  auto incrementalElapsed = std::chrono::steady_clock::now() - incrementalStart;

  double fullChecksum = 0.0;
  auto fullStart = std::chrono::steady_clock::now();
  for (int round = 0; round < kRounds; ++round) {
    full.SetVariable("input0",
                     From(static_cast<double>(100 + round), units::precise::m));
    fullChecksum += SumEvaluatedExpressions(full, sources);
  }
  auto fullElapsed = std::chrono::steady_clock::now() - fullStart;

  if (std::abs(incrementalChecksum - fullChecksum) > 1e-8) {
    throw std::runtime_error(
        "incremental and full evaluation checksums differ");
  }
  const auto milliseconds = [](auto duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
  };
  std::cout << expressionCount << " expressions, " << kVariableCount
            << " variables, " << (dense ? "dense" : "sparse") << " dependency, "
            << kRounds << " single-variable updates: incremental "
            << milliseconds(incrementalElapsed) << " ms, full "
            << milliseconds(fullElapsed) << " ms, checksum "
            << incrementalChecksum << '\n';
}

}  // namespace

int main() {
  try {
    constexpr std::array expressionCounts{750, 1000};
    for (const auto expressionCount : expressionCounts) {
      Benchmark(expressionCount, false);
      Benchmark(expressionCount, true);
    }
  } catch (const std::exception& error) {
    std::cerr << "Benchmark failed: " << error.what() << '\n';
    return 1;
  }
  return 0;
}

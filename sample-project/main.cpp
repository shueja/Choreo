#include <wpilib-expression/expression.hpp>

#include <cmath>
#include <iostream>
#include <string>

using wpilib::expression::EvaluationContext;
using wpilib::expression::Quantity;

namespace {

int RunSmokeTest() {
  EvaluationContext context;
  auto length = Quantity::From(1.0, "m").dimension;
  auto result = context.Evaluate("2 m + 3 ft", length);
  if (std::abs(result.In("m") - 2.9144) > 1e-10) {
    std::cerr << "expression library smoke test failed\n";
    return 1;
  }

  auto pose = context.EvaluatePose2d("Pose2d(3 m, 4 m, 90 deg)");
  if (std::abs(pose.X().value() - 3.0) > 1e-10 ||
      std::abs(pose.Y().value() - 4.0) > 1e-10) {
    std::cerr << "WPILib geometry smoke test failed\n";
    return 1;
  }
  return 0;
}

void RunRepl() {
  EvaluationContext context;
  std::string source;
  while (std::cout << "> " && std::getline(std::cin, source)) {
    if (source == ":quit") {
      break;
    }
    try {
      auto result = context.Evaluate(source);
      std::cout << result.siValue << " SI [";
      for (std::size_t index = 0; index < result.dimension.exponents.size(); ++index) {
        if (index != 0) {
          std::cout << ", ";
        }
        std::cout << result.dimension.exponents[index];
      }
      std::cout << "]\n";
    } catch (const std::exception& error) {
      std::cerr << error.what() << '\n';
    }
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "--smoke") {
    return RunSmokeTest();
  }
  RunRepl();
  return 0;
}
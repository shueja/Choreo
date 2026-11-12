#include <wpilib-expression/expression.hpp>

#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <string_view>

using wpilib::expression::EvaluationContext;
using wpilib::expression::From;
using wpilib::expression::In;

namespace {

int RunSmokeTest() {
  EvaluationContext context;
  context.SetSetupScript("var offset = 3 ft;");
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

bool LoadSetup(EvaluationContext& context, const char* setupPath) {
  if (setupPath == nullptr) {
    std::println(stderr, "No setup file specified; start with --setup <file>.");
    return false;
  }
  std::ifstream setupFile(setupPath, std::ios::binary);
  if (!setupFile) {
    std::println(stderr, "Could not open setup file: {}", setupPath);
    return false;
  }
  std::ostringstream setup;
  setup << setupFile.rdbuf();
  if (setupFile.bad() || setup.bad()) {
    std::println(stderr, "Could not read setup file: {}", setupPath);
    return false;
  }
  try {
    context.SetSetupScript(setup.str());
  } catch (const std::exception& error) {
    std::println(stderr, "Invalid setup file {}: {}", setupPath, error.what());
    return false;
  }
  return true;
}

int RunRepl(const char* setupPath) {
  EvaluationContext context;
  if (setupPath != nullptr && !LoadSetup(context, setupPath)) {
    return 1;
  }
  std::string source;
  while (std::print("> "), std::getline(std::cin, source)) {
    if (source == ":quit") {
      break;
    }
    if (source == ":reload") {
      if (LoadSetup(context, setupPath)) {
        std::println("Setup reloaded.");
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
  if (argc == 3 && std::string_view(argv[1]) == "--setup") {
    return RunRepl(argv[2]);
  }
  std::println(stderr, "Usage: expression_sample [--setup <file> | --smoke]");
  return 1;
}
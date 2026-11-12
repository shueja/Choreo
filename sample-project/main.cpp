#include <wpilib-expression/expression.hpp>

#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
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
  auto length = From(1.0, "m").units();
  auto result = context.Evaluate("2 m + offset", length);
  if (std::abs(In(result, "m") - 2.9144) > 1e-10) {
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

bool LoadSetup(EvaluationContext& context, const char* setupPath) {
  if (setupPath == nullptr) {
    std::cerr << "No setup file specified; start with --setup <file>.\n";
    return false;
  }
  std::ifstream setupFile(setupPath, std::ios::binary);
  if (!setupFile) {
    std::cerr << "Could not open setup file: " << setupPath << '\n';
    return false;
  }
  std::ostringstream setup;
  setup << setupFile.rdbuf();
  if (setupFile.bad() || setup.bad()) {
    std::cerr << "Could not read setup file: " << setupPath << '\n';
    return false;
  }
  try {
    context.SetSetupScript(setup.str());
  } catch (const std::exception& error) {
    std::cerr << "Invalid setup file " << setupPath << ": " << error.what() << '\n';
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
  while (std::cout << "> " && std::getline(std::cin, source)) {
    if (source == ":quit") {
      break;
    }
    if (source == ":reload") {
      if (LoadSetup(context, setupPath)) {
        std::cout << "Setup reloaded.\n";
      }
      continue;
    }
    try {
      auto result = context.Evaluate(source);
      std::cout << result.value() * result.units().multiplier() << " "
                << units::to_string(result.units()) << "\n";
    } catch (const std::exception& error) {
      std::cerr << error.what() << '\n';
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
  std::cerr << "Usage: expression_sample [--setup <file> | --smoke]\n";
  return 1;
}
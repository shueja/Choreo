#include "wpilib-expression/expression.hpp"

#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

#include <wpi/units/angle.hpp>
#include <wpi/units/length.hpp>

using wpilib::expression::EvaluationContext;
using wpilib::expression::Quantity;
using wpi::math::Pose2d;

int main() {
  EvaluationContext setup;
  setup.SetVariable("old", Quantity::From(1.0, "m"));
  setup.SetSetupScript("// 2 unknown_units\nvar base = 2 m; /* 3 unknown_units */ "
               "var offset = 3 ft; var total = base + offset; "
                       "var pose = Pose2d(3 m, 4 m, 90 deg); var gain = 2; "
                       "var heading = pose.Rotation().Degrees(); "
                       "global shared = 4 m; const var extra = 1 m; auto combined = total + extra;");
  if (std::abs(setup.Evaluate("base + offset").In("m") - 2.9144) > 1e-10 ||
      std::abs(setup.Evaluate("total").In("m") - 2.9144) > 1e-10 ||
      std::abs(setup.Evaluate("pose.X()").In("m") - 3.0) > 1e-10 ||
      std::abs(setup.Evaluate("pose.Rotation().Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(setup.Evaluate("pose.Rotation().Degrees() / deg").siValue - 90.0) > 1e-10 ||
      std::abs(setup.Evaluate("heading").In("deg") - 90.0) > 1e-10 ||
      std::abs(setup.Evaluate("gain").siValue - 2.0) > 1e-10 ||
      std::abs(setup.Evaluate("shared").In("m") - 4.0) > 1e-10 ||
      std::abs(setup.Evaluate("combined").In("m") - 3.9144) > 1e-10) {
    std::cerr << "setup script returned incorrect variables\n";
    return 1;
  }
  bool setupFailureRejected = false;
  try {
    setup.SetSetupScript("var invalid = 1 m + 2 s;");
  } catch (const std::exception&) {
    setupFailureRejected = true;
  }
  if (!setupFailureRejected || std::abs(setup.Evaluate("base").In("m") - 2.0) > 1e-10) {
    std::cerr << "failed setup changed the context\n";
    return 1;
  }
  bool unsupportedSetupRejected = false;
  try {
    setup.SetSetupScript("var bad = [1, 2];");
  } catch (const std::exception&) {
    unsupportedSetupRejected = true;
  }
  if (!unsupportedSetupRejected || std::abs(setup.Evaluate("base").In("m") - 2.0) > 1e-10) {
    std::cerr << "unsupported setup value was accepted or changed the context\n";
    return 1;
  }
  for (const char* script : {"var count = 0; while (count < 1) { count += 1; }",
                             "var value = eval(\"1\");", "var value = use(\"missing.chai\");",
                             "load_module(\"missing\");", "var value = 1; value = 2;",
                             "def calculate() { return 1; } var value = calculate();",
                             "var value = (fun() { return 1; })();",
                             "var value = quantity(1, \"${eval(1)}\");",
                             "var value = get_objects();", "var value = 1 / 0; print(\"no\");",
                             "var value = 1; var value = 2;", "var value;"}) {
    bool rejected = false;
    try {
      setup.SetSetupScript(script);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    if (!rejected || std::abs(setup.Evaluate("base").In("m") - 2.0) > 1e-10) {
      std::cerr << "unsafe setup was accepted or changed the context\n";
      return 1;
    }
  }
  bool reservedSetupRejected = false;
  try {
    setup.SetSetupScript("var m = 2;");
  } catch (const std::exception&) {
    reservedSetupRejected = true;
  }
  if (!reservedSetupRejected || std::abs(setup.Evaluate("base").In("m") - 2.0) > 1e-10) {
    std::cerr << "reserved setup name was accepted or changed the context\n";
    return 1;
  }
  for (const char* name : {"Cos", "Degrees"}) {
    bool methodNameRejected = false;
    try {
      setup.SetVariable(name, Quantity::From(1.0, "m"));
    } catch (const std::invalid_argument&) {
      methodNameRejected = true;
    }
    if (!methodNameRejected) {
      std::cerr << "geometry method name was accepted as a variable\n";
      return 1;
    }
  }
  setup.SetSetupScript("var replacement = 5 m;");
  bool oldSetupRejected = false;
  try {
    setup.Evaluate("base");
  } catch (const std::exception&) {
    oldSetupRejected = true;
  }
  if (!oldSetupRejected || std::abs(setup.Evaluate("replacement").In("m") - 5.0) > 1e-10) {
    std::cerr << "setup did not replace previous variables\n";
    return 1;
  }

  EvaluationContext context;
  auto length = Quantity::From(1.0, "m").dimension;
  auto mixedLength = context.Evaluate("2 * m + 3 * ft", length);
  if (std::abs(mixedLength.In("m") - 2.9144) > 1e-10) {
    std::cerr << "mixed-unit addition returned the wrong value\n";
    return 1;
  }

  bool incompatibleAdditionRejected = false;
  try {
    context.Evaluate("2 * m + 3 * s");
  } catch (const std::exception&) {
    incompatibleAdditionRejected = true;
  }
  if (!incompatibleAdditionRejected) {
    std::cerr << "incompatible addition was accepted\n";
    return 1;
  }

  bool expectedDimensionRejected = false;
  try {
    context.Evaluate("2 * m * 3 * m", length);
  } catch (const std::invalid_argument&) {
    expectedDimensionRejected = true;
  }
  if (!expectedDimensionRejected) {
    std::cerr << "unexpected result dimension was accepted\n";
    return 1;
  }

  context.SetVariable("offset", Quantity::From(3.0, "ft"));
  auto withVariable = context.Evaluate("2 * m + offset", length);
  if (std::abs(withVariable.In("m") - 2.9144) > 1e-10) {
    std::cerr << "registered variable returned the wrong value\n";
    return 1;
  }
  context.SetVariable("offset", Quantity::From(1.0, "m"));
  if (std::abs(context.Evaluate("offset", length).In("m") - 1.0) > 1e-10) {
    std::cerr << "updated variable returned the wrong value\n";
    return 1;
  }
  for (const char* expression : {"var temporary = 2; 2 * m", "offset = 5 * m",
                                 "def temporaryFunction() { return 2 * m; } temporaryFunction()",
                                 "eval(\"1 * m\")", "++offset", "set_global(2 * m, \"offset\")",
                                 "(fun() { return 2 * m; })()", "1 * m; 2 * m",
                                 "quantity(1, \"${set_global(2, 1)}\")"}) {
    bool rejected = false;
    try {
      context.Evaluate(expression);
    } catch (const std::exception&) {
      rejected = true;
    }
    if (!rejected || std::abs(context.Evaluate("offset").In("m") - 1.0) > 1e-10) {
      std::cerr << "state-changing evaluation was accepted or changed the context\n";
      return 1;
    }
  }
  if (!context.EvaluateBoolean("1 m == 100 cm") ||
      !context.EvaluateBoolean("1 m < 2 m && 2 m > 1 m")) {
    std::cerr << "read-only comparison was rejected\n";
    return 1;
  }

  EvaluationContext first;
  EvaluationContext second = first;
  first.SetVariable("distance", Quantity::From(1.0, "m"));
  second.SetVariable("distance", Quantity::From(2.0, "m"));
  std::atomic<bool> concurrentFailed{false};
  auto evaluateRepeatedly = [&](const EvaluationContext& current, double expected) {
    try {
      for (int iteration = 0; iteration < 5; ++iteration) {
        if (std::abs(current.Evaluate("distance").In("m") - expected) > 1e-10) {
          concurrentFailed = true;
        }
      }
    } catch (const std::exception&) {
      concurrentFailed = true;
    }
  };
  std::thread firstThread(evaluateRepeatedly, std::cref(first), 1.0);
  std::thread secondThread(evaluateRepeatedly, std::cref(second), 2.0);
  firstThread.join();
  secondThread.join();
  if (concurrentFailed) {
    std::cerr << "concurrent copied contexts returned the wrong value\n";
    return 1;
  }
  EvaluationContext moved = std::move(first);
  if (std::abs(moved.Evaluate("distance").In("m") - 1.0) > 1e-10 ||
      std::abs(first.Evaluate("1 m").In("m") - 1.0) > 1e-10) {
    std::cerr << "moved context or moved-from context returned the wrong value\n";
    return 1;
  }

  if (std::abs(context.Evaluate("2 m").In("m") - 2.0) > 1e-10) {
    std::cerr << "bare length suffix returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("90 deg").In("rad") - std::acos(-1.0) / 2.0) > 1e-10) {
    std::cerr << "bare angle suffix returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("6000 RPM").In("rad/s") - 200.0 * std::acos(-1.0)) > 1e-9) {
    std::cerr << "rotational speed conversion returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("1 in").In("m") - 0.0254) > 1e-10 ||
      std::abs(context.Evaluate("100 cm").In("m") - 1.0) > 1e-10) {
    std::cerr << "alternate length units returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("1 min").In("s") - 60.0) > 1e-10 ||
      std::abs(context.Evaluate("1 ms").In("s") - 1e-3) > 1e-10) {
    std::cerr << "alternate time units returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("1 lb").In("kg") - 0.45359237) > 1e-10 ||
      std::abs(context.Evaluate("500 mA").In("A") - 0.5) > 1e-10) {
    std::cerr << "alternate mass or current units returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("1 turn").In("rad") - 2.0 * std::acos(-1.0)) > 1e-10 ||
      std::abs(context.Evaluate("2 kV").In("V") - 2000.0) > 1e-10) {
    std::cerr << "alternate angle or voltage units returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("1 kW").In("W") - 1000.0) > 1e-10 ||
      std::abs(context.Evaluate("1 kPa").In("Pa") - 1000.0) > 1e-10) {
    std::cerr << "derived power or pressure units returned the wrong value\n";
    return 1;
  }
  auto feet = Quantity::From(1.0, "m").TryAs<wpi::units::foot_t>();
  if (!feet || std::abs(feet->value() - (1.0 / 0.3048)) > 1e-10) {
    std::cerr << "WPILib foot conversion returned the wrong value\n";
    return 1;
  }
  auto radians = Quantity::From(90.0, "deg").TryAs<wpi::units::radian_t>();
  if (!radians || std::abs(radians->value() - std::acos(-1.0) / 2.0) > 1e-10) {
    std::cerr << "WPILib radian conversion returned the wrong value\n";
    return 1;
  }
  if (Quantity::From(1.0, "s").TryAs<wpi::units::meter_t>()) {
    std::cerr << "incompatible WPILib unit conversion succeeded\n";
    return 1;
  }
  wpi::units::meter_t output;
  if (!Quantity::From(2.0, "m").TryAs(output) || std::abs(output.value() - 2.0) > 1e-10) {
    std::cerr << "WPILib output-parameter conversion returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("2 m/s^2").In("m/s^2") - 2.0) > 1e-10) {
    std::cerr << "compound acceleration unit returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("0.018 N*m/A").In("N*m/A") - 0.018) > 1e-10) {
    std::cerr << "compound motor constant returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("(2e0 m + 3 ft)").In("m") - 2.9144) > 1e-10) {
    std::cerr << "scientific notation or grouping returned the wrong value\n";
    return 1;
  }

  bool unknownSuffixRejected = false;
  try {
    context.Evaluate("2 furlong");
  } catch (const std::invalid_argument&) {
    unknownSuffixRejected = true;
  }
  if (!unknownSuffixRejected) {
    std::cerr << "unknown unit suffix was accepted\n";
    return 1;
  }

  bool unitNameCollisionRejected = false;
  try {
    context.SetVariable("m", Quantity::From(1.0, "m"));
  } catch (const std::invalid_argument&) {
    unitNameCollisionRejected = true;
  }
  if (!unitNameCollisionRejected) {
    std::cerr << "unit name was accepted as a variable\n";
    return 1;
  }
  bool constructorNameCollisionRejected = false;
  try {
    context.SetVariable("Pose2d", Quantity::From(1.0, "m"));
  } catch (const std::invalid_argument&) {
    constructorNameCollisionRejected = true;
  }
  if (!constructorNameCollisionRejected) {
    std::cerr << "geometry constructor name was accepted as a variable\n";
    return 1;
  }

  auto area = context.Evaluate("(2 m)^2");
  if (std::abs(area.In("m^2") - 4.0) > 1e-10) {
    std::cerr << "integer quantity power returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("sin(90 deg)").siValue - 1.0) > 1e-10) {
    std::cerr << "angle-aware sine returned the wrong value\n";
    return 1;
  }
  if (!context.EvaluateBoolean("2 ft < 1 m")) {
    std::cerr << "comparison across compatible units returned the wrong value\n";
    return 1;
  }
  bool incompatibleComparisonRejected = false;
  try {
    context.EvaluateBoolean("2 m < 3 s");
  } catch (const std::exception&) {
    incompatibleComparisonRejected = true;
  }
  if (!incompatibleComparisonRejected) {
    std::cerr << "comparison across incompatible dimensions was accepted\n";
    return 1;
  }

  Pose2d pose;
  try {
    pose = context.EvaluatePose2d("Pose2d(3 m, 4 m, 90 deg)");
  } catch (const std::exception& error) {
    std::cerr << "pose constructor failed: " << error.what() << '\n';
    return 1;
  }
  context.SetVariable("pose", pose);
  auto origin = context.EvaluatePose2d("Pose2d(0 m, 0 m, 0 deg)");
  context.SetVariable("origin", origin);
  auto rotation = context.EvaluateRotation2d("Rotation2d(90 deg)");
  context.SetVariable("rotation", rotation);
  if (std::abs(context.Evaluate("pose.X()", length).In("m") - 3.0) > 1e-10) {
    std::cerr << "Pose2d::X returned the wrong value\n";
    return 1;
  }

  auto relativePose = context.EvaluatePose2d("pose.RotateBy(rotation).RelativeTo(origin)");
  if (std::abs(relativePose.X().value() + 4.0) > 1e-10 ||
      std::abs(relativePose.Y().value() - 3.0) > 1e-10) {
    std::cerr << "WPILib pose method chain returned the wrong translation\n";
    return 1;
  }

  auto translation = context.EvaluateTranslation2d("Translation2d(3 m, 4 m)");
  auto otherTranslation = context.EvaluateTranslation2d("Translation2d(0 m, 0 m)");
  context.SetVariable("translation", translation);
  context.SetVariable("other", otherTranslation);
  if (std::abs(context.Evaluate("translation.Distance(other)", length).In("m") - 5.0) >
      1e-10) {
    std::cerr << "Translation2d::Distance returned the wrong value\n";
    return 1;
  }

  bool invalidGeometryRejected = false;
  try {
    context.EvaluatePose2d("Pose2d(3 s, 4 m, 90 deg)");
  } catch (const std::exception&) {
    invalidGeometryRejected = true;
  }
  if (!invalidGeometryRejected || context.EvaluatePose2d("pose") != pose) {
    std::cerr << "invalid geometry input was accepted or corrupted the context\n";
    return 1;
  }

  return 0;
}
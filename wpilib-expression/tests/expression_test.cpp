#include "wpilib-expression/expression.hpp"

#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
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
                       "var heading = pose.Rotation().Degrees(); var targetX = pose.X(); "
                       "global shared = 4 m; const var extra = 1 m; auto combined = total + extra;");
  if (std::abs(setup.Evaluate("base + offset").In("m") - 2.9144) > 1e-10 ||
      std::abs(setup.Evaluate("total").In("m") - 2.9144) > 1e-10 ||
      std::abs(setup.Evaluate("pose.X()").In("m") - 3.0) > 1e-10 ||
      std::abs(setup.Evaluate("pose.Rotation().Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(setup.Evaluate("pose.Rotation().Degrees() / deg").siValue - 90.0) > 1e-10 ||
      std::abs(setup.Evaluate("heading").In("deg") - 90.0) > 1e-10 ||
      std::abs(setup.Evaluate("targetX").In("m") - 3.0) > 1e-10 ||
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
  if (std::abs(context.Evaluate("((2 m)^2) * 3 m").In("m^3") - 12.0) > 1e-10 ||
      std::abs(context.Evaluate("3 m * ((2 m)^2)").In("m^3") - 12.0) > 1e-10 ||
      std::abs(context.Evaluate("10000000000 * m").In("m") - 1e10) > 1e-3) {
    std::cerr << "compound quantity arithmetic returned the wrong value\n";
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
  auto translationFromPose = context.EvaluateTranslation2d("Translation2d(pose.X(), 2 ft)");
  if (std::abs(translationFromPose.X().value() - 3.0) > 1e-10 ||
      std::abs(translationFromPose.Y().value() - 0.6096) > 1e-10) {
    std::cerr << "typed Translation2d conversion returned the wrong value\n";
    return 1;
  }
  bool invalidTranslationRejected = false;
  try {
    context.EvaluateTranslation2d("Translation2d(pose.X(), 2 s)");
  } catch (const std::exception&) {
    invalidTranslationRejected = true;
  }
  if (!invalidTranslationRejected) {
    std::cerr << "typed Translation2d accepted an incompatible dimension\n";
    return 1;
  }
  if (std::abs(context.Evaluate("Rotation2d(90 deg).Radians()").In("deg") - 90.0) > 1e-10 ||
      std::abs(context.Evaluate("Transform2d(3 ft, 4 m, 90 deg).X()").In("m") - 0.9144) >
          1e-10 ||
      std::abs(context.Evaluate("Transform2d(3 ft, 4 m, 90 deg).Y()").In("m") - 4.0) >
        1e-10 ||
      std::abs(context.Evaluate("Twist2d(3 m, 4 m, 90 deg).dx()").In("m") - 3.0) > 1e-10 ||
      std::abs(context.Evaluate("Twist2d(3 m, 4 m, 90 deg).dy()").In("m") - 4.0) > 1e-10 ||
      std::abs(context.Evaluate("Twist2d(3 m, 4 m, 90 deg).dtheta()").In("deg") - 90.0) >
        1e-10 ||
      std::abs(context.Evaluate("pose.X() + pose.Y()").In("m") - 7.0) > 1e-10 ||
      !context.EvaluateBoolean("pose.X() == 3 m")) {
    std::cerr << "typed geometry conversion returned the wrong value\n";
    return 1;
  }
  if (std::abs(context.Evaluate("Pose2d().X()").In("m")) > 1e-10 ||
      std::abs(context.Evaluate("Translation2d().Y()").In("m")) > 1e-10 ||
      std::abs(context.Evaluate("Rotation2d().Radians()").In("rad")) > 1e-10 ||
      std::abs(context.Evaluate("Transform2d().X()").In("m")) > 1e-10 ||
      std::abs(context.Evaluate("Rotation2d(0.0, 1.0).Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(context.Evaluate("Pose2d(Translation2d(3 m, 4 m), Rotation2d(90 deg)).X()")
                   .In("m") - 3.0) > 1e-10 ||
      std::abs(context.Evaluate("Transform2d(Translation2d(3 m, 4 m), Rotation2d(90 deg)).Y()")
                   .In("m") - 4.0) > 1e-10 ||
      std::abs(context.Evaluate("Transform2d(Pose2d(), Pose2d(3 m, 4 m, 90 deg)).X()")
                   .In("m") - 3.0) > 1e-10) {
    std::cerr << "geometry constructor overload returned the wrong value\n";
    return 1;
  }
  context.SetVariable("nonfiniteLength",
                      Quantity{std::numeric_limits<double>::infinity(), length});
  context.SetVariable("nonfinitePose",
                      Pose2d{wpi::units::meter_t{std::numeric_limits<double>::infinity()},
                             wpi::units::meter_t{0.0}, wpi::math::Rotation2d{}});
  for (const char* expression : {"Translation2d(1 s, 2 m).X()",
                                 "Translation2d(nonfiniteLength, 2 m).X()",
                                 "nonfinitePose.X()",
                                 "Rotation2d(1 s).Radians()",
                                 "Transform2d(1 m, 2 m, 3 s).X()",
                                 "Twist2d(1 m, 2 m, 3 s).dtheta()"}) {
    bool rejected = false;
    try {
      context.Evaluate(expression);
    } catch (const std::exception&) {
      rejected = true;
    }
    if (!rejected) {
      std::cerr << "typed geometry accepted an incompatible or non-finite quantity\n";
      return 1;
    }
  }
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
        1e-10 ||
      std::abs(context.Evaluate("translation.Norm()").In("m") - 5.0) > 1e-10 ||
      std::abs(context.Evaluate("translation.Y()").In("m") - 4.0) > 1e-10) {
    std::cerr << "Translation2d::Distance returned the wrong value\n";
    return 1;
  }

  if (std::abs(context.Evaluate("translation.SquaredNorm()").In("m^2") - 25.0) > 1e-10 ||
      std::abs(context.Evaluate("translation.SquaredDistance(other)").In("m^2") - 25.0) > 1e-10 ||
      std::abs(context.Evaluate("translation.Dot(Translation2d(2 m, 1 m))").In("m^2") - 10.0) > 1e-10 ||
      std::abs(context.Evaluate("translation.Cross(Translation2d(2 m, 1 m))").In("m^2") + 5.0) > 1e-10 ||
      std::abs(context.Evaluate("Translation2d(2 m, Rotation2d(90 deg)).Y()").In("m") - 2.0) > 1e-10 ||
      std::abs(context.Evaluate("Translation2d(0 m, 1 m).Angle().Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(context.Evaluate("Translation2d(2 m, 0 m).RotateAround(Translation2d(1 m, 0 m), rotation).Y()").In("m") - 1.0) > 1e-10 ||
      std::abs(context.Evaluate("Pose2d(2 m, 0 m, 0 deg).RotateAround(Translation2d(1 m, 0 m), rotation).Y()").In("m") - 1.0) > 1e-10) {
    std::cerr << "translation or pose operation returned the wrong value\n";
    return 1;
  }
  bool undefinedAngleRejected = false;
  try {
    context.EvaluateRotation2d("Translation2d().Angle()");
  } catch (const std::exception&) {
    undefinedAngleRejected = true;
  }
  if (!undefinedAngleRejected) {
    std::cerr << "undefined translation angle was accepted\n";
    return 1;
  }
  context.SetVariable("factor", Quantity{2.0, {}});
  context.SetVariable("nonfiniteFactor", Quantity{std::numeric_limits<double>::infinity(), {}});
  if (std::abs(context.Evaluate("(translation + Translation2d(1 m, 2 m)).X()").In("m") - 4.0) > 1e-10 ||
      std::abs(context.Evaluate("(translation - Translation2d(1 m, 2 m)).Y()").In("m") - 2.0) > 1e-10 ||
      std::abs(context.Evaluate("(-translation).X()").In("m") + 3.0) > 1e-10 ||
      std::abs(context.Evaluate("(translation * factor).X()").In("m") - 6.0) > 1e-10 ||
      std::abs(context.Evaluate("(translation * 2).X()").In("m") - 6.0) > 1e-10 ||
      std::abs(context.Evaluate("(translation / 2.0).Y()").In("m") - 2.0) > 1e-10 ||
      std::abs(context.Evaluate("(translation / factor).Y()").In("m") - 2.0) > 1e-10 ||
      std::abs(context.Evaluate("(Rotation2d(60 deg) - Rotation2d(30 deg)).Degrees()").In("deg") - 30.0) > 1e-10 ||
      std::abs(context.Evaluate("(Rotation2d(60 deg) + Rotation2d(30 deg)).Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(context.Evaluate("(Rotation2d(60 deg) * factor).Degrees()").In("deg") - 120.0) > 1e-10 ||
      std::abs(context.Evaluate("(rotation / 2.0).Degrees()").In("deg") - 45.0) > 1e-10 ||
      std::abs(context.Evaluate("(-rotation).Degrees()").In("deg") + 90.0) > 1e-10 ||
      std::abs(context.Evaluate("(Pose2d() + Transform2d(2 m, 0 m, 0 deg)).X()").In("m") - 2.0) > 1e-10 ||
      std::abs(context.Evaluate("(pose - origin).X()").In("m") - 3.0) > 1e-10 ||
      std::abs(context.Evaluate("(pose * 2.0).X()").In("m") - 6.0) > 1e-10 ||
      std::abs(context.Evaluate("(pose / factor).X()").In("m") - 1.5) > 1e-10 ||
      std::abs(context.Evaluate("(Transform2d(2 m, 0 m, 0 deg) * 2.0).X()").In("m") - 4.0) > 1e-10 ||
      std::abs(context.Evaluate("(Transform2d(2 m, 0 m, 0 deg) / factor).X()").In("m") - 1.0) > 1e-10 ||
      std::abs(context.Evaluate("(Transform2d(2 m, 0 m, 0 deg) + Transform2d(1 m, 0 m, 0 deg)).X()").In("m") - 3.0) > 1e-10 ||
      std::abs(context.Evaluate("(Twist2d(2 m, 0 m, 0 deg) * factor).dx()").In("m") - 4.0) > 1e-10 ||
      !context.EvaluateBoolean("Translation2d(1 m, 2 m) == Translation2d(1 m, 2 m)") ||
      !context.EvaluateBoolean("Rotation2d(0 deg) != Rotation2d(90 deg)") ||
      !context.EvaluateBoolean("Pose2d() == Pose2d()") ||
      !context.EvaluateBoolean("Transform2d() == Transform2d()") ||
      !context.EvaluateBoolean("Twist2d(0 m, 0 m, 0 deg) == Twist2d(0 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Pose2d() != Pose2d(1 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Transform2d() != Transform2d(1 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Twist2d(0 m, 0 m, 0 deg) != Twist2d(1 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Twist2d() == Twist2d(0 m, 0 m, 0 deg)") ||
      std::abs(context.Evaluate("Pose2d(1 m, 2 m, rotation).Rotation().Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(context.Evaluate("Transform2d(1 m, 2 m, rotation).Rotation().Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(context.Evaluate("translation.Nearest([Translation2d(9 m, 9 m), Translation2d(4 m, 4 m)]).X()").In("m") - 4.0) > 1e-10 ||
      std::abs(context.Evaluate("origin.Nearest([Pose2d(5 m, 0 m, 0 deg), Pose2d(1 m, 0 m, 0 deg)]).X()").In("m") - 1.0) > 1e-10 ||
      std::abs(context.Evaluate("origin.Nearest([Pose2d(1 m, 0 m, 90 deg), Pose2d(1 m, 0 m, 0 deg)]).Rotation().Degrees()").In("deg")) > 1e-10) {
    std::cerr << "geometry arithmetic or equality returned the wrong value\n";
    return 1;
  }
  for (const char* expression : {"(translation * 1 m).X()", "(translation / 0.0).X()",
                                 "(translation / nonfiniteLength).X()",
                                 "(translation * nonfiniteFactor).X()",
                                 "translation.Nearest([]).X()",
                                 "origin.Nearest([Translation2d()]).X()",
                                 "translation.Nearest([eval(\"1\")]).X()"}) {
    bool rejected = false;
    try {
      context.Evaluate(expression);
    } catch (const std::exception&) {
      rejected = true;
    }
    if (!rejected) {
      std::cerr << "invalid geometry scalar was accepted\n";
      return 1;
    }
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

  if (!context.EvaluateBoolean("Rectangle2d(Pose2d(), 4 m, 2 m).Contains(Translation2d(1 m, 0 m))") ||
      !context.EvaluateBoolean("Ellipse2d(Pose2d(), 2 m, 1 m).Intersects(Translation2d(2 m, 0 m))") ||
      std::abs(context.Evaluate("Rectangle2d(Pose2d(), 4 m, 2 m).XWidth()").In("m") - 4.0) > 1e-10 ||
      std::abs(context.Evaluate("Ellipse2d(Pose2d(), 2 m, 1 m).Distance(Translation2d(3 m, 0 m))").In("m") - 1.0) > 1e-8) {
    std::cerr << "rectangle or ellipse binding returned the wrong value\n";
    return 1;
  }

  EvaluationContext shapes;
  shapes.SetSetupScript("var rect = Rectangle2d(Pose2d(), 4 m, 2 m); "
                        "var ellipse = Ellipse2d(Pose2d(), 2 m, 1 m); "
                        "var focus = ellipse.FocalPoints()[0];");
  auto rectangle = shapes.EvaluateRectangle2d("rect");
  auto ellipse = shapes.EvaluateEllipse2d("ellipse");
  shapes.SetVariable("otherRect", rectangle);
  shapes.SetVariable("otherEllipse", ellipse);
  if (!shapes.EvaluateBoolean("rect == otherRect") ||
      !shapes.EvaluateBoolean("ellipse == otherEllipse") ||
      !shapes.EvaluateBoolean("rect != Rectangle2d(Pose2d(), 2 m, 2 m)") ||
      !shapes.EvaluateBoolean("ellipse != Ellipse2d(Pose2d(), 1 m, 1 m)") ||
      !shapes.EvaluateBoolean("rect.Intersects(Translation2d(2 m, 0 m))") ||
      !shapes.EvaluateBoolean("ellipse.Contains(Translation2d(1 m, 0 m))") ||
      std::abs(shapes.Evaluate("rect.YWidth()").In("m") - 2.0) > 1e-10 ||
      std::abs(shapes.Evaluate("ellipse.XSemiAxis()").In("m") - 2.0) > 1e-10 ||
      std::abs(shapes.Evaluate("ellipse.YSemiAxis()").In("m") - 1.0) > 1e-10 ||
      std::abs(shapes.Evaluate("focus.X()").In("m") + std::sqrt(3.0)) > 1e-10 ||
      std::abs(shapes.Evaluate("ellipse.FocalPoints()[1].X()").In("m") - std::sqrt(3.0)) > 1e-10 ||
      std::abs(shapes.Evaluate("rect.Nearest(Translation2d(3 m, 0 m)).X()").In("m") - 2.0) > 1e-10 ||
      std::abs(shapes.Evaluate("ellipse.Nearest(Translation2d(3 m, 0 m)).X()").In("m") - 2.0) > 1e-8 ||
      std::abs(shapes.Evaluate("rect.Distance(Translation2d(3 m, 0 m))").In("m") - 1.0) > 1e-10 ||
      std::abs(shapes.Evaluate("rect.TransformBy(Transform2d(1 m, 0 m, 0 deg)).Center().X()").In("m") - 1.0) > 1e-10 ||
      std::abs(shapes.Evaluate("ellipse.TransformBy(Transform2d(1 m, 0 m, 0 deg)).Center().X()").In("m") - 1.0) > 1e-10 ||
      std::abs(shapes.Evaluate("rect.RotateBy(Rotation2d(90 deg)).Rotation().Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(shapes.Evaluate("ellipse.RotateBy(Rotation2d(90 deg)).Rotation().Degrees()").In("deg") - 90.0) > 1e-10 ||
      std::abs(shapes.Evaluate("Rectangle2d(Translation2d(0 m, 0 m), Translation2d(4 m, 2 m)).Center().X()").In("m") - 2.0) > 1e-10 ||
      std::abs(shapes.Evaluate("Ellipse2d(Translation2d(0 m, 0 m), 2.0).YSemiAxis()").In("m") - 2.0) > 1e-10) {
    std::cerr << "shape method, constructor, or setup binding returned the wrong value\n";
    return 1;
  }
  for (const char* expression : {"rect.FocalPoints()[0].X()",
                                 "ellipse.FocalPoints()[2].X()",
                                 "ellipse.FocalPoints()[-1].X()",
                                 "ellipse.FocalPoints()[eval(\"0\")].X()",
                                 "Rectangle2d(Pose2d(), 4 s, 2 m).XWidth()",
                                 "Ellipse2d(Pose2d(), 0 m, 1 m).XSemiAxis()"}) {
    bool rejected = false;
    try {
      shapes.Evaluate(expression);
    } catch (const std::exception&) {
      rejected = true;
    }
    if (!rejected) {
      std::cerr << "invalid shape expression was accepted\n";
      return 1;
    }
  }

  return 0;
}
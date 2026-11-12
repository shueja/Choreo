#include "wpilib-expression/expression.hpp"

#include <atomic>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include <wpi/units/angle.hpp>
#include <wpi/units/length.hpp>

using wpilib::expression::EvaluationContext;
using wpilib::expression::From;
using wpilib::expression::In;
using wpilib::expression::TryAs;
using wpi::math::Pose2d;

namespace {

bool EvaluatesTo(const EvaluationContext& context, std::string_view expression,
                 double expected, std::string_view unit, double tolerance = 1e-10) {
  auto dimension = unit.empty() ? units::precise::one : From(1.0, unit).units();
  auto result = context.Evaluate(expression, dimension);
  auto value = unit.empty() ? result.value() * result.units().multiplier() : In(result, unit);
  if (std::abs(value - expected) <= tolerance) {
    return true;
  }
  std::cerr << "Expression '" << expression << "' returned " << value << " " << unit
            << "; expected " << expected << " within " << tolerance << '\n';
  return false;
}

int TestNamespaces() {
  EvaluationContext setup;
  setup.SetSetupScript("namespace(\"field\"); field.origin = Pose2d(1 m, 0 m, 0 deg); "
                       "field.distance = 2 m; namespace field::deep::more { var length = 2 ft; } "
                       "namespace field::deep { var goal = Pose2d(field.deep.more.length, 0 m, 0 deg); "
                       "var X = field.deep.more.length * 2; "
                       "var region = Rectangle2d(Pose2d(), 4 m, 2 m); } "
                       "namespace(\"field::deep::legacy\"); field.deep.legacy.m = 3 m; "
                       "var result = field.origin.X();");
  setup.SetVariable("target", Pose2d{wpi::units::meter_t{4.0}, wpi::units::meter_t{4.0},
                                     wpi::math::Rotation2d{}});
  if (!EvaluatesTo(setup, "field.origin.X() + field.distance", 3.0, "m") ||
      !EvaluatesTo(setup, "result", 1.0, "m") ||
      !EvaluatesTo(setup, "field.deep.goal.X()", 2.0, "ft") ||
      !EvaluatesTo(setup, "field.deep.more.length", 2.0, "ft") ||
      !EvaluatesTo(setup, "field::deep::more::length", 2.0, "ft") ||
      !EvaluatesTo(setup, "field.deep.legacy.m", 3.0, "m") ||
      !EvaluatesTo(setup, "field.deep.X", 4.0, "ft") ||
      std::abs(setup.EvaluatePose2d("field.origin").X().value() - 1.0) > 1e-10 ||
      !setup.EvaluateBoolean("field.deep.region.Contains(Translation2d(1 m, 0 m))")) {
    std::cerr << "namespace values returned the wrong result\n";
    return 1;
  }
    if (!EvaluatesTo(setup, "field.origin.Translation().Norm()", 1.0, "m") ||
      !EvaluatesTo(setup, "field.origin.Translation().SquaredNorm()", 1.0, "m^2") ||
      !EvaluatesTo(setup, "field.origin.Distance(Pose2d(4 m, 4 m, 90 deg))", 5.0, "m") ||
      !EvaluatesTo(setup, "field.origin.Distance(target)", 5.0, "m") ||
      !EvaluatesTo(setup, "field.origin.Distance(Pose2d(1 m, 0 m, 180 deg))", 0.0, "m")) {
    std::cerr << "pose distance or norm returned the wrong value\n";
    return 1;
  }
  for (const char* expression : {"field.origin = Pose2d()", "field.deep.more.length = 5 m",
                                 "field.deep.more.missing", "field.deep.more = 3 m"}) {
    bool rejected = false;
    try {
      setup.Evaluate(expression);
    } catch (const std::exception&) {
      rejected = true;
    }
    if (!rejected || !EvaluatesTo(setup, "field.deep.more.length", 2.0, "ft")) {
      std::cerr << "namespace mutation or missing member was accepted\n";
      return 1;
    }
  }
  for (const char* script : {"namespace(\"field\"); field.origin = eval(\"1\");",
                             "namespace(\"field\"); field.origin = 1 m; namespace field::origin { var x = 1 m; }",
                             "namespace field { def run() { return 1; } }",
                             "namespace field { var x = 1 m; var y = x + 1 m; }",
                             "namespace field { var x = 1 m; var y = field.missing; }",
                             "namespace(\"cyclic\"); cyclic.self = cyclic;",
                             "namespace(\"field\"); field.origin = 1 m; field.origin.X = 2 m;"}) {
    bool rejected = false;
    try {
      setup.SetSetupScript(script);
    } catch (const std::exception&) {
      rejected = true;
    }
    if (!rejected || !EvaluatesTo(setup, "field.origin.X()", 1.0, "m")) {
      std::cerr << "invalid namespace setup was accepted or changed the context\n";
      return 1;
    }
  }
  auto copy = setup;
  copy.SetSetupScript("namespace field::deep::third::fourth::fifth::sixth { var replacement = 5 m; }");
  if (!EvaluatesTo(copy, "field.deep.third.fourth.fifth.sixth.replacement", 5.0, "m") ||
      !EvaluatesTo(setup, "field.deep.more.length", 2.0, "ft")) {
    std::cerr << "namespace snapshots leaked between copied contexts\n";
    return 1;
  }
  return 0;
}

int TestSetup() {
  EvaluationContext setup;
  setup.SetVariable("old", From(1.0, units::unit_from_string("m")));
  setup.SetSetupScript("// 2 unknown_units\nvar base = 2 m; /* 3 unknown_units */ "
               "var offset = 3 ft; var total = base + offset; "
                       "var pose = Pose2d(3 m, 4 m, 90 deg); var gain = 2; "
                       "var heading = pose.Rotation().Degrees(); var targetX = pose.X(); "
                       "global shared = 4 m; const var extra = 1 m; auto combined = total + extra;");
  if (!EvaluatesTo(setup, "base + offset", 2.9144, "m") ||
      !EvaluatesTo(setup, "total", 2.9144, "m") ||
      !EvaluatesTo(setup, "pose.X()", 3.0, "m") ||
      !EvaluatesTo(setup, "pose.Rotation().Degrees()", 90.0, "deg") ||
      !EvaluatesTo(setup, "pose.Rotation().Degrees() / deg", 90.0, "") ||
      !EvaluatesTo(setup, "heading", 90.0, "deg") ||
      !EvaluatesTo(setup, "targetX", 3.0, "m") ||
      !EvaluatesTo(setup, "gain", 2.0, "") ||
      !EvaluatesTo(setup, "shared", 4.0, "m") ||
      !EvaluatesTo(setup, "combined", 3.9144, "m")) {
    std::cerr << "setup script returned incorrect variables\n";
    return 1;
  }
  bool setupFailureRejected = false;
  try {
    setup.SetSetupScript("var invalid = 1 m + 2 s;");
  } catch (const std::exception&) {
    setupFailureRejected = true;
  }
  if (!setupFailureRejected || !EvaluatesTo(setup, "base", 2.0, "m")) {
    std::cerr << "failed setup changed the context\n";
    return 1;
  }
  bool unsupportedSetupRejected = false;
  try {
    setup.SetSetupScript("var bad = [1, 2];");
  } catch (const std::exception&) {
    unsupportedSetupRejected = true;
  }
  if (!unsupportedSetupRejected || !EvaluatesTo(setup, "base", 2.0, "m")) {
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
    if (!rejected || !EvaluatesTo(setup, "base", 2.0, "m")) {
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
  if (!reservedSetupRejected || !EvaluatesTo(setup, "base", 2.0, "m")) {
    std::cerr << "reserved setup name was accepted or changed the context\n";
    return 1;
  }
  for (const char* name : {"Cos", "Degrees"}) {
    bool methodNameRejected = false;
    try {
      setup.SetVariable(name, From(1.0, units::unit_from_string("m")));
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
  if (!oldSetupRejected || !EvaluatesTo(setup, "replacement", 5.0, "m")) {
    std::cerr << "setup did not replace previous variables\n";
    return 1;
  }
  return 0;
}

int TestArithmeticAndVariables() {
  EvaluationContext context;
  auto length = From(1.0, units::unit_from_string("m")).units();
  auto mixedLength = context.Evaluate("2 * m + 3 * ft", length);
  if (std::abs(In(mixedLength, units::unit_from_string("m")) - 2.9144) > 1e-10) {
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

  context.SetVariable("offset", From(3.0, units::unit_from_string("ft")));
  auto withVariable = context.Evaluate("2 * m + offset", length);
  if (std::abs(In(withVariable, units::unit_from_string("m")) - 2.9144) > 1e-10) {
    std::cerr << "registered variable returned the wrong value\n";
    return 1;
  }
  context.SetVariable("offset", From(1.0, units::unit_from_string("m")));
  if (!EvaluatesTo(context, "offset", 1.0, "m")) {
    std::cerr << "updated variable returned the wrong value\n";
    return 1;
  }
  return 0;
}

int TestReadOnlyExpressions() {
  EvaluationContext context;
  context.SetVariable("offset", From(1.0, units::unit_from_string("m")));
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
    if (!rejected || !EvaluatesTo(context, "offset", 1.0, "m")) {
      std::cerr << "state-changing evaluation was accepted or changed the context\n";
      return 1;
    }
  }
  if (!context.EvaluateBoolean("1 m == 100 cm") ||
      !context.EvaluateBoolean("1 m < 2 m && 2 m > 1 m")) {
    std::cerr << "read-only comparison was rejected\n";
    return 1;
  }
  return 0;
}

int TestErrorMessages() {
  EvaluationContext context;
  for (const char* expression : {"2 * (", "Pose2d().Norm()", "Pose2d().SquaredNorm()",
                                 "Translation2d(1 m, 2 m).Distance(Pose2d())"}) {
    try {
      context.Evaluate(expression);
      std::cerr << "invalid expression was accepted\n";
      return 1;
    } catch (const std::invalid_argument& error) {
      std::string_view message = error.what();
      if (!message.starts_with("Syntax error:") || message.find("function dispatch") != message.npos) {
        std::cerr << "invalid expression exposed an internal error: " << message << '\n';
        return 1;
      }
    }
  }
  try {
    context.Evaluate("Translation2d(1 m, 2 m).Distance(Pose2d())");
    std::cerr << "invalid method arguments were accepted\n";
    return 1;
  } catch (const std::invalid_argument& error) {
    if (std::string_view(error.what()).find("invalid arguments for 'Distance'") ==
        std::string_view::npos) {
      std::cerr << "method error did not identify the invalid call: " << error.what() << '\n';
      return 1;
    }
  }
  for (const char* script : {"var value = (", "var value = Translation2d().Distance(Pose2d());"}) {
    try {
      context.SetSetupScript(script);
      std::cerr << "invalid setup script was accepted\n";
      return 1;
    } catch (const std::invalid_argument& error) {
      if (!std::string_view(error.what()).starts_with("Syntax error:")) {
        std::cerr << "invalid setup did not report a syntax error: " << error.what() << '\n';
        return 1;
      }
    }
  }
  return 0;
}

int TestContextCopies() {
  EvaluationContext first;
  EvaluationContext second = first;
  first.SetVariable("distance", From(1.0, units::unit_from_string("m")));
  second.SetVariable("distance", From(2.0, units::unit_from_string("m")));
  std::atomic<bool> concurrentFailed{false};
  auto evaluateRepeatedly = [&](const EvaluationContext& current, double expected) {
    try {
      for (int iteration = 0; iteration < 5; ++iteration) {
        if (!EvaluatesTo(current, "distance", expected, "m")) {
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
  if (!EvaluatesTo(moved, "distance", 1.0, "m") ||
      !EvaluatesTo(first, "1 m", 1.0, "m")) {
    std::cerr << "moved context or moved-from context returned the wrong value\n";
    return 1;
  }
  return 0;
}

int TestUnitExpressions() {
  EvaluationContext context;
  if (!EvaluatesTo(context, "2 m", 2.0, "m")) {
    std::cerr << "bare length suffix returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "90 deg", std::acos(-1.0) / 2.0, "rad")) {
    std::cerr << "bare angle suffix returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "6000 RPM", 200.0 * std::acos(-1.0), "rad/s", 1e-9)) {
    std::cerr << "rotational speed conversion returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "1 in", 0.0254, "m") ||
      !EvaluatesTo(context, "100 cm", 1.0, "m")) {
    std::cerr << "alternate length units returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "1 min", 60.0, "s") ||
      !EvaluatesTo(context, "1 ms", 1e-3, "s")) {
    std::cerr << "alternate time units returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "1 lb", 0.45359237, "kg") ||
      !EvaluatesTo(context, "500 mA", 0.5, "A")) {
    std::cerr << "alternate mass or current units returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "1 turn", 2.0 * std::acos(-1.0), "rad") ||
      !EvaluatesTo(context, "2 kV", 2000.0, "V")) {
    std::cerr << "alternate angle or voltage units returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "1 kW", 1000.0, "W") ||
      !EvaluatesTo(context, "1 kPa", 1000.0, "Pa")) {
    std::cerr << "derived power or pressure units returned the wrong value\n";
    return 1;
  }
  auto feet = TryAs<wpi::units::foot_t>(From(1.0, units::unit_from_string("m")));
  if (!feet || std::abs(feet->value() - (1.0 / 0.3048)) > 1e-10) {
    std::cerr << "WPILib foot conversion returned the wrong value\n";
    return 1;
  }
  auto radians = TryAs<wpi::units::radian_t>(From(90.0, units::unit_from_string("deg")));
  if (!radians || std::abs(radians->value() - std::acos(-1.0) / 2.0) > 1e-10) {
    std::cerr << "WPILib radian conversion returned the wrong value\n";
    return 1;
  }
  if (TryAs<wpi::units::meter_t>(From(1.0, units::unit_from_string("s")))) {
    std::cerr << "incompatible WPILib unit conversion succeeded\n";
    return 1;
  }
  wpi::units::meter_t output;
  if (!TryAs(From(2.0, units::unit_from_string("m")), output) || std::abs(output.value() - 2.0) > 1e-10) {
    std::cerr << "WPILib output-parameter conversion returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "2 m/s^2", 2.0, "m/s^2")) {
    std::cerr << "compound acceleration unit returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "0.018 N*m/A", 0.018, "N*m/A")) {
    std::cerr << "compound motor constant returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "(2e0 m + 3 ft)", 2.9144, "m")) {
    std::cerr << "scientific notation or grouping returned the wrong value\n";
    return 1;
  }
  return 0;
}

int TestIdentifiers() {
  EvaluationContext context;
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
    context.SetVariable("m", From(1.0, units::unit_from_string("m")));
  } catch (const std::invalid_argument&) {
    unitNameCollisionRejected = true;
  }
  if (!unitNameCollisionRejected) {
    std::cerr << "unit name was accepted as a variable\n";
    return 1;
  }
  bool constructorNameCollisionRejected = false;
  try {
    context.SetVariable("Pose2d", From(1.0, units::unit_from_string("m")));
  } catch (const std::invalid_argument&) {
    constructorNameCollisionRejected = true;
  }
  if (!constructorNameCollisionRejected) {
    std::cerr << "geometry constructor name was accepted as a variable\n";
    return 1;
  }
  return 0;
}

int TestMeasurementMath() {
  EvaluationContext context;
  auto area = context.Evaluate("(2 m)^2");
  if (std::abs(In(area, units::unit_from_string("m^2")) - 4.0) > 1e-10) {
    std::cerr << "integer quantity power returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "((2 m)^2) * 3 m", 12.0, "m^3") ||
      !EvaluatesTo(context, "3 m * ((2 m)^2)", 12.0, "m^3") ||
      !EvaluatesTo(context, "10000000000 * m", 1e10, "m", 1e-3)) {
    std::cerr << "compound quantity arithmetic returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "sin(90 deg)", 1.0, "")) {
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
  return 0;
}

int TestGeometryConstructors() {
  EvaluationContext context;
  auto length = From(1.0, units::unit_from_string("m")).units();
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
  if (!EvaluatesTo(context, "Rotation2d(90 deg).Radians()", 90.0, "deg") ||
      !EvaluatesTo(context, "Transform2d(3 ft, 4 m, 90 deg).X()", 0.9144, "m") ||
      !EvaluatesTo(context, "Transform2d(3 ft, 4 m, 90 deg).Y()", 4.0, "m") ||
      !EvaluatesTo(context, "Twist2d(3 m, 4 m, 90 deg).dx()", 3.0, "m") ||
      !EvaluatesTo(context, "Twist2d(3 m, 4 m, 90 deg).dy()", 4.0, "m") ||
      !EvaluatesTo(context, "Twist2d(3 m, 4 m, 90 deg).dtheta()", 90.0, "deg") ||
      !EvaluatesTo(context, "pose.X() + pose.Y()", 7.0, "m") ||
      !context.EvaluateBoolean("pose.X() == 3 m")) {
    std::cerr << "typed geometry conversion returned the wrong value\n";
    return 1;
  }
  if (!EvaluatesTo(context, "Pose2d().X()", 0.0, "m") ||
      !EvaluatesTo(context, "Translation2d().Y()", 0.0, "m") ||
      !EvaluatesTo(context, "Rotation2d().Radians()", 0.0, "rad") ||
      !EvaluatesTo(context, "Transform2d().X()", 0.0, "m") ||
      !EvaluatesTo(context, "Rotation2d(0.0, 1.0).Degrees()", 90.0, "deg") ||
      !EvaluatesTo(context, "Pose2d(Translation2d(3 m, 4 m), Rotation2d(90 deg)).X()", 3.0, "m") ||
      !EvaluatesTo(context, "Transform2d(Translation2d(3 m, 4 m), Rotation2d(90 deg)).Y()", 4.0, "m") ||
      !EvaluatesTo(context, "Transform2d(Pose2d(), Pose2d(3 m, 4 m, 90 deg)).X()", 3.0, "m")) {
    std::cerr << "geometry constructor overload returned the wrong value\n";
    return 1;
  }
  context.SetVariable("nonfiniteLength",
                      units::precise_measurement{std::numeric_limits<double>::infinity(), units::precise::m});
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
  return 0;
}

int TestGeometryOperations() {
  EvaluationContext context;
  auto length = From(1.0, units::unit_from_string("m")).units();
  auto pose = context.EvaluatePose2d("Pose2d(3 m, 4 m, 90 deg)");
  context.SetVariable("pose", pose);
  context.SetVariable("nonfiniteLength",
                      units::precise_measurement{std::numeric_limits<double>::infinity(), units::precise::m});
  auto origin = context.EvaluatePose2d("Pose2d(0 m, 0 m, 0 deg)");
  context.SetVariable("origin", origin);
  auto rotation = context.EvaluateRotation2d("Rotation2d(90 deg)");
  context.SetVariable("rotation", rotation);
  if (!EvaluatesTo(context, "pose.X()", 3.0, "m")) {
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
  if (!EvaluatesTo(context, "translation.Distance(other)", 5.0, "m") ||
      !EvaluatesTo(context, "translation.Norm()", 5.0, "m") ||
      !EvaluatesTo(context, "translation.Y()", 4.0, "m")) {
    std::cerr << "Translation2d::Distance returned the wrong value\n";
    return 1;
  }

  if (!EvaluatesTo(context, "translation.SquaredNorm()", 25.0, "m^2") ||
      !EvaluatesTo(context, "translation.SquaredDistance(other)", 25.0, "m^2") ||
      !EvaluatesTo(context, "translation.Dot(Translation2d(2 m, 1 m))", 10.0, "m^2") ||
      !EvaluatesTo(context, "translation.Cross(Translation2d(2 m, 1 m))", -(5.0), "m^2") ||
      !EvaluatesTo(context, "Translation2d(2 m, Rotation2d(90 deg)).Y()", 2.0, "m") ||
      !EvaluatesTo(context, "Translation2d(0 m, 1 m).Angle().Degrees()", 90.0, "deg") ||
      !EvaluatesTo(context, "Translation2d(2 m, 0 m).RotateAround(Translation2d(1 m, 0 m), rotation).Y()", 1.0, "m") ||
      !EvaluatesTo(context, "Pose2d(2 m, 0 m, 0 deg).RotateAround(Translation2d(1 m, 0 m), rotation).Y()", 1.0, "m")) {
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
  context.SetVariable("factor", units::precise_measurement{2.0, units::precise::one});
  context.SetVariable("nonfiniteFactor", units::precise_measurement{std::numeric_limits<double>::infinity(), units::precise::one});
  if (!EvaluatesTo(context, "(translation + Translation2d(1 m, 2 m)).X()", 4.0, "m") ||
      !EvaluatesTo(context, "(translation - Translation2d(1 m, 2 m)).Y()", 2.0, "m") ||
      !EvaluatesTo(context, "(-translation).X()", -(3.0), "m") ||
      !EvaluatesTo(context, "(translation * factor).X()", 6.0, "m") ||
      !EvaluatesTo(context, "(translation * 2).X()", 6.0, "m") ||
      !EvaluatesTo(context, "(translation / 2.0).Y()", 2.0, "m") ||
      !EvaluatesTo(context, "(translation / factor).Y()", 2.0, "m") ||
      !EvaluatesTo(context, "(Rotation2d(60 deg) - Rotation2d(30 deg)).Degrees()", 30.0, "deg") ||
      !EvaluatesTo(context, "(Rotation2d(60 deg) + Rotation2d(30 deg)).Degrees()", 90.0, "deg") ||
      !EvaluatesTo(context, "(Rotation2d(60 deg) * factor).Degrees()", 120.0, "deg") ||
      !EvaluatesTo(context, "(rotation / 2.0).Degrees()", 45.0, "deg") ||
      !EvaluatesTo(context, "(-rotation).Degrees()", -(90.0), "deg") ||
      !EvaluatesTo(context, "(Pose2d() + Transform2d(2 m, 0 m, 0 deg)).X()", 2.0, "m") ||
      !EvaluatesTo(context, "(pose - origin).X()", 3.0, "m") ||
      !EvaluatesTo(context, "(pose * 2.0).X()", 6.0, "m") ||
      !EvaluatesTo(context, "(pose / factor).X()", 1.5, "m") ||
      !EvaluatesTo(context, "(Transform2d(2 m, 0 m, 0 deg) * 2.0).X()", 4.0, "m") ||
      !EvaluatesTo(context, "(Transform2d(2 m, 0 m, 0 deg) / factor).X()", 1.0, "m") ||
      !EvaluatesTo(context, "(Transform2d(2 m, 0 m, 0 deg) + Transform2d(1 m, 0 m, 0 deg)).X()", 3.0, "m") ||
      !EvaluatesTo(context, "(Twist2d(2 m, 0 m, 0 deg) * factor).dx()", 4.0, "m") ||
      !context.EvaluateBoolean("Translation2d(1 m, 2 m) == Translation2d(1 m, 2 m)") ||
      !context.EvaluateBoolean("Rotation2d(0 deg) != Rotation2d(90 deg)") ||
      !context.EvaluateBoolean("Pose2d() == Pose2d()") ||
      !context.EvaluateBoolean("Transform2d() == Transform2d()") ||
      !context.EvaluateBoolean("Twist2d(0 m, 0 m, 0 deg) == Twist2d(0 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Pose2d() != Pose2d(1 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Transform2d() != Transform2d(1 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Twist2d(0 m, 0 m, 0 deg) != Twist2d(1 m, 0 m, 0 deg)") ||
      !context.EvaluateBoolean("Twist2d() == Twist2d(0 m, 0 m, 0 deg)") ||
      !EvaluatesTo(context, "Pose2d(1 m, 2 m, rotation).Rotation().Degrees()", 90.0, "deg") ||
      !EvaluatesTo(context, "Transform2d(1 m, 2 m, rotation).Rotation().Degrees()", 90.0, "deg") ||
      !EvaluatesTo(context, "translation.Nearest([Translation2d(9 m, 9 m), Translation2d(4 m, 4 m)]).X()", 4.0, "m") ||
      !EvaluatesTo(context, "origin.Nearest([Pose2d(5 m, 0 m, 0 deg), Pose2d(1 m, 0 m, 0 deg)]).X()", 1.0, "m") ||
      !EvaluatesTo(context, "origin.Nearest([Pose2d(1 m, 0 m, 90 deg), Pose2d(1 m, 0 m, 0 deg)]).Rotation().Degrees()", 0.0, "deg")) {
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
  return 0;
}

int TestShapes() {
  EvaluationContext context;
  if (!context.EvaluateBoolean("Rectangle2d(Pose2d(), 4 m, 2 m).Contains(Translation2d(1 m, 0 m))") ||
      !context.EvaluateBoolean("Ellipse2d(Pose2d(), 2 m, 1 m).Intersects(Translation2d(2 m, 0 m))") ||
      !EvaluatesTo(context, "Rectangle2d(Pose2d(), 4 m, 2 m).XWidth()", 4.0, "m") ||
      !EvaluatesTo(context, "Ellipse2d(Pose2d(), 2 m, 1 m).Distance(Translation2d(3 m, 0 m))", 1.0, "m", 1e-8)) {
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
      !EvaluatesTo(shapes, "rect.YWidth()", 2.0, "m") ||
      !EvaluatesTo(shapes, "ellipse.XSemiAxis()", 2.0, "m") ||
      !EvaluatesTo(shapes, "ellipse.YSemiAxis()", 1.0, "m") ||
      !EvaluatesTo(shapes, "focus.X()", -(std::sqrt(3.0)), "m") ||
      !EvaluatesTo(shapes, "ellipse.FocalPoints()[1].X()", std::sqrt(3.0), "m") ||
      !EvaluatesTo(shapes, "rect.Nearest(Translation2d(3 m, 0 m)).X()", 2.0, "m") ||
      !EvaluatesTo(shapes, "ellipse.Nearest(Translation2d(3 m, 0 m)).X()", 2.0, "m", 1e-8) ||
      !EvaluatesTo(shapes, "rect.Distance(Translation2d(3 m, 0 m))", 1.0, "m") ||
      !EvaluatesTo(shapes, "rect.TransformBy(Transform2d(1 m, 0 m, 0 deg)).Center().X()", 1.0, "m") ||
      !EvaluatesTo(shapes, "ellipse.TransformBy(Transform2d(1 m, 0 m, 0 deg)).Center().X()", 1.0, "m") ||
      !EvaluatesTo(shapes, "rect.RotateBy(Rotation2d(90 deg)).Rotation().Degrees()", 90.0, "deg") ||
      !EvaluatesTo(shapes, "ellipse.RotateBy(Rotation2d(90 deg)).Rotation().Degrees()", 90.0, "deg") ||
      !EvaluatesTo(shapes, "Rectangle2d(Translation2d(0 m, 0 m), Translation2d(4 m, 2 m)).Center().X()", 2.0, "m") ||
      !EvaluatesTo(shapes, "Ellipse2d(Translation2d(0 m, 0 m), 2.0).YSemiAxis()", 2.0, "m")) {
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

struct TestCase {
  std::string_view name;
  int (*run)();
};

constexpr TestCase tests[]{
    {"namespaces", TestNamespaces},
    {"setup", TestSetup},
    {"arithmetic_and_variables", TestArithmeticAndVariables},
    {"read_only_expressions", TestReadOnlyExpressions},
    {"error_messages", TestErrorMessages},
    {"context_copies", TestContextCopies},
    {"unit_expressions", TestUnitExpressions},
    {"identifiers", TestIdentifiers},
    {"measurement_math", TestMeasurementMath},
    {"geometry_constructors", TestGeometryConstructors},
    {"geometry_operations", TestGeometryOperations},
    {"shapes", TestShapes},
};

}  // namespace

int main(int argc, char** argv) {
  if (argc > 2) {
    std::cerr << "Usage: wpilib_expression_test [case]\n";
    return 2;
  }
  bool found = false;
  for (const auto& test : tests) {
    if (argc == 2 && test.name != argv[1]) {
      continue;
    }
    found = true;
    try {
      if (test.run() != 0) {
        std::cerr << "Test failed: " << test.name << '\n';
        return 1;
      }
    } catch (const std::exception& error) {
      std::cerr << "Test " << test.name << " threw: " << error.what() << '\n';
      return 1;
    } catch (...) {
      std::cerr << "Test " << test.name << " threw a non-standard exception\n";
      return 1;
    }
  }
  if (!found) {
    std::cerr << "Unknown test case: " << argv[1] << '\n';
    return 2;
  }
  return 0;
}
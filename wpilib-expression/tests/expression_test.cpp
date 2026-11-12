#include "wpilib-expression/expression.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include <wpi/units/angle.hpp>
#include <wpi/units/length.hpp>

using wpilib::expression::EvaluationContext;
using wpilib::expression::Quantity;
using wpi::math::Pose2d;

int main() {
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
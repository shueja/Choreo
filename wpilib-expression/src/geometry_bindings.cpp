#include "geometry_bindings.hpp"

#include "units.hpp"

#include <wpi/math/geometry/Pose2d.hpp>
#include <wpi/math/geometry/Rotation2d.hpp>
#include <wpi/math/geometry/Transform2d.hpp>
#include <wpi/math/geometry/Translation2d.hpp>
#include <wpi/math/geometry/Twist2d.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace wpilib::expression::detail {
namespace {

using Pose2d = wpi::math::Pose2d;
using Translation2d = wpi::math::Translation2d;
using Rotation2d = wpi::math::Rotation2d;
using Transform2d = wpi::math::Transform2d;
using Twist2d = wpi::math::Twist2d;

Dimension LengthDimension() { return MakeDimension(1, 0, 0, 0, 0); }
Dimension AngleDimension() { return MakeDimension(0, 0, 0, 0, 1); }

void RequireDimension(const Quantity& value, const Dimension& expected,
                      std::string_view argument) {
  if (value.dimension != expected) {
    throw std::invalid_argument(std::string(argument) + " has an incompatible dimension");
  }
}

wpi::units::meter_t ToMeters(const Quantity& value, std::string_view argument) {
  RequireDimension(value, LengthDimension(), argument);
  return wpi::units::meter_t{value.siValue};
}

Rotation2d ToRotation(const Quantity& value, std::string_view argument) {
  RequireDimension(value, AngleDimension(), argument);
  return Rotation2d{wpi::units::radian_t{value.siValue}};
}

Quantity FromMeters(wpi::units::meter_t value) {
  return {value.value(), LengthDimension()};
}

Quantity FromRadians(wpi::units::radian_t value) {
  return {value.value(), AngleDimension()};
}

}  // namespace

void ConfigureGeometryBindings(chaiscript::ChaiScript& engine) {
  engine.add(chaiscript::user_type<Pose2d>(), "Pose2d");
  engine.add(chaiscript::user_type<Translation2d>(), "Translation2d");
  engine.add(chaiscript::user_type<Rotation2d>(), "Rotation2d");
  engine.add(chaiscript::user_type<Transform2d>(), "Transform2d");
  engine.add(chaiscript::user_type<Twist2d>(), "Twist2d");

  engine.add(chaiscript::fun([](Quantity x, Quantity y, Quantity angle) {
    return Pose2d{ToMeters(x, "x"), ToMeters(y, "y"), ToRotation(angle, "rotation")};
  }), "Pose2d");
  engine.add(chaiscript::fun([](Translation2d translation, Rotation2d rotation) {
    return Pose2d{std::move(translation), std::move(rotation)};
  }), "Pose2d");
  engine.add(chaiscript::fun([] { return Pose2d{}; }), "Pose2d");
  engine.add(chaiscript::fun([](const Pose2d& pose) { return FromMeters(pose.X()); }), "X");
  engine.add(chaiscript::fun([](const Pose2d& pose) { return FromMeters(pose.Y()); }), "Y");
  engine.add(chaiscript::fun([](const Pose2d& pose) { return pose.Translation(); }), "Translation");
  engine.add(chaiscript::fun([](const Pose2d& pose) { return pose.Rotation(); }), "Rotation");
  engine.add(chaiscript::fun(&Pose2d::RotateBy), "RotateBy");
  engine.add(chaiscript::fun(&Pose2d::RelativeTo), "RelativeTo");
  engine.add(chaiscript::fun(&Pose2d::TransformBy), "TransformBy");

  engine.add(chaiscript::fun([](Quantity x, Quantity y) {
    return Translation2d{ToMeters(x, "x"), ToMeters(y, "y")};
  }), "Translation2d");
  engine.add(chaiscript::fun([] { return Translation2d{}; }), "Translation2d");
  engine.add(chaiscript::fun([](const Translation2d& translation) {
    return FromMeters(translation.X());
  }), "X");
  engine.add(chaiscript::fun([](const Translation2d& translation) {
    return FromMeters(translation.Y());
  }), "Y");
  engine.add(chaiscript::fun([](const Translation2d& translation,
                                const Translation2d& other) {
    return FromMeters(translation.Distance(other));
  }), "Distance");
  engine.add(chaiscript::fun(&Translation2d::RotateBy), "RotateBy");
  engine.add(chaiscript::fun([](const Translation2d& translation) {
    return FromMeters(translation.Norm());
  }), "Norm");

  engine.add(chaiscript::fun([](Quantity angle) { return ToRotation(angle, "angle"); }),
             "Rotation2d");
  engine.add(chaiscript::fun([](double x, double y) { return Rotation2d{x, y}; }),
             "Rotation2d");
  engine.add(chaiscript::fun([] { return Rotation2d{}; }), "Rotation2d");
  engine.add(chaiscript::fun([](const Rotation2d& rotation) {
    return FromRadians(rotation.Radians());
  }), "Radians");
  engine.add(chaiscript::fun(&Rotation2d::RotateBy), "RotateBy");
  engine.add(chaiscript::fun(&Rotation2d::RelativeTo), "RelativeTo");
  engine.add(chaiscript::fun(&Rotation2d::Cos), "Cos");
  engine.add(chaiscript::fun(&Rotation2d::Sin), "Sin");
  engine.add(chaiscript::fun(&Rotation2d::Tan), "Tan");

  engine.add(chaiscript::fun([](Quantity x, Quantity y, Quantity angle) {
    return Transform2d{ToMeters(x, "x"), ToMeters(y, "y"),
                       ToRotation(angle, "rotation")};
  }), "Transform2d");
  engine.add(chaiscript::fun([](Translation2d translation, Rotation2d rotation) {
    return Transform2d{std::move(translation), std::move(rotation)};
  }), "Transform2d");
  engine.add(chaiscript::fun([](const Pose2d& initial, const Pose2d& final) {
    return Transform2d{initial, final};
  }), "Transform2d");
  engine.add(chaiscript::fun([] { return Transform2d{}; }), "Transform2d");
  engine.add(chaiscript::fun([](const Transform2d& transform) {
    return FromMeters(transform.X());
  }), "X");
  engine.add(chaiscript::fun([](const Transform2d& transform) {
    return FromMeters(transform.Y());
  }), "Y");
  engine.add(chaiscript::fun([](const Transform2d& transform) {
    return transform.Translation();
  }), "Translation");
  engine.add(chaiscript::fun([](const Transform2d& transform) {
    return transform.Rotation();
  }), "Rotation");
  engine.add(chaiscript::fun(&Transform2d::Inverse), "Inverse");
  engine.add(chaiscript::fun(&Transform2d::Log), "Log");

  engine.add(chaiscript::fun([](Quantity dx, Quantity dy, Quantity dtheta) {
    return Twist2d{ToMeters(dx, "dx"), ToMeters(dy, "dy"),
                   ToRotation(dtheta, "dtheta").Radians()};
  }), "Twist2d");
  engine.add(chaiscript::fun([](const Twist2d& twist) { return FromMeters(twist.dx); }), "dx");
  engine.add(chaiscript::fun([](const Twist2d& twist) { return FromMeters(twist.dy); }), "dy");
  engine.add(chaiscript::fun([](const Twist2d& twist) { return FromRadians(twist.dtheta); }),
             "dtheta");
  engine.add(chaiscript::fun(&Twist2d::Exp), "Exp");
}

}  // namespace wpilib::expression::detail
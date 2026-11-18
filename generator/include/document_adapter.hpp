#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <choreo/document/constraint.hpp>
#include <choreo/document/project.hpp>
#include <choreo/document/waypoint.hpp>
#include <trajopt/constraint/angular_velocity_max_magnitude_constraint.hpp>
#include <trajopt/constraint/heading_angle_constraint.hpp>
#include <trajopt/constraint/keep_in_circle_constraint.hpp>
#include <trajopt/constraint/linear_velocity_max_magnitude_constraint.hpp>
#include <trajopt/differential_trajectory_generator.hpp>
#include <trajopt/swerve_trajectory_generator.hpp>
#include <wpi/math/trajectory/DifferentialSample.hpp>
#include <wpi/math/trajectory/HolonomicSample.hpp>

namespace choreo {

inline std::optional<std::size_t> ResolveWaypointId(
    const document::WaypointId& id,
    const std::vector<document::Waypoint>& waypoints) {
  if (const auto* uuid = std::get_if<std::string>(&id)) {
    for (std::size_t index = 0; index < waypoints.size(); ++index) {
      if (waypoints[index].uuid == *uuid) {
        return index;
      }
    }
    return std::nullopt;
  }
  if (waypoints.empty()) {
    return std::nullopt;
  }
  switch (std::get<document::WaypointBoundary>(id)) {
    case document::WaypointBoundary::First:
      return 0;
    case document::WaypointBoundary::Last:
      return waypoints.size() - 1;
  }
  return std::nullopt;
}

inline trajopt::Pose2d ToTrajoptPose(const document::Waypoint& waypoint) {
  const auto& pose = waypoint.pose.value;
  return {pose.X().value(), pose.Y().value(),
          trajopt::Rotation2d{pose.Rotation().Radians().value()}};
}

inline wpi::math::HolonomicSample ToWPILibSample(
  const trajopt::SwerveTrajectorySample& sample) {
  wpi::math::HolonomicSample result;
  result.time = wpi::units::second_t{sample.timestamp};
  result.pose = {wpi::units::meter_t{sample.x}, wpi::units::meter_t{sample.y},
         wpi::math::Rotation2d{
           wpi::units::radian_t{sample.heading}}};
  result.velocity.vx = wpi::units::meters_per_second_t{sample.velocity_x};
  result.velocity.vy = wpi::units::meters_per_second_t{sample.velocity_y};
  result.velocity.omega =
    wpi::units::radians_per_second_t{sample.angular_velocity};
  result.acceleration.ax =
    wpi::units::meters_per_second_squared_t{sample.acceleration_x};
  result.acceleration.ay =
    wpi::units::meters_per_second_squared_t{sample.acceleration_y};
  result.acceleration.alpha =
    wpi::units::radians_per_second_squared_t{sample.angular_acceleration};
  return result;
}

inline wpi::math::DifferentialSample ToWPILibSample(
  const trajopt::DifferentialTrajectorySample& sample) {
  const double cos_heading = std::cos(sample.heading);
  const double sin_heading = std::sin(sample.heading);
  const double linear_velocity = 0.5 * (sample.velocity_l + sample.velocity_r);
  const double linear_acceleration =
    0.5 * (sample.acceleration_l + sample.acceleration_r);
  wpi::math::HolonomicSample result;
  result.time = wpi::units::second_t{sample.timestamp};
  result.pose = {wpi::units::meter_t{sample.x}, wpi::units::meter_t{sample.y},
         wpi::math::Rotation2d{
           wpi::units::radian_t{sample.heading}}};
  result.velocity.vx = wpi::units::meters_per_second_t{
    linear_velocity * cos_heading};
  result.velocity.vy = wpi::units::meters_per_second_t{
    linear_velocity * sin_heading};
  result.velocity.omega =
    wpi::units::radians_per_second_t{sample.angular_velocity};
  result.acceleration.ax = wpi::units::meters_per_second_squared_t{
    linear_acceleration * cos_heading -
    sample.angular_velocity * linear_velocity * sin_heading};
  result.acceleration.ay = wpi::units::meters_per_second_squared_t{
    linear_acceleration * sin_heading +
    sample.angular_velocity * linear_velocity * cos_heading};
  result.acceleration.alpha =
    wpi::units::radians_per_second_squared_t{sample.angular_acceleration};
  return {result, wpi::units::meters_per_second_t{sample.velocity_l},
      wpi::units::meters_per_second_t{sample.velocity_r}};
}

inline std::vector<trajopt::Translation2d> ToTrajoptBumperPoints(
    const document::RobotConfig& config) {
  std::vector<trajopt::Translation2d> points;
  const auto& vertices = config.bumpers.value.Vertices();
  points.reserve(vertices.size());
  for (const auto& point : vertices) {
    points.emplace_back(point.X().value(), point.Y().value());
  }
  return points;
}

inline trajopt::SwerveDrivetrain ToSwerveDrivetrain(
    const document::RobotConfig& config) {
  if (!config.wheels.swerve) {
    throw std::invalid_argument("swerve project has no swerve wheel config");
  }
  std::vector<trajopt::Translation2d> modules;
  modules.reserve(config.wheels.swerve->modules.size());
  for (const auto& module : config.wheels.swerve->modules) {
    modules.emplace_back(module.value.X().value(), module.value.Y().value());
  }
  return {.mass = config.mass.value.value(),
          .moi = config.inertia.value.value(),
          .wheel_radius = config.radius.value.value(),
          .wheel_max_angular_velocity =
              (config.motor.freeSpeed.value / config.gearing.value).value(),
          .wheel_max_torque =
              (config.motor.stallTorque.value * config.gearing.value).value(),
          .wheel_cof = config.cof.value.value(),
          .modules = std::move(modules)};
}

inline trajopt::DifferentialDrivetrain ToDifferentialDrivetrain(
    const document::RobotConfig& config) {
  if (!config.wheels.differential) {
    throw std::invalid_argument(
        "differential project has no differential wheel config");
  }
  return {.mass = config.mass.value.value(),
          .moi = config.inertia.value.value(),
          .wheel_radius = config.radius.value.value(),
          .wheel_max_angular_velocity =
              (config.motor.freeSpeed.value / config.gearing.value).value(),
          .wheel_max_torque =
              (config.motor.stallTorque.value * config.gearing.value).value(),
          .wheel_cof = config.cof.value.value(),
          .trackwidth = config.wheels.differential->trackwidth.value.value()};
}

inline trajopt::Constraint ToTrajoptConstraint(
    const document::ConstraintData& constraint,
    const std::vector<trajopt::Translation2d>& bumperPoints) {
  return std::visit(
      [&bumperPoints](const auto& data) -> trajopt::Constraint {
        using Type = std::remove_cvref_t<decltype(data)>;
        if constexpr (std::is_same_v<Type, document::MaxLinearVelocity>) {
          return trajopt::LinearVelocityMaxMagnitudeConstraint{
              data.max.value.value()};
        } else if constexpr (std::is_same_v<Type,
                                            document::MaxAngularVelocity>) {
          return trajopt::AngularVelocityMaxMagnitudeConstraint{
              data.max.value.value()};
        } else if constexpr (std::is_same_v<Type, document::KeepInCircle>) {
          const auto& ellipse = data.circle.value;
          const double xRadius = ellipse.XSemiAxis().value();
          const double yRadius = ellipse.YSemiAxis().value();
          if (std::abs(xRadius - yRadius) > 1e-9) {
            throw std::invalid_argument(
                "the trajectory generator only supports circular keep-in regions");
          }
          const auto& center = ellipse.Center();
          return trajopt::KeepInCircleConstraint{
              bumperPoints, {center.X().value(), center.Y().value()}, xRadius};
        } else {
          return trajopt::HeadingAngleConstraint{
              data.heading.value.Radians().value(),
              data.tolerance.value.value()};
        }
      },
      constraint);
}

}  // namespace choreo
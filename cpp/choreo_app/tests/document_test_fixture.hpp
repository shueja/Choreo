#pragma once

#include <map>
#include <optional>
#include <utility>

#include "choreo/document/store.hpp"

inline choreo::document::Project MakeTestProject() {
  using namespace choreo::document;
  Project project;
  project.uuid = "project";
  project.variables.emplace(
      "length", Variable{"length", "", "2 m", VariableType::Quantity,
                         std::map<std::string, int>{{"meter", 1}}, 0.0});
  project.variables.emplace(
      "twice", Variable{"twice", "", "length * 2", VariableType::Quantity,
                        std::map<std::string, int>{{"meter", 1}}, 0.0});
  return project;
}

inline choreo::document::Document MakeTestDocument(
    choreo::document::Project project) {
  using namespace choreo::document;
  auto& config = project.config;
  config.mass.source = "5 kg";
  config.inertia.source = "3 kg*m^2";
  config.gearing.source = "6";
  config.radius.source = "0.05 m";
  config.cof.source = "1";
  config.wheels.differential = DifferentialWheels{};
  config.wheels.differential->trackwidth.source = "0.5 m";
  config.bumpers.source =
      "[Translation2d(0 m, 0 m), Translation2d(1 m, 0 m), Translation2d(0 m, 1 "
      "m)]";
  config.motor.freeSpeed.source = "60 RPM";
  config.motor.stallTorque.source = "1 N*m";
  config.motor.kT.source = "0.02 N*m/A";
  config.motor.kV.source = "0.02 V*s/rad";
  config.motor.supplyLimit.source = "40 A";
  config.motor.statorLimit.source = "60 A";
  Document document;
  document.project = std::move(project);
  Trajectory trajectory;
  trajectory.uuid = "candidate";
  trajectory.params.targetDt.source = "0.02 s";
  trajectory.params.waypoints.push_back(
      Waypoint{"point",
               {"Pose2d(length, 0 m, 0 rad)", wpi::math::Pose2d{}},
               1,
               false,
               true,
               true,
               false});
  trajectory.params.constraints.push_back(
      Constraint{"circle", WaypointBoundary::First, std::nullopt,
                 KeepInCircle{EllipseExpression{
                     "Ellipse2d(Translation2d(), 2 m)",
                     wpi::math::Ellipse2d{wpi::math::Translation2d{}, 1.0}}},
                 true});
  trajectory.snapshot = trajectory.params;
  document.trajectories = document.trajectories.set("candidate", trajectory);
  return document;
}
#pragma once

#include <map>
#include <optional>
#include <utility>

#include "choreo/document/store.hpp"

inline constexpr char kTestProjectUuid[] =
    "00000000-0000-4000-8000-000000002001";
inline constexpr char kTestLengthVariableUuid[] =
    "00000000-0000-4000-8000-000000002002";
inline constexpr char kTestTwiceVariableUuid[] =
    "00000000-0000-4000-8000-000000002003";
inline constexpr char kTestOccupiedVariableUuid[] =
    "00000000-0000-4000-8000-00000000200f";
inline constexpr char kTestTrajectoryUuid[] =
    "00000000-0000-4000-8000-000000002004";
inline constexpr char kTestWaypointUuid[] =
    "00000000-0000-4000-8000-000000002005";
inline constexpr char kTestConstraintUuid[] =
    "00000000-0000-4000-8000-000000002006";
inline constexpr char kTestSecondTrajectoryUuid[] =
    "00000000-0000-4000-8000-000000002007";
inline constexpr char kTestStandaloneTrajectoryUuid[] =
    "00000000-0000-4000-8000-000000002008";
inline constexpr char kTestEndWaypointUuid[] =
    "00000000-0000-4000-8000-000000002009";
inline constexpr char kTestStartEventUuid[] =
    "00000000-0000-4000-8000-00000000200a";
inline constexpr char kTestEndEventUuid[] =
    "00000000-0000-4000-8000-00000000200b";
inline constexpr char kTestMiddleWaypointUuid[] =
    "00000000-0000-4000-8000-00000000200c";
inline constexpr char kTestMiddleEventUuid[] =
    "00000000-0000-4000-8000-00000000200d";
inline constexpr char kTestWrongTrajectoryUuid[] =
    "00000000-0000-4000-8000-00000000200e";

inline choreo::document::Project MakeTestProject() {
  using namespace choreo::document;
  Project project;
  project.uuid = kTestProjectUuid;
  project.variables.emplace(
      kTestLengthVariableUuid,
      Variable{"length", "", "2 m", VariableType::Quantity,
                         std::map<std::string, int>{{"meter", 1}}, 0.0});
  project.variables.emplace(
      kTestTwiceVariableUuid,
      Variable{"twice", "", "length * 2", VariableType::Quantity,
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
    trajectory.uuid = kTestTrajectoryUuid;
  trajectory.params.targetDt.source = "0.02 s";
  trajectory.params.waypoints.push_back(
    Waypoint{kTestWaypointUuid,
               {"Pose2d(length, 0 m, 0 rad)", wpi::math::Pose2d{}},
               1,
               false,
               true,
               true,
               false});
  trajectory.params.constraints.push_back(
    Constraint{kTestConstraintUuid, WaypointBoundary::First, std::nullopt,
                 KeepInCircle{EllipseExpression{
                     "Ellipse2d(Translation2d(), 2 m)",
                     wpi::math::Ellipse2d{wpi::math::Translation2d{}, 1.0}}},
                 true});
  trajectory.snapshot = trajectory.params;
    document.trajectories =
            document.trajectories.set(kTestTrajectoryUuid, trajectory);
  return document;
}
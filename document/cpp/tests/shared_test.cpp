#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <wpi/util/json.hpp>

#include "choreo/document/document.hpp"
#include "choreo/document/project.hpp"
#include "choreo/document/robot_trajectory.hpp"
#include "choreo/document/variable.hpp"

bool Rejects(const wpi::util::json& json) {
  try {
    choreo::document::LoadRobotTrajectory(json.to_string());
    return false;
  } catch (const std::exception&) {
    return true;
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  choreo::document::Waypoint waypoint{
      "waypoint", {"Pose2d()", wpi::math::Pose2d{}}, 10, false, true, true,
      false};
  const auto restored =
      wpi::util::json{waypoint}.get<choreo::document::Waypoint>();
  if (restored.uuid != waypoint.uuid ||
      restored.pose.source != waypoint.pose.source ||
      restored.pose.value != waypoint.pose.value ||
      restored.intervals != waypoint.intervals) {
    return EXIT_FAILURE;
  }

  choreo::document::Project project;
  project.uuid = "project";
  project.name = "Project";
  project.config.wheels.differential = choreo::document::DifferentialWheels{};
  const std::vector<wpi::math::Translation2d> triangle{
      wpi::math::Translation2d{},
      wpi::math::Translation2d{wpi::units::meter_t{1}, wpi::units::meter_t{0}},
      wpi::math::Translation2d{wpi::units::meter_t{0}, wpi::units::meter_t{1}}};
  project.config.bumpers.value = wpi::math::Polygon2d{triangle};
  if (wpi::util::json{wpi::util::json{project.config}
                          .get<choreo::document::RobotConfig>()} !=
      wpi::util::json{project.config}) {
    return EXIT_FAILURE;
  }
  project.variables.emplace(
      "length",
      choreo::document::Variable{
          "length", "field", "2 m", choreo::document::VariableType::Quantity,
          std::map<std::string, int>{{"meter", 1}}, 2.0});
  project.variables.emplace(
      "pose", choreo::document::Variable{"pose", "", "Pose2d()",
                                         choreo::document::VariableType::Pose2d,
                                         std::nullopt, wpi::math::Pose2d{}});
  project.variables.emplace(
      "translation",
      choreo::document::Variable{"translation", "", "Translation2d()",
                                 choreo::document::VariableType::Translation2d,
                                 std::nullopt, wpi::math::Translation2d{}});
  project.variables.emplace(
      "polygon",
      choreo::document::Variable{"polygon", "", "vertices",
                                 choreo::document::VariableType::Polygon2d,
                                 std::nullopt, wpi::math::Polygon2d{triangle}});
  project.variables.emplace(
      "rotation",
      choreo::document::Variable{"rotation", "", "Rotation2d()",
                                 choreo::document::VariableType::Rotation2d,
                                 std::nullopt, wpi::math::Rotation2d{}});
  project.variables.emplace(
      "transform",
      choreo::document::Variable{"transform", "", "Transform2d()",
                                 choreo::document::VariableType::Transform2d,
                                 std::nullopt, wpi::math::Transform2d{}});
  project.variables.emplace(
      "twist",
      choreo::document::Variable{"twist", "", "Twist2d()",
                                 choreo::document::VariableType::Twist2d,
                                 std::nullopt, wpi::math::Twist2d{}});
  project.variables.emplace(
      "rectangle",
      choreo::document::Variable{
          "rectangle", "", "Rectangle2d()",
          choreo::document::VariableType::Rectangle2d, std::nullopt,
          wpi::math::Rectangle2d{wpi::math::Pose2d{}, wpi::units::meter_t{2},
                                 wpi::units::meter_t{1}}});
  project.variables.emplace(
      "ellipse",
      choreo::document::Variable{
          "ellipse", "", "Ellipse2d()",
          choreo::document::VariableType::Ellipse2d, std::nullopt,
          wpi::math::Ellipse2d{wpi::math::Pose2d{}, wpi::units::meter_t{2},
                               wpi::units::meter_t{1}}});
  const wpi::util::json projectJson{project};
  const auto restoredProject = projectJson.get<choreo::document::Project>();
  if (wpi::util::json{restoredProject} != projectJson ||
      std::get<double>(restoredProject.variables.at("length").value) != 2.0) {
    return EXIT_FAILURE;
  }
  auto invalidProject = projectJson;
  invalidProject["config"]["mass"]["unexpected"] = 1;
  try {
    invalidProject.get<choreo::document::Project>();
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  invalidProject = projectJson;
  invalidProject["config"]["bumpers"]["val"]["vertices"] =
      std::vector<wpi::math::Translation2d>(2);
  try {
    invalidProject.get<choreo::document::Project>();
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  invalidProject = projectJson;
  invalidProject["config"]["bumpers"]["val"]["vertices"] =
      std::vector<wpi::math::Translation2d>{triangle[0], triangle[2],
                                            triangle[1]};
  try {
    invalidProject.get<choreo::document::Project>();
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  invalidProject = projectJson;
  invalidProject["variables"]["variables"]["length"]["namespacePath"] =
      "bad..path";
  try {
    invalidProject.get<choreo::document::Project>();
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }

  wpi::math::HolonomicSample first;
  first.time = wpi::units::second_t{7.0};
  wpi::math::HolonomicSample second;
  second.time = wpi::units::second_t{5.0};
  auto output = wpi::util::json::object(
      "sampleType", "Swerve", "waypoints", std::vector<double>{5.0, 7.0},
      "splits", std::vector<int>{0}, "trajectory",
      wpi::util::json::object(
          "samples", std::vector<wpi::math::HolonomicSample>{first, second}));
  auto event = wpi::util::json::object(
      "uuid", "event", "name", "Marker", "from",
      wpi::util::json::object(
          "target", "first", "targetTime", 6.0, "offset",
          wpi::util::json::object("exp", "0.1 s", "val", 0.1)));
  auto unresolved = event;
  unresolved["uuid"] = "unresolved";
  unresolved["from"]["targetTime"] = nullptr;
  auto root = wpi::util::json::object(
      "uuid", "path", "version", 4, "name", "Path", "config", nullptr,
      "snapshot", nullptr, "params",
      wpi::util::json::object(
          "waypoints", wpi::util::json::array(), "constraints",
          wpi::util::json::array(), "targetDt",
          wpi::util::json::object("exp", "0.02 s", "val", 0.02)),
      "output", output, "events",
      std::vector<wpi::util::json>{event, unresolved});
  auto linear = wpi::util::json::object(
      "uuid", "linear", "from", "first", "to", "last", "enabled", true, "data",
      wpi::util::json::object(
          "type", "MaxLinearVelocity", "max",
          wpi::util::json::object("exp", "3 m/s", "val", 3.0)));
  auto angular = wpi::util::json::object(
      "uuid", "angular", "from", "first", "enabled", true, "data",
      wpi::util::json::object(
          "type", "MaxAngularVelocity", "max",
          wpi::util::json::object("exp", "2 rad/s", "val", 2.0)));
  auto heading = wpi::util::json::object(
      "uuid", "heading", "from", "first", "enabled", true, "data",
      wpi::util::json::object(
          "type", "Heading", "heading",
          wpi::util::json::object("exp", "Rotation2d()", "val",
                                  wpi::math::Rotation2d{}),
          "tolerance", wpi::util::json::object("exp", "0.1 rad", "val", 0.1)));
  auto circle = wpi::util::json::object(
      "uuid", "circle", "from", "first", "enabled", true, "data",
      wpi::util::json::object(
          "type", "KeepInCircle", "circle",
        choreo::document::EllipseExpression{
          "Ellipse2d(Translation2d(), 1 m)",
          wpi::math::Ellipse2d{wpi::math::Translation2d{}, 1.0}}));
  const auto circleExpression =
      circle.at("data").at("circle").get<choreo::document::EllipseExpression>();
  if (wpi::util::json{circleExpression} != circle.at("data").at("circle")) {
    return EXIT_FAILURE;
  }
  choreo::document::EllipseExpression ellipseExpression{
      "ellipse",
      wpi::math::Ellipse2d{
          wpi::math::Pose2d{wpi::math::Translation2d{},
                            wpi::math::Rotation2d{wpi::units::radian_t{0.25}}},
          wpi::units::meter_t{2.0}, wpi::units::meter_t{1.0}}};
  const auto ellipseJson = wpi::util::json{ellipseExpression};
  if (wpi::util::json{ellipseJson.get<choreo::document::EllipseExpression>()} !=
      ellipseJson) {
    return EXIT_FAILURE;
  }
  root["params"]["constraints"] =
      std::vector<wpi::util::json>{linear, angular, heading, circle};
  for (const auto& serializedConstraint :
       root["params"]["constraints"].get_array()) {
    if (wpi::util::json{
            serializedConstraint.get<choreo::document::Constraint>()} !=
        serializedConstraint) {
      return EXIT_FAILURE;
    }
  }
  root["params"]["waypoints"] = std::vector<wpi::util::json>{waypoint.ToJson()};
  root["snapshot"] = root["params"];
  if (wpi::util::json{root.at("params").get<choreo::document::Parameters>()} !=
          root.at("params") ||
      wpi::util::json{root.at("events")
                          .get_array()[0]
                          .get<choreo::document::EventMarker>()} != event ||
      wpi::util::json{
          event.at("from").get<choreo::document::EventMarkerData>()} !=
          event.at("from")) {
    return EXIT_FAILURE;
  }
  const auto document = root.get<choreo::document::Trajectory>();
  const wpi::util::json normalizedOutput{*document.output};
  if (wpi::util::json{
          normalizedOutput.get<choreo::document::TrajectoryOutput>()} !=
      normalizedOutput) {
    return EXIT_FAILURE;
  }
  const wpi::util::json normalized{document};
  {
    std::ofstream projectFile{std::filesystem::path{argv[1]} /
                              "schema-project.json"};
    std::ofstream trajectoryFile{std::filesystem::path{argv[1]} /
                                 "schema-trajectory.json"};
    projectFile << projectJson.to_string();
    trajectoryFile << normalized.to_string();
    if (!projectFile || !trajectoryFile) {
      return EXIT_FAILURE;
    }
  }
  if (wpi::util::json{normalized.get<choreo::document::Trajectory>()} !=
          normalized ||
      document.params.constraints.size() != 4 || !document.snapshot ||
      document.events.size() != 2 || document.events[1].from.targetTime) {
    return EXIT_FAILURE;
  }
  const auto trajectory =
      choreo::document::LoadRobotTrajectory(root.to_string());
  const auto& samples = std::get<std::optional<wpi::math::HolonomicTrajectory>>(
                            trajectory.output.trajectory)
                            ->Samples();
  if (samples.size() != 2 || samples[0].time.value() != 0.0 ||
      samples[1].time.value() != 2.0 || trajectory.output.waypoints[0] != 0.0 ||
      trajectory.events.size() != 1 ||
      *trajectory.events[0].from.targetTime != 1.0 ||
      std::abs(trajectory.events[0].from.offset.value.value() - 0.1) > 1e-12 ||
      trajectory.warnings.size() != 1) {
    return EXIT_FAILURE;
  }
  auto invalid = root;
  invalid["version"] = 3;
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["params"]["constraints"][0]["data"]["unexpected"] = 1;
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["events"][0]["from"]["targetExtra"] = 1;
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["output"]["trajectory"]["samples"][0]["pose"]["translation"]
         ["extra"] = 1;
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["output"] = nullptr;
  if (!Rejects(invalid) || invalid.get<choreo::document::Trajectory>().output) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["params"]["constraints"][3]["data"]["circle"]["val"]["ySemiAxis"] =
      2.0;
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["params"]["constraints"][3]["data"]["circle"]["val"]["xSemiAxis"] =
      0.0;
  invalid["params"]["constraints"][3]["data"]["circle"]["val"]["ySemiAxis"] =
      0.0;
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["output"]["trajectory"]["samples"] =
      std::vector<wpi::math::HolonomicSample>{};
  invalid["output"]["splits"] = std::vector<int>{0};
  const auto emptyOutput = invalid.get<choreo::document::Trajectory>().output;
  if (!emptyOutput || std::get<std::optional<wpi::math::HolonomicTrajectory>>(
                          emptyOutput->trajectory)) {
    return EXIT_FAILURE;
  }
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  invalid = root;
  invalid["output"]["splits"] = std::vector<int>{0, 1};
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  wpi::math::HolonomicSample splitStart;
  splitStart.time = wpi::units::second_t{3.0};
  wpi::math::HolonomicSample withinSplit;
  withinSplit.time = wpi::units::second_t{1.0};
  wpi::math::HolonomicSample trajectoryStart;
  invalid = root;
  invalid["output"]["trajectory"]["samples"] =
      std::vector<wpi::math::HolonomicSample>{trajectoryStart, splitStart,
                                              withinSplit};
  invalid["output"]["splits"] = std::vector<int>{0, 1};
  if (!Rejects(invalid)) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
#include "choreo/document/document.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

#include <wpilib-expression/expression.hpp>

#include "choreo/document/document_store.hpp"
#include "choreo/document/evaluation.hpp"
#include "choreo/document/history.hpp"

template <typename ExpressionType>
bool CheckDimension(wpilib::expression::EvaluationContext& context,
                    const char* source, double expected) {
  using Dimension = typename ExpressionType::DimensionType;
  const auto measurement = context.Evaluate(
      source, choreo::document::evaluation::RuntimeDimension<Dimension>::unit);
  const auto value =
      choreo::document::evaluation::FromMeasurement<Dimension>(measurement);
  return value && std::abs(value->value() - expected) < 1e-10;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  using namespace choreo::document;

  static_assert(std::is_constructible_v<wpi::units::meter_t,
                                        decltype(RobotConfig{}.radius)::Value>);
  static_assert(
      std::is_constructible_v<wpi::units::radians_per_second_t,
                              decltype(MotorConfig{}.freeSpeed)::Value>);
  static_assert(
      evaluation::RuntimeDimension<dimension::Angle>::unit.base_units()
          .radian() == 1);
  static_assert(std::is_same_v<decltype(MotorConfig{}.freeSpeed),
                               Expression<dimension::AngularVelocity>>);
  static_assert(std::is_same_v<decltype(MotorConfig{}.kT),
                               Expression<dimension::TorqueConstant>>);
  static_assert(std::is_same_v<decltype(MotorConfig{}.kV),
                               Expression<dimension::BackEmfConstant>>);

  wpilib::expression::EvaluationContext context;
  auto length = evaluation::FromMeasurement<dimension::Length>(context.Evaluate(
      "2 ft", evaluation::RuntimeDimension<dimension::Length>::unit));
  auto angularSpeed =
      evaluation::FromMeasurement<dimension::AngularVelocity>(context.Evaluate(
          "60 RPM",
          evaluation::RuntimeDimension<dimension::AngularVelocity>::unit));
  auto wrongDimension =
      evaluation::FromMeasurement<dimension::Length>(context.Evaluate("2 s"));
  const units::precise_unit currencyUnit{
      units::detail::unit_data{0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}};
  const units::precise_unit countUnit{
      units::detail::unit_data{0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}};
  if (!length || std::abs(length->value() - 0.6096) > 1e-12 || !angularSpeed ||
      std::abs(angularSpeed->value() - 6.283185307179586) > 1e-12 ||
      wrongDimension ||
      evaluation::FromMeasurement<dimension::Dimensionless>(
          units::precise_measurement{1.0, currencyUnit}) ||
      evaluation::FromMeasurement<dimension::Dimensionless>(
          units::precise_measurement{1.0, countUnit}) ||
      evaluation::FromMeasurement<dimension::Length>(units::precise_measurement{
          std::numeric_limits<double>::infinity(),
          evaluation::RuntimeDimension<dimension::Length>::unit})) {
    return EXIT_FAILURE;
  }
  if (!CheckDimension<decltype(MotorConfig{}.stallTorque)>(context, "3 N*m",
                                                           3.0) ||
      !CheckDimension<decltype(MotorConfig{}.kT)>(context, "0.018 N*m/A",
                                                  0.018) ||
      !CheckDimension<decltype(MotorConfig{}.kV)>(context, "0.02 V*s/rad",
                                                  0.02) ||
      !CheckDimension<decltype(MotorConfig{}.supplyLimit)>(context, "40 A",
                                                           40.0) ||
      !CheckDimension<decltype(RobotConfig{}.mass)>(context, "5 kg", 5.0) ||
      !CheckDimension<decltype(RobotConfig{}.inertia)>(context, "3 kg*m^2",
                                                       3.0) ||
      !CheckDimension<decltype(RobotConfig{}.gearing)>(context, "6.5", 6.5) ||
      !CheckDimension<decltype(MaxLinearVelocity{}.max)>(context, "3 m/s",
                                                         3.0) ||
      !CheckDimension<decltype(MaxAngularVelocity{}.max)>(context, "2 rad/s",
                                                          2.0) ||
      !CheckDimension<decltype(HeadingConstraint{}.tolerance)>(
          context, "90 deg", 1.5707963267948966) ||
      !CheckDimension<decltype(Parameters{}.targetDt)>(context, "20 ms",
                                                       0.02) ||
      !CheckDimension<decltype(EventMarkerData{}.offset)>(context, "1 s",
                                                          1.0)) {
    return EXIT_FAILURE;
  }

  Waypoint waypoint{
      "waypoint", {"Pose2d()", wpi::math::Pose2d{}}, 10, false, true, true,
      false};
  auto restoredWaypoint = Waypoint::FromJson(waypoint.ToJson());
  if (restoredWaypoint.uuid != waypoint.uuid ||
      restoredWaypoint.pose.source != waypoint.pose.source ||
      restoredWaypoint.pose.value != waypoint.pose.value ||
      restoredWaypoint.intervals != waypoint.intervals ||
      restoredWaypoint.fixTranslation != waypoint.fixTranslation) {
    return EXIT_FAILURE;
  }
  Expression<dimension::Length> lengthExpression{"2 ft", *length};
  auto restoredLength =
      Expression<dimension::Length>::FromJson(lengthExpression.ToJson());
  if (restoredLength.source != lengthExpression.source ||
      restoredLength.value != lengthExpression.value) {
    return EXIT_FAILURE;
  }
  RobotConfig config;
  config.wheels.differential = DifferentialWheels{lengthExpression};
  config.wheels.swerve = SwerveWheels{};
  config.wheels.mecanum = MecanumWheels{};
  config.bumpers = {"polygon",
                    wpi::math::Polygon2d{std::vector<wpi::math::Translation2d>{
                        wpi::math::Translation2d{},
                        wpi::math::Translation2d{wpi::units::meter_t{1},
                                                 wpi::units::meter_t{0}},
                        wpi::math::Translation2d{wpi::units::meter_t{0},
                                                 wpi::units::meter_t{1}}}}};
  const auto configJson = config.ToJson();
  if (RobotConfig::FromJson(configJson).ToJson() != configJson) {
    return EXIT_FAILURE;
  }
  CodeGenConfig codegen;
  codegen.genVars = false;
  if (CodeGenConfig::FromJson(codegen.ToJson()).ToJson() != codegen.ToJson()) {
    return EXIT_FAILURE;
  }
  Project variablesProject;
  variablesProject.uuid = "project";
  variablesProject.variables.emplace(
      "length", Variable{"length", "", "2 m", VariableType::Quantity,
                         std::map<std::string, int>{{"meter", 1}}, 0.0});
  variablesProject.variables.emplace(
      "twice", Variable{"twice", "", "length * 2", VariableType::Quantity,
                        std::map<std::string, int>{{"meter", 1}}, 0.0});
  auto evaluated = evaluation::EvaluateVariables(variablesProject);
  if (std::get<double>(evaluated.variables.at("twice").value) != 4.0 ||
      std::get<double>(variablesProject.variables.at("twice").value) != 0.0) {
    return EXIT_FAILURE;
  }
  variablesProject.variables.at("length").expression = "twice";
  try {
    evaluation::EvaluateVariables(variablesProject);
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  if (std::get<double>(variablesProject.variables.at("length").value) != 0.0) {
    return EXIT_FAILURE;
  }
  variablesProject.variables.at("length").expression = "1 s";
  try {
    evaluation::EvaluateVariables(variablesProject);
    return EXIT_FAILURE;
  } catch (const evaluation::EvaluationError& error) {
    if (error.ObjectId() != "length" || error.Field() != "expression") {
      return EXIT_FAILURE;
    }
  }
  variablesProject.variables.at("length").expression = "2 m";
  auto& evaluatedConfig = variablesProject.config;
  evaluatedConfig.mass.source = "5 kg";
  evaluatedConfig.inertia.source = "3 kg*m^2";
  evaluatedConfig.gearing.source = "6";
  evaluatedConfig.radius.source = "0.05 m";
  evaluatedConfig.cof.source = "1";
  evaluatedConfig.wheels.differential = DifferentialWheels{};
  evaluatedConfig.wheels.differential->trackwidth.source = "0.5 m";
  evaluatedConfig.bumpers.source =
      "[Translation2d(0 m, 0 m), Translation2d(1 m, 0 m), Translation2d(0 m, 1 "
      "m)]";
  evaluatedConfig.motor.freeSpeed.source = "60 RPM";
  evaluatedConfig.motor.stallTorque.source = "1 N*m";
  evaluatedConfig.motor.kT.source = "0.02 N*m/A";
  evaluatedConfig.motor.kV.source = "0.02 V*s/rad";
  evaluatedConfig.motor.supplyLimit.source = "40 A";
  evaluatedConfig.motor.statorLimit.source = "60 A";
  Document candidate;
  candidate.project = variablesProject;
  Trajectory candidateTrajectory;
  candidateTrajectory.uuid = "candidate";
  candidateTrajectory.params.targetDt.source = "0.02 s";
  candidateTrajectory.params.waypoints.push_back(
      Waypoint{"point",
               {"Pose2d(length, 0 m, 0 rad)", wpi::math::Pose2d{}},
               1,
               false,
               true,
               true,
               false});
  candidateTrajectory.params.constraints.push_back(
      Constraint{"circle", WaypointBoundary::First, std::nullopt,
                 KeepInCircle{EllipseExpression{
                     "Ellipse2d(Translation2d(), 2 m)",
                     wpi::math::Ellipse2d{wpi::math::Translation2d{}, 1.0}}},
                 true});
  candidateTrajectory.snapshot = candidateTrajectory.params;
  candidate.trajectories =
      candidate.trajectories.set("candidate", candidateTrajectory);
  const auto evaluatedDocument = evaluation::EvaluateDocument(candidate);
  const auto& evaluatedTrajectory =
      evaluatedDocument.trajectories.at("candidate");
  if (evaluatedTrajectory.params.waypoints[0].pose.value.X().value() != 2.0 ||
      std::get<KeepInCircle>(evaluatedTrajectory.params.constraints[0].data)
              .circle.value.XSemiAxis()
              .value() != 2.0 ||
      evaluatedTrajectory.snapshot->waypoints[0].pose.value.X().value() !=
          0.0 ||
      candidate.trajectories.at("candidate")
              .params.waypoints[0]
              .pose.value.X()
              .value() != 0.0) {
    return EXIT_FAILURE;
  }
  auto invalidCircle = candidateTrajectory;
  std::get<KeepInCircle>(invalidCircle.params.constraints[0].data)
      .circle.source = "Ellipse2d(Translation2d(), -1.0)";
  auto invalidCircleDocument = candidate;
  invalidCircleDocument.trajectories =
      invalidCircleDocument.trajectories.set("candidate", invalidCircle);
  try {
    evaluation::EvaluateDocument(invalidCircleDocument);
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  std::get<KeepInCircle>(invalidCircle.params.constraints[0].data)
      .circle.source = "Ellipse2d(Translation2d(), 0.0)";
  invalidCircleDocument.trajectories =
      invalidCircleDocument.trajectories.set("candidate", invalidCircle);
  try {
    evaluation::EvaluateDocument(invalidCircleDocument);
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  std::get<KeepInCircle>(invalidCircle.params.constraints[0].data)
      .circle.source = "Ellipse2d(Pose2d(), 1 m, 2 m)";
  invalidCircleDocument.trajectories =
      invalidCircleDocument.trajectories.set("candidate", invalidCircle);
  try {
    evaluation::EvaluateDocument(invalidCircleDocument);
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  DocumentStore store{candidate};
  DocumentStore notificationStore{candidate};
  std::vector<std::uint64_t> observedRevisions;
  std::vector<std::uint64_t> otherRevisions;
  const auto listenerId = notificationStore.Subscribe(
      [&](std::shared_ptr<const Document> snapshot, std::uint64_t revision) {
        observedRevisions.push_back(revision);
        if (revision == 1) {
          auto nested = *snapshot;
          nested.project.name = "nested";
          notificationStore.Commit(std::move(nested));
        }
      });
  notificationStore.Subscribe(
      [](std::shared_ptr<const Document>, std::uint64_t) {
        throw std::runtime_error("listener error");
      });
  notificationStore.Subscribe(
      [&](std::shared_ptr<const Document>, std::uint64_t revision) {
        otherRevisions.push_back(revision);
      });
  auto notified = *notificationStore.Snapshot();
  notified.project.name = "first";
  notificationStore.Commit(notified);
  if (observedRevisions != std::vector<std::uint64_t>{1, 2} ||
      otherRevisions != std::vector<std::uint64_t>{1, 2} ||
      notificationStore.TakeNotificationErrors().size() != 2 ||
      notificationStore.Snapshot()->project.name != "nested") {
    return EXIT_FAILURE;
  }
  notificationStore.Unsubscribe(listenerId);
  if (!notificationStore.Undo() || observedRevisions.size() != 2 ||
      otherRevisions != std::vector<std::uint64_t>{1, 2, 3} ||
      notificationStore.TakeNotificationErrors().size() != 1) {
    return EXIT_FAILURE;
  }
  auto namespaced = candidate;
  namespaced.project.variables.at("length").namespacePath = "field";
  namespaced.project.variables.at("twice").expression = "field.length * 2";
  namespaced.project.variables.emplace(
      "occupied", Variable{"occupied", "field", "1 m", VariableType::Quantity,
                           std::map<std::string, int>{{"meter", 1}}, 0.0});
  namespaced.project.config.wheels.differential->trackwidth.source =
      "field.length / 4";
  auto namespacedTrajectory = namespaced.trajectories.at("candidate");
  namespacedTrajectory.params.waypoints[0].pose.source =
      "Pose2d(field.length + 0 m, 0 m, 0 rad)";
  namespaced.trajectories =
      namespaced.trajectories.set("candidate", namespacedTrajectory);
  auto secondTrajectory = namespacedTrajectory;
  secondTrajectory.uuid = "second";
  namespaced.trajectories =
      namespaced.trajectories.set("second", secondTrajectory);
  DocumentStore renameStore{namespaced};
  const auto beforeRename = renameStore.Snapshot();
  renameStore.RenameVariable("length", "width");
  const auto renamed = renameStore.Snapshot();
  if (renamed->project.variables.at("length").name != "width" ||
      renamed->project.variables.at("twice").expression != "field.width * 2" ||
      renamed->project.config.wheels.differential->trackwidth.source !=
          "field.width / 4" ||
      renamed->trajectories.at("second").params.waypoints[0].pose.source !=
          "Pose2d(field.width + 0 m, 0 m, 0 rad)" ||
      renamed->trajectories.at("candidate")
              .snapshot->waypoints[0]
              .pose.source != beforeRename->trajectories.at("candidate")
                                  .snapshot->waypoints[0]
                                  .pose.source ||
      !renameStore.Undo() ||
      renameStore.Snapshot()->project.variables.at("length").name != "length" ||
      !renameStore.Redo()) {
    return EXIT_FAILURE;
  }
  const auto beforeCollision = renameStore.Snapshot();
  const auto beforeCollisionRevision = renameStore.Revision();
  try {
    renameStore.RenameVariable("length", "occupied");
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  if (renameStore.Snapshot() != beforeCollision ||
      renameStore.Revision() != beforeCollisionRevision) {
    return EXIT_FAILURE;
  }
  const auto oldSnapshot = store.Snapshot();
  auto invalidReference = *oldSnapshot;
  auto invalidTrajectory = invalidReference.trajectories.at("candidate");
  invalidTrajectory.params.constraints[0].from = std::string{"missing"};
  invalidReference.trajectories =
      invalidReference.trajectories.set("candidate", invalidTrajectory);
  try {
    store.Commit(invalidReference);
    return EXIT_FAILURE;
  } catch (const evaluation::EvaluationError& error) {
    if (error.ObjectId() != "circle" || error.Field() != "from") {
      return EXIT_FAILURE;
    }
  }
  if (store.Snapshot() != oldSnapshot || store.Revision() != 0) {
    return EXIT_FAILURE;
  }
  store.Commit(*oldSnapshot);
  if (store.Revision() != 0 || store.CanUndo()) {
    return EXIT_FAILURE;
  }
  auto changed = *oldSnapshot;
  changed.project.variables.at("length").expression = "3 m";
  store.Commit(changed, store.Revision());
  if (store.Snapshot()
              ->trajectories.at("candidate")
              .params.waypoints[0]
              .pose.value.X()
              .value() != 3.0 ||
      oldSnapshot->trajectories.at("candidate")
              .params.waypoints[0]
              .pose.value.X()
              .value() != 2.0 ||
      !store.Undo() || !store.Redo()) {
    return EXIT_FAILURE;
  }
  const auto serialized = store.Serialize();
  const auto revisionBeforeReload = store.Revision();
  store.Reload(serialized);
  if (store.Revision() <= revisionBeforeReload || store.CanUndo() ||
      store.Snapshot()
              ->trajectories.at("candidate")
              .params.waypoints[0]
              .pose.value.X()
              .value() != 3.0) {
    return EXIT_FAILURE;
  }
  try {
    store.Commit(changed, revisionBeforeReload);
    return EXIT_FAILURE;
  } catch (const std::invalid_argument&) {
  }
  auto invalidSerialized = serialized;
  invalidSerialized.project = "{}";
  const auto beforeFailedReload = store.Snapshot();
  try {
    store.Reload(invalidSerialized);
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  if (store.Snapshot() != beforeFailedReload) {
    return EXIT_FAILURE;
  }
  const std::filesystem::path testDirectory{argv[1]};
  DocumentFiles files{testDirectory / "document-test.chor",
                      {{"candidate", testDirectory / "document-test.traj"}}};
  auto duplicateFiles = files;
  duplicateFiles.trajectories["candidate"] = files.project;
  try {
    store.SaveFiles(duplicateFiles);
    return EXIT_FAILURE;
  } catch (const std::invalid_argument&) {
  }
#if defined(_WIN32)
  duplicateFiles.project = testDirectory / "document-case.chor";
  duplicateFiles.trajectories["candidate"] =
      testDirectory / "DOCUMENT-CASE.CHOR";
  try {
    store.SaveFiles(duplicateFiles);
    return EXIT_FAILURE;
  } catch (const std::invalid_argument&) {
  }
#endif
  if (!store.SaveFiles(files).Success()) {
    return EXIT_FAILURE;
  }
  if (store.IsProjectDirty() || store.IsTrajectoryDirty("candidate")) {
    std::cerr << "dirty immediately after save\n";
    return EXIT_FAILURE;
  }
  auto unsaved = *store.Snapshot();
  unsaved.project.variables.at("length").expression = "4 m";
  store.Commit(unsaved);
  if (!store.IsProjectDirty() || !store.IsTrajectoryDirty("candidate") ||
      !store.Undo() || store.IsProjectDirty() ||
      store.IsTrajectoryDirty("candidate")) {
    std::cerr << "dirty tracking failed across edit and undo\n";
    return EXIT_FAILURE;
  }
  store.ReloadFiles(files);
  if (store.Snapshot()
          ->trajectories.at("candidate")
          .params.waypoints[0]
          .pose.value.X()
          .value() != 3.0) {
    return EXIT_FAILURE;
  }
  auto brokenFiles = files;
  brokenFiles.trajectories["candidate"] =
      testDirectory / "missing-directory" / "document-test.traj";
  const auto partialSave = store.SaveFiles(brokenFiles);
  if (partialSave.projectError ||
      !partialSave.trajectoryErrors.contains("candidate")) {
    return EXIT_FAILURE;
  }
  if (store.IsProjectDirty() || !store.IsTrajectoryDirty("candidate")) {
    std::cerr << "incorrect dirty state after partial save\n";
    return EXIT_FAILURE;
  }
  const auto beforeFailedFileReload = store.Snapshot();
  try {
    store.ReloadFiles(brokenFiles);
    return EXIT_FAILURE;
  } catch (const std::exception&) {
  }
  if (store.Snapshot() != beforeFailedFileReload) {
    return EXIT_FAILURE;
  }
  std::filesystem::remove(files.project);
  std::filesystem::remove(files.trajectories.at("candidate"));
  candidateTrajectory.params.waypoints[0].pose.source =
      "Pose2d(length, 0 s, 0 rad)";
  candidate.trajectories =
      candidate.trajectories.set("candidate", candidateTrajectory);
  try {
    evaluation::EvaluateDocument(candidate);
    return EXIT_FAILURE;
  } catch (const evaluation::EvaluationError& error) {
    if (error.ObjectId() != "point" || error.Field() != "pose") {
      return EXIT_FAILURE;
    }
  }
  if (candidate.project.config.mass.value.value() != 0.0) {
    return EXIT_FAILURE;
  }
  Trajectory trajectory;
  trajectory.uuid = "trajectory";
  trajectory.name = "Path";
  trajectory.params.waypoints = std::vector<Waypoint>{waypoint};
  Document original;
  original.trajectories =
      original.trajectories.set(trajectory.uuid, trajectory);

  trajectory.name = "Updated";
  Document updated = original;
  updated.trajectories = original.trajectories.set(trajectory.uuid, trajectory);
  if (original.trajectories.at("trajectory").name != "Path" ||
      updated.trajectories.at("trajectory").name != "Updated" ||
      original.trajectories.at("trajectory").params.waypoints[0].pose.source !=
          "Pose2d()") {
    return EXIT_FAILURE;
  }

  TrajectoryOutput emptyDifferential{
      {}, {}, std::optional<wpi::math::DifferentialTrajectory>{}};
  trajectory.output = emptyDifferential;
  updated.trajectories = updated.trajectories.set(trajectory.uuid, trajectory);
  const auto& output = updated.trajectories.at("trajectory").output;
  if (original.trajectories.at("trajectory").output || !output ||
      !std::holds_alternative<std::optional<wpi::math::DifferentialTrajectory>>(
          output->trajectory) ||
      std::get<std::optional<wpi::math::DifferentialTrajectory>>(
          output->trajectory)) {
    return EXIT_FAILURE;
  }

  SnapshotHistory history{original, 1};
  auto initial = history.Snapshot();
  history.Commit(updated);
  if (!history.CanUndo() || history.Revision() != 1 || !history.Undo() ||
      history.Snapshot()->trajectories.at("trajectory").name != "Path" ||
      !history.Redo() ||
      history.Snapshot()->trajectories.at("trajectory").name != "Updated") {
    return EXIT_FAILURE;
  }
  Document next = *history.Snapshot();
  next.project.name = "New project";
  history.Commit(next);
  if (!history.Undo() || history.Undo() || !history.CanRedo() ||
      initial->trajectories.at("trajectory").name != "Path") {
    return EXIT_FAILURE;
  }
  history.Commit(original);
  if (history.CanRedo() || history.Redo()) {
    return EXIT_FAILURE;
  }

  SnapshotHistory noHistory{original, 0};
  noHistory.Commit(updated);
  if (noHistory.CanUndo() || noHistory.Undo()) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
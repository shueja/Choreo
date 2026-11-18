#include "choreo/document/generation_process.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "document_test_fixture.hpp"

int FakeGenerator(int argc, char** argv) {
  if (argc != 7 || std::string{argv[1]} != "--chor" ||
      std::string{argv[3]} != "--trajectory" ||
      std::string{argv[5]} != "--output") {
    return EXIT_FAILURE;
  }
  auto read = [](const char* path) {
    std::ifstream input{std::filesystem::u8path(path), std::ios::binary};
    return std::string{std::istreambuf_iterator<char>{input},
                       std::istreambuf_iterator<char>{}};
  };
  auto project = wpi::util::json::parse_or_throw(read(argv[2]))
                     .get<choreo::document::Project>();
  auto trajectory = wpi::util::json::parse_or_throw(read(argv[4]))
                        .get<choreo::document::Trajectory>();
  if (trajectory.name == "fail-child") {
    return EXIT_FAILURE;
  }
  std::ofstream output{std::filesystem::u8path(argv[6]), std::ios::binary};
  if (trajectory.name == "invalid-child") {
    output << "not json";
  } else {
    trajectory.config = project.config;
    trajectory.snapshot = trajectory.params;
    trajectory.output = choreo::document::TrajectoryOutput{};
    trajectory.output->trajectory =
        std::optional<wpi::math::HolonomicTrajectory>{
            wpi::math::HolonomicTrajectory{
                std::vector<wpi::math::HolonomicSample>{
                    wpi::math::HolonomicSample{}}}};
    if (trajectory.name == "tampered-child") {
      trajectory.name = "changed by child";
    }
    if (trajectory.name == "changed-params") {
      trajectory.params.targetDt.source = "0.03 s";
    }
    if (trajectory.name == "changed-config") {
      trajectory.config->mass.source = "10 kg";
    }
    if (trajectory.name == "changed-snapshot") {
      trajectory.snapshot->targetDt.source = "0.03 s";
    }
    if (trajectory.name == "wrong-id") {
      trajectory.uuid = "wrong";
    }
    if (trajectory.name == "missing-output") {
      trajectory.output.reset();
    }
    if (trajectory.name == "wrong-drive-child") {
      trajectory.output->trajectory =
          std::optional<wpi::math::DifferentialTrajectory>{
              wpi::math::DifferentialTrajectory{
                  std::vector<wpi::math::DifferentialSample>{
                      wpi::math::DifferentialSample{}}}};
    }
    output << wpi::util::json{trajectory}.to_string();
  }
  output.close();
  return output ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char** argv) {
  if (argc > 1 && std::string{argv[1]} == "--chor") {
    return FakeGenerator(argc, argv);
  }
  if (argc != 2) {
    return EXIT_FAILURE;
  }
  using namespace choreo::document;
  Document candidate = MakeTestDocument(MakeTestProject());
  if (std::string{argv[1]} == "--handoff") {
    DocumentStore processStore{candidate};
    std::size_t processNotifications = 0;
    processStore.Subscribe([&](std::shared_ptr<const Document>, std::uint64_t) {
      ++processNotifications;
    });
    GenerationProcess process{std::filesystem::u8path(argv[0]), processStore,
                              "candidate"};
    try {
      process.CommitResult(processStore);
      return EXIT_FAILURE;
    } catch (const std::runtime_error&) {
    }
    process.Wait();
    if (!process.Poll() || processStore.Revision() != 0) {
      return EXIT_FAILURE;
    }
    process.CommitResult(processStore);
    if (processStore.Revision() != 1 || processNotifications != 1 ||
        !processStore.Snapshot()->trajectories.at("candidate").output ||
        !processStore.Undo() ||
        processStore.Snapshot()->trajectories.at("candidate").output ||
        !processStore.Redo()) {
      return EXIT_FAILURE;
    }
    try {
      process.CommitResult(processStore);
      return EXIT_FAILURE;
    } catch (const std::runtime_error&) {
    }
    GenerationProcess staleProcess{std::filesystem::u8path(argv[0]),
                                   processStore, "candidate"};
    auto changedDuringGeneration = *processStore.Snapshot();
    changedDuringGeneration.project.name = "changed";
    processStore.Commit(std::move(changedDuringGeneration));
    staleProcess.Wait();
    const auto beforeStaleProcess = processStore.Snapshot();
    try {
      staleProcess.CommitResult(processStore);
      return EXIT_FAILURE;
    } catch (const std::invalid_argument&) {
    }
    if (processStore.Snapshot() != beforeStaleProcess) {
      return EXIT_FAILURE;
    }
    DocumentStore otherStore{candidate};
    GenerationProcess wrongOwner{std::filesystem::u8path(argv[0]), processStore,
                                 "candidate"};
    wrongOwner.Wait();
    try {
      wrongOwner.CommitResult(otherStore);
      return EXIT_FAILURE;
    } catch (const std::invalid_argument&) {
    }
    if (otherStore.Revision() != 0) {
      return EXIT_FAILURE;
    }
    for (const auto* failure :
         {"fail-child", "invalid-child", "tampered-child", "wrong-drive-child",
          "changed-params", "changed-config", "changed-snapshot", "wrong-id",
          "missing-output"}) {
      auto failureDocument = candidate;
      auto failureTrajectory = failureDocument.trajectories.at("candidate");
      failureTrajectory.name = failure;
      failureDocument.trajectories =
          failureDocument.trajectories.set("candidate", failureTrajectory);
      DocumentStore failureStore{failureDocument};
      GenerationProcess failureProcess{std::filesystem::u8path(argv[0]),
                                       failureStore, "candidate"};
      failureProcess.Wait();
      try {
        failureProcess.CommitResult(failureStore);
        return EXIT_FAILURE;
      } catch (const std::exception&) {
      }
      if (failureStore.Revision() != 0) {
        return EXIT_FAILURE;
      }
    }
    GenerationProcess cancelledProcess{std::filesystem::u8path(argv[0]),
                                       processStore, "candidate"};
    cancelledProcess.Cancel();
    cancelledProcess.Wait();
    try {
      cancelledProcess.CommitResult(processStore);
      return EXIT_FAILURE;
    } catch (const std::runtime_error&) {
    }
    return EXIT_SUCCESS;
  }
  const std::string mode{argv[1]};
  if (mode == "--real-differential" || mode == "--real-swerve" ||
      mode == "--real-split") {
    const char* realGenerator = std::getenv("CHOREO_GENERATOR_TEST_EXECUTABLE");
    if (!realGenerator || !*realGenerator) {
      std::cerr << "Real generator path not configured\n";
      return 77;
    }
    auto realDocument = candidate;
    realDocument.project.type = DriveType::Differential;
    auto realTrajectory = realDocument.trajectories.at("candidate");
    realTrajectory.params.constraints.clear();
    realTrajectory.params.waypoints[0].overrideIntervals = true;
    realTrajectory.params.waypoints[0].intervals = 8;
    auto endWaypoint = realTrajectory.params.waypoints[0];
    endWaypoint.uuid = "end";
    endWaypoint.pose.source = "Pose2d(length + 1 m, 0 m, 0 rad)";
    realTrajectory.params.waypoints.push_back(endWaypoint);
    realTrajectory.events.push_back(
        EventMarker{"event", "start",
                    EventMarkerData{WaypointBoundary::First,
                                    std::nullopt,
                                    {"0.1 s", wpi::units::second_t{0.1}}}});
    realTrajectory.events.push_back(
        EventMarker{"end-event", "end",
                    EventMarkerData{WaypointBoundary::Last,
                                    std::nullopt,
                                    {"0.2 s", wpi::units::second_t{0.2}}}});
    realDocument.trajectories =
        realDocument.trajectories.set("candidate", realTrajectory);
    if (mode == "--real-differential") {
      DocumentStore realStore{realDocument};
      const auto originalDocument = realStore.Snapshot();
      std::size_t realNotifications = 0;
      realStore.Subscribe([&](std::shared_ptr<const Document>, std::uint64_t) {
        ++realNotifications;
      });
      GenerationProcess realProcess{std::filesystem::u8path(realGenerator),
                                    realStore, "candidate"};
      realProcess.Wait();
      realProcess.CommitResult(realStore);
      const auto& result = realStore.Snapshot()->trajectories.at("candidate");
      const auto& original = originalDocument->trajectories.at("candidate");
      if (realStore.Revision() != 1 || realNotifications != 1 ||
          !result.output || !result.snapshot || !result.config ||
          result.name != original.name || result.version != original.version ||
          result.uuid != original.uuid ||
          wpi::util::json{result.params} != wpi::util::json{original.params} ||
          wpi::util::json{*result.snapshot} !=
              wpi::util::json{original.params} ||
          wpi::util::json{*result.config} !=
              wpi::util::json{originalDocument->project.config} ||
          wpi::util::json{realStore.Snapshot()->project} !=
              wpi::util::json{originalDocument->project} ||
          result.output->waypoints.size() != original.params.waypoints.size() ||
          std::abs(result.output->waypoints.front()) > 1e-9 ||
          result.output->splits != std::vector<std::uint32_t>{0} ||
          result.events.size() != 2 || !result.events[0].from.targetTime ||
          !result.events[1].from.targetTime ||
          std::abs(*result.events[0].from.targetTime -
                   result.output->waypoints.front()) > 1e-9 ||
          std::abs(*result.events[1].from.targetTime -
                   result.output->waypoints.back()) > 1e-9 ||
          result.events[0].from.offset.source != "0.1 s" ||
          result.events[1].from.offset.source != "0.2 s") {
        std::cerr << "real generator document invariants failed\n";
        return EXIT_FAILURE;
      }
      const auto* samples =
          std::get_if<std::optional<wpi::math::DifferentialTrajectory>>(
              &result.output->trajectory);
      if (!samples || !*samples || (*samples)->Samples().size() != 9 ||
          std::abs((*samples)->Samples().front().time.value() -
                   result.output->waypoints.front()) > 1e-9 ||
          std::abs((*samples)->Samples().back().time.value() -
                   result.output->waypoints.back()) > 1e-9 ||
          !realStore.Undo() || realNotifications != 2 ||
          wpi::util::json{realStore.Snapshot()->trajectories.at("candidate")} !=
              wpi::util::json{original}) {
        std::cerr << "real generator samples or undo failed\n";
        return EXIT_FAILURE;
      }
    }

    if (mode == "--real-swerve") {
      realDocument.project.type = DriveType::Swerve;
      realDocument.project.config.wheels.swerve = SwerveWheels{};
      auto& modules = realDocument.project.config.wheels.swerve->modules;
      modules[0].source = "Translation2d(0.25 m, 0.25 m)";
      modules[1].source = "Translation2d(0.25 m, -0.25 m)";
      modules[2].source = "Translation2d(-0.25 m, 0.25 m)";
      modules[3].source = "Translation2d(-0.25 m, -0.25 m)";
      DocumentStore swerveStore{realDocument};
      const auto originalSwerve = swerveStore.Snapshot();
      GenerationProcess swerveProcess{std::filesystem::u8path(realGenerator),
                                      swerveStore, "candidate"};
      swerveProcess.Wait();
      swerveProcess.CommitResult(swerveStore);
      const auto& swerve = swerveStore.Snapshot()->trajectories.at("candidate");
      if (swerveStore.Revision() != 1 || !swerve.output || !swerve.snapshot ||
          !swerve.config || swerve.output->waypoints.size() != 2 ||
          swerve.output->splits != std::vector<std::uint32_t>{0} ||
          wpi::util::json{swerve.params} !=
              wpi::util::json{
                  originalSwerve->trajectories.at("candidate").params} ||
          wpi::util::json{*swerve.snapshot} != wpi::util::json{swerve.params} ||
          wpi::util::json{*swerve.config} !=
              wpi::util::json{originalSwerve->project.config} ||
          swerve.events.size() != 2 || !swerve.events[0].from.targetTime ||
          !swerve.events[1].from.targetTime ||
          std::abs(*swerve.events[0].from.targetTime -
                   swerve.output->waypoints.front()) > 1e-9 ||
          std::abs(*swerve.events[1].from.targetTime -
                   swerve.output->waypoints.back()) > 1e-9) {
        std::cerr << "swerve generator document invariants failed\n";
        return EXIT_FAILURE;
      }
      const auto* swerveSamples =
          std::get_if<std::optional<wpi::math::HolonomicTrajectory>>(
              &swerve.output->trajectory);
      if (!swerveSamples || !*swerveSamples ||
          (*swerveSamples)->Samples().size() != 9 || !swerveStore.Undo() ||
          wpi::util::json{
              swerveStore.Snapshot()->trajectories.at("candidate")} !=
              wpi::util::json{originalSwerve->trajectories.at("candidate")} ||
          !swerveStore.Redo() ||
          !swerveStore.Snapshot()->trajectories.at("candidate").output) {
        std::cerr << "swerve generator samples or history failed\n";
        return EXIT_FAILURE;
      }
    }

    if (mode == "--real-split") {
      realDocument.project.type = DriveType::Differential;
      auto splitTrajectory = realDocument.trajectories.at("candidate");
      auto middle = splitTrajectory.params.waypoints.front();
      middle.uuid = "middle";
      middle.pose.source = "Pose2d(length + 0.5 m, 0 m, 0 rad)";
      middle.split = true;
      splitTrajectory.params.waypoints.insert(
          splitTrajectory.params.waypoints.begin() + 1, middle);
      splitTrajectory.events.push_back(
          EventMarker{"middle-event", "middle",
                      EventMarkerData{std::string{"middle"},
                                      std::nullopt,
                                      {"0.3 s", wpi::units::second_t{0.3}}}});
      realDocument.trajectories =
          realDocument.trajectories.set("candidate", splitTrajectory);
      DocumentStore splitStore{realDocument};
      const auto originalSplit = splitStore.Snapshot();
      GenerationProcess splitProcess{std::filesystem::u8path(realGenerator),
                                     splitStore, "candidate"};
      splitProcess.Wait();
      splitProcess.CommitResult(splitStore);
      const auto& splitResult =
          splitStore.Snapshot()->trajectories.at("candidate");
      if (!splitResult.output || !splitResult.snapshot ||
          splitResult.output->waypoints.size() != 3 ||
          splitResult.output->splits != std::vector<std::uint32_t>{0, 8} ||
          splitResult.events.size() != 3 ||
          !splitResult.events[2].from.targetTime ||
          std::abs(*splitResult.events[2].from.targetTime -
                   splitResult.output->waypoints[1]) > 1e-9 ||
          wpi::util::json{splitResult.params} !=
              wpi::util::json{
                  originalSplit->trajectories.at("candidate").params}) {
        std::cerr << "intermediate split or UUID event invariant failed\n";
        return EXIT_FAILURE;
      }
      const auto* splitSamples =
          std::get_if<std::optional<wpi::math::DifferentialTrajectory>>(
              &splitResult.output->trajectory);
      if (!splitSamples || !*splitSamples ||
          (*splitSamples)->Samples().size() != 17 ||
          std::abs((*splitSamples)->Samples()[8].time.value() -
                   splitResult.output->waypoints[1]) > 1e-9 ||
          !splitStore.Undo() ||
          wpi::util::json{
              splitStore.Snapshot()->trajectories.at("candidate")} !=
              wpi::util::json{originalSplit->trajectories.at("candidate")}) {
        std::cerr << "intermediate split samples or undo failed\n";
        return EXIT_FAILURE;
      }
    }
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}

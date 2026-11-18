#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <vector>

#include <wpi/util/json.hpp>

#include "choreo/Choreo.hpp"

TEST_CASE("Choreo loads v4 WPILib samples") {
  wpi::math::HolonomicSample holonomic;
  auto root = wpi::util::json::object(
      "uuid", "00000000-0000-4000-8000-000000000101", "version", 4, "name", "Path", "config", nullptr, "snapshot", nullptr,
      "params", wpi::util::json::object("waypoints", wpi::util::json::array(),
          "constraints", wpi::util::json::array(),
          "targetDt", wpi::util::json::object("exp", "0.02 s", "val", 0.02)),
      "output", wpi::util::json::object(
          "sampleType", "Swerve", "waypoints", std::vector<double>{0.0},
          "splits", std::vector<int>{0}, "trajectory",
          wpi::util::json::object("samples", std::vector{holonomic})),
      "events", std::vector<wpi::util::json>{});

  auto result = choreo::Choreo::LoadTrajectoryString(root.to_string());
  const auto& swerve = std::get<std::optional<wpi::math::HolonomicTrajectory>>(
      result.output.trajectory);
  REQUIRE(swerve);
  REQUIRE(swerve->Samples().size() == 1);
  CHECK(swerve->Samples()[0] == holonomic);

  wpi::math::DifferentialSample differential;
  root["output"]["sampleType"] = "Differential";
  root["output"]["trajectory"]["samples"] = std::vector{differential};
  result = choreo::Choreo::LoadTrajectoryString(root.to_string());
  const auto& tank = std::get<std::optional<wpi::math::DifferentialTrajectory>>(
      result.output.trajectory);
  REQUIRE(tank);
  REQUIRE(tank->Samples().size() == 1);
  CHECK(tank->Samples()[0] == differential);

  root["version"] = 3;
  CHECK_THROWS_AS(choreo::Choreo::LoadTrajectoryString(root.to_string()),
                  std::invalid_argument);
}
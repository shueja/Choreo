// Copyright (c) Choreo contributors

#pragma once
#include <cmath>
#include <numbers>
#include <optional>
#include <print>
#include <vector>

#include <choreo/document/constraint.hpp>
#include <choreo/document/robot_config.hpp>
#include <choreo/document/waypoint.hpp>
#include <sleipnir/optimization/solver/exit_status.hpp>
#include <trajopt/swerve_trajectory_generator.hpp>
#include <wpi/math/geometry/Pose2d.hpp>
namespace choreo {

struct Segment {
  document::Waypoint start;

  std::optional<wpi::units::second_t> estimated_time = std::nullopt;
  std::vector<document::ConstraintData> segment_constraints;
  std::vector<document::ConstraintData> waypoint_constraints;
  bool coalesce_with_previous =
      false;  // Whether this segment should be merged with the previous one in
              // the optimization problem, by treating the first following point
              // as the start of the segment instead of the constrained
              // waypoint. This is used for waypoints that are marked as initial
              // guesses, since they aren't actually constrained and can be
              // treated as part of the previous segment for optimization
              // purposes.
  wpi::units::second_t estimate_segment_time(
      const Segment& next, const document::RobotConfig& config) const;
  /// Returns the number of intervals that should be used for this segment in
  /// the optimization problem, based on the estimated time and the target_dt,
  /// or the override_intervals if that is set. If neither is available, returns
  /// nullopt, which indicates that the optimizer should use a default value. If
  /// the override_intervals is set but is less than the number of intervals
  /// that would be estimated based on the estimated time and target_dt, a
  /// warning is printed since this may lead to a suboptimal trajectory. If the
  /// estimated time is very large compared to the target_dt, a warning is also
  /// printed since this may indicate an issue with the time estimation and
  /// could lead to a very long optimization time or an inability to find a
  /// solution.
  std::optional<size_t> interval_count(wpi::units::second_t target_dt) const {
    if (!estimated_time.has_value()) {
      return std::nullopt;
    }
    return std::optional<size_t>(static_cast<size_t>(std::ceil(
        static_cast<double>((estimated_time.value() / target_dt).value()))));
  }

  std::optional<size_t> interval_count(wpi::units::second_t target_dt,
                                       const Segment& next,
                                       const document::RobotConfig& config) const {
    auto estimated_time = estimate_segment_time(next, config);
    if (start.overrideIntervals) {
      return std::optional<size_t>(start.intervals);

    } else {
      if (estimated_time >
          target_dt * 1000) {  // Arbitrary threshold of 1000 intervals, which
                               // is likely more than enough for any reasonable
                               // trajectory and indicates that something may
                               // have gone wrong with the time estimation.
        std::println(
            "Warning: Waypoint at ({}, {}) has estimated_time of {}, which "
            "would require {} intervals at target_dt of {}. This may indicate "
            "that the time estimation is inaccurate or that the constraints on "
            "this segment are very difficult to satisfy, and could lead to a "
            "very long optimization time or an inability to find a solution.",
            start.pose.value.X().value(), start.pose.value.Y().value(),
            estimated_time.value(),
            static_cast<size_t>(std::ceil(
                static_cast<double>(estimated_time.value() / target_dt))),
            target_dt.value());
      }
      return std::optional<size_t>(static_cast<size_t>(
          std::ceil((estimated_time.value() / target_dt).value())));
    }
    return std::optional<size_t>(static_cast<size_t>(
        std::ceil(static_cast<double>(estimated_time.value() / target_dt))));
  }

  void update_start_intervals(wpi::units::second_t target_dt,
                              const Segment& next,
                              const document::RobotConfig& config) {
    auto count = interval_count(target_dt, next, config);
    if (count.has_value()) {
      start.intervals = count.value();
    }
  }
};
}  // namespace choreo

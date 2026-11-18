// Copyright (c) TrajoptLib contributors

#pragma once

#include <cassert>
#include <cmath>
#include <numbers>

#include <sleipnir/autodiff/variable.hpp>
#include <sleipnir/optimization/problem.hpp>

#include "trajopt/geometry/pose2.hpp"
#include "trajopt/geometry/translation2.hpp"
#include "trajopt/util/symbol_exports.hpp"

namespace trajopt {

class TRAJOPT_DLLEXPORT HeadingAngleConstraint {
 public:
  explicit HeadingAngleConstraint(double heading, double tolerance)
      : m_heading{heading}, m_tolerance{tolerance} {
    assert(tolerance >= 0.0);
  }

  void apply(
      slp::Problem<double>& problem, const Pose2v<double>& pose,
      [[maybe_unused]] const Translation2v<double>& linear_velocity,
      [[maybe_unused]] const slp::Variable<double>& angular_velocity,
      [[maybe_unused]] const Translation2v<double>& linear_acceleration,
      [[maybe_unused]] const slp::Variable<double>& angular_acceleration) {
    if (m_tolerance >= std::numbers::pi) {
      return;
    }
    const double target_cos = std::cos(m_heading);
    const double target_sin = std::sin(m_heading);
    if (m_tolerance == 0.0) {
      problem.subject_to(pose.rotation().cos() == target_cos);
      problem.subject_to(pose.rotation().sin() == target_sin);
    } else {
      problem.subject_to(pose.rotation().cos() * target_cos +
                             pose.rotation().sin() * target_sin >=
                         std::cos(m_tolerance));
    }
  }

 private:
  double m_heading;
  double m_tolerance;
};

}  // namespace trajopt
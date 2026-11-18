#pragma once

#include <optional>
#include <string>
#include <variant>

#include "choreo/document/heading_constraint.hpp"
#include "choreo/document/keep_in_circle.hpp"
#include "choreo/document/max_angular_velocity.hpp"
#include "choreo/document/max_linear_velocity.hpp"
#include "choreo/document/waypoint_id.hpp"

namespace choreo::document {

using ConstraintData = std::variant<MaxLinearVelocity, MaxAngularVelocity,
                                    KeepInCircle, HeadingConstraint>;

class Constraint {
 public:
  std::string uuid;
  WaypointId from;
  std::optional<WaypointId> to;
  ConstraintData data;
  bool enabled = true;
};

void to_json(wpi::util::json& json, const Constraint& value);
void from_json(const wpi::util::json& json, Constraint& value);

}
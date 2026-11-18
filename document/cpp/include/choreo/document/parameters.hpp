#pragma once

#include <vector>

#include "choreo/document/constraint.hpp"
#include "choreo/document/waypoint.hpp"

namespace choreo::document {

class Parameters {
 public:
  std::vector<Waypoint> waypoints;
  std::vector<Constraint> constraints;
  Expression<dimension::Time> targetDt;
};

void to_json(wpi::util::json& json, const Parameters& value);
void from_json(const wpi::util::json& json, Parameters& value);

}
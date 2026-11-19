#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "choreo/document/expression.hpp"

namespace choreo::document {

class Waypoint {
 public:
  std::string uuid;
  PoseExpression pose;
  std::uint32_t intervals = 0;
  bool split = false;
  bool fixTranslation = false;
  bool fixHeading = false;
  bool overrideIntervals = false;
  std::map<std::string, bool> allowDrag{
      {"x", true}, {"y", true}, {"heading", true}};

  wpi::util::json ToJson() const;
  static Waypoint FromJson(const wpi::util::json& json);
};

void to_json(wpi::util::json& json, const Waypoint& waypoint);
void from_json(const wpi::util::json& json, Waypoint& waypoint);

}
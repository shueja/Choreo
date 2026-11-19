#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "choreo/document/codegen_config.hpp"
#include "choreo/document/project_schema_version.hpp"
#include "choreo/document/robot_config.hpp"
#include "choreo/document/variable.hpp"

namespace choreo::document {

enum class DriveType { Swerve, Differential };

class Project {
 public:
  std::string uuid;
  std::string name;
  std::uint32_t version = kProjectSchemaVersion;
  DriveType type = DriveType::Swerve;
  std::map<std::string, Variable> variables;
  std::vector<std::string> variableOrder;
  RobotConfig config;
  CodeGenConfig codegen;
};

void to_json(wpi::util::json& json, const Project& project);
void from_json(const wpi::util::json& json, Project& project);

}
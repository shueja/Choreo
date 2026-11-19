#include "choreo/document/project.hpp"

#include <set>
#include <stdexcept>
#include <utility>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

using wpi::util::json;

void to_json(json& serialized, const Project& project) {
  detail::CheckUuid(project.uuid);
  if (project.version != kProjectSchemaVersion ||
      (project.type != DriveType::Swerve &&
       project.type != DriveType::Differential)) {
    throw std::invalid_argument("invalid project schema version or drive type");
  }
  json variables = json::object();
  std::vector<std::string> order = project.variableOrder;
  if (order.empty()) {
    for (const auto& [id, unused] : project.variables) {
      order.push_back(id);
    }
  }
  std::set<std::string> orderedIds;
  if (order.size() != project.variables.size()) {
    throw std::invalid_argument("variable order does not match variables");
  }
  for (const auto& id : order) {
    if (!project.variables.contains(id) || !orderedIds.insert(id).second) {
      throw std::invalid_argument(
          "variable order has an unknown or duplicate ID");
    }
  }
  for (const auto& [id, variable] : project.variables) {
    detail::CheckUuid(id);
    variables[id] = variable;
  }
  serialized = json::object(
      "uuid", project.uuid, "name", project.name, "version", project.version,
      "type", project.type == DriveType::Swerve ? "Swerve" : "Differential",
      "variables",
      json::object("variables", std::move(variables), "order",
                   std::move(order)),
      "config", project.config, "codegen", project.codegen);
}

void from_json(const json& serialized, Project& project) {
  detail::CheckProperties(serialized, {"uuid", "name", "version", "type",
                                       "variables", "config", "codegen"});
  detail::CheckProperties(serialized.at("variables"), {"variables", "order"});
  if (!serialized.at("version").is_int() ||
      serialized.at("version").get_int() != kProjectSchemaVersion) {
    throw std::invalid_argument("unsupported project schema version");
  }
  Project result;
  result.uuid = serialized.at("uuid").get_string();
  detail::CheckUuid(result.uuid);
  result.name = serialized.at("name").get_string();
  const auto& type = serialized.at("type").get_string();
  if (type == "Swerve") {
    result.type = DriveType::Swerve;
  } else if (type == "Differential") {
    result.type = DriveType::Differential;
  } else {
    throw std::invalid_argument("unsupported project drive type");
  }
  for (const auto& [id, variable] :
       serialized.at("variables").at("variables").get_object()) {
    detail::CheckUuid(id);
    result.variables.emplace(id, variable.get<Variable>());
  }
  if (serialized.at("variables").contains("order")) {
    result.variableOrder =
        serialized.at("variables").at("order").get<std::vector<std::string>>();
    std::set<std::string> orderedIds;
    if (result.variableOrder.size() != result.variables.size()) {
      throw std::invalid_argument("variable order does not match variables");
    }
    for (const auto& id : result.variableOrder) {
      detail::CheckUuid(id);
      if (!result.variables.contains(id) || !orderedIds.insert(id).second) {
        throw std::invalid_argument(
            "variable order has an unknown or duplicate ID");
      }
    }
  } else {
    for (const auto& [id, unused] : result.variables) {
      result.variableOrder.push_back(id);
    }
  }
  result.config = serialized.at("config").get<RobotConfig>();
  result.codegen = serialized.at("codegen").get<CodeGenConfig>();
  project = std::move(result);
}

}  // namespace choreo::document
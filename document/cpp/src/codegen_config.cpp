#include "choreo/document/codegen_config.hpp"

#include "choreo/document/json_validation.hpp"
namespace choreo::document {
wpi::util::json CodeGenConfig::ToJson() const {
  auto json = wpi::util::json::object();
  if (root) {
    json["root"] = *root;
  }
  if (genVars) {
    json["genVars"] = *genVars;
  }
  if (genTrajData) {
    json["genTrajData"] = *genTrajData;
  }
  if (useChoreoLib) {
    json["useChoreoLib"] = *useChoreoLib;
  }
  return json;
}
CodeGenConfig CodeGenConfig::FromJson(const wpi::util::json& json) {
  detail::CheckProperties(json,
                          {"root", "genVars", "genTrajData", "useChoreoLib"});
  CodeGenConfig result;
  if (json.contains("root")) {
    result.root = json.at("root").get_string();
  }
  if (json.contains("genVars")) {
    result.genVars = json.at("genVars").get_bool();
  }
  if (json.contains("genTrajData")) {
    result.genTrajData = json.at("genTrajData").get_bool();
  }
  if (json.contains("useChoreoLib")) {
    result.useChoreoLib = json.at("useChoreoLib").get_bool();
  }
  return result;
}
void to_json(wpi::util::json& json, const CodeGenConfig& value) {
  json = value.ToJson();
}
void from_json(const wpi::util::json& json, CodeGenConfig& value) {
  value = CodeGenConfig::FromJson(json);
}
}
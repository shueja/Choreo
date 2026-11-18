#pragma once
#include <optional>
#include <string>

#include <wpi/util/json.hpp>
namespace choreo::document {
class CodeGenConfig {
 public:
  std::optional<std::string> root;
  std::optional<bool> genVars;
  std::optional<bool> genTrajData;
  std::optional<bool> useChoreoLib;
  wpi::util::json ToJson() const;
  static CodeGenConfig FromJson(const wpi::util::json& json);
};
void to_json(wpi::util::json& json, const CodeGenConfig& value);
void from_json(const wpi::util::json& json, CodeGenConfig& value);
}
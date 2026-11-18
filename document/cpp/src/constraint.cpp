#include "choreo/document/constraint.hpp"

#include <stdexcept>
#include <utility>

#include "choreo/document/json_validation.hpp"

namespace choreo::document {

void to_json(wpi::util::json& json, const Constraint& value) {
  detail::CheckUuid(value.uuid);
  auto data = std::visit(
      [](const auto& variant) { return wpi::util::json{variant}; }, value.data);
  json = wpi::util::json::object("uuid", value.uuid, "from",
                                 WriteWaypointId(value.from), "data",
                                 std::move(data), "enabled", value.enabled);
  if (value.to) {
    json["to"] = WriteWaypointId(*value.to);
  }
}

void from_json(const wpi::util::json& json, Constraint& value) {
  detail::CheckProperties(json, {"uuid", "from", "to", "data", "enabled"});
  Constraint result;
  result.uuid = json.at("uuid").get_string();
  detail::CheckUuid(result.uuid);
  result.from = ReadWaypointId(json.at("from"));
  if (json.contains("to")) {
    result.to = ReadWaypointId(json.at("to"));
  }
  result.enabled = json.at("enabled").get_bool();
  const auto& data = json.at("data");
  const auto& type = data.at("type").get_string();
  if (type == "MaxLinearVelocity") {
    result.data = data.get<MaxLinearVelocity>();
  } else if (type == "MaxAngularVelocity") {
    result.data = data.get<MaxAngularVelocity>();
  } else if (type == "Heading") {
    result.data = data.get<HeadingConstraint>();
  } else if (type == "KeepInCircle") {
    result.data = data.get<KeepInCircle>();
  } else {
    throw std::invalid_argument("unsupported constraint type: " + type);
  }
  value = std::move(result);
}

}
#include <stdexcept>
#include <type_traits>

#include <wpilib-expression/expression.hpp>

#include "choreo/document/evaluation.hpp"

namespace choreo::document::evaluation {
namespace {

template <typename Value>
void Rewrite(Expression<Value>& expression, const std::string& oldName,
             const std::string& newName) {
  expression.source = wpilib::expression::RenameQualifiedVariableReferences(
      expression.source, oldName, newName);
}

void RewriteConfig(RobotConfig& config, const std::string& oldName,
                   const std::string& newName) {
  Rewrite(config.mass, oldName, newName);
  Rewrite(config.inertia, oldName, newName);
  Rewrite(config.gearing, oldName, newName);
  Rewrite(config.radius, oldName, newName);
  Rewrite(config.cof, oldName, newName);
  if (config.wheels.differential) {
    Rewrite(config.wheels.differential->trackwidth, oldName, newName);
  }
  if (config.wheels.swerve) {
    for (auto& module : config.wheels.swerve->modules) {
      Rewrite(module, oldName, newName);
    }
  }
  if (config.wheels.mecanum) {
    auto& wheels = *config.wheels.mecanum;
    Rewrite(wheels.frontLeft, oldName, newName);
    Rewrite(wheels.frontRight, oldName, newName);
    Rewrite(wheels.backLeft, oldName, newName);
    Rewrite(wheels.backRight, oldName, newName);
  }
  Rewrite(config.bumpers, oldName, newName);
  Rewrite(config.motor.freeSpeed, oldName, newName);
  Rewrite(config.motor.stallTorque, oldName, newName);
  Rewrite(config.motor.kT, oldName, newName);
  Rewrite(config.motor.kV, oldName, newName);
  Rewrite(config.motor.supplyLimit, oldName, newName);
  Rewrite(config.motor.statorLimit, oldName, newName);
}

void RewriteParameters(Parameters& parameters, const std::string& oldName,
                       const std::string& newName) {
  Rewrite(parameters.targetDt, oldName, newName);
  for (auto& waypoint : parameters.waypoints) {
    Rewrite(waypoint.pose, oldName, newName);
  }
  for (auto& constraint : parameters.constraints) {
    std::visit(
        [&](auto& data) {
          using Data = std::remove_cvref_t<decltype(data)>;
          if constexpr (std::is_same_v<Data, MaxLinearVelocity> ||
                        std::is_same_v<Data, MaxAngularVelocity>) {
            Rewrite(data.max, oldName, newName);
          } else if constexpr (std::is_same_v<Data, HeadingConstraint>) {
            Rewrite(data.heading, oldName, newName);
            Rewrite(data.tolerance, oldName, newName);
          } else if constexpr (std::is_same_v<Data, KeepInCircle>) {
            Rewrite(data.circle, oldName, newName);
          }
        },
        constraint.data);
  }
}

}

Document RenameVariable(const Document& document, const std::string& id,
                        const std::string& newName) {
  Document result = document;
  auto target = result.project.variables.find(id);
  if (target == result.project.variables.end()) {
    throw std::invalid_argument("unknown variable ID: " + id);
  }
  const auto oldName =
      target->second.namespacePath.empty()
          ? target->second.name
          : target->second.namespacePath + "." + target->second.name;
  const auto newQualifiedName =
      target->second.namespacePath.empty()
          ? newName
          : target->second.namespacePath + "." + newName;
  for (auto& [variableId, variable] : result.project.variables) {
    variable.expression = wpilib::expression::RenameQualifiedVariableReferences(
        variable.expression, oldName, newQualifiedName);
  }
  target->second.name = newName;
  RewriteConfig(result.project.config, oldName, newQualifiedName);
  for (const auto& [trajectoryId, original] : document.trajectories) {
    auto trajectory = original;
    RewriteParameters(trajectory.params, oldName, newQualifiedName);
    for (auto& event : trajectory.events) {
      Rewrite(event.from.offset, oldName, newQualifiedName);
    }
    result.trajectories =
        result.trajectories.set(trajectoryId, std::move(trajectory));
  }
  return result;
}

}
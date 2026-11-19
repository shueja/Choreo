#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <iterator>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <wpilib-expression/expression.hpp>

#include "choreo/document/evaluation.hpp"
#include "choreo/document/json_validation.hpp"

namespace choreo::document::evaluation {
namespace {

units::precise_unit RuntimeUnit(const std::map<std::string, int>& dimensions) {
  auto exponent = [&](const char* name) {
    auto found = dimensions.find(name);
    return found == dimensions.end() ? 0 : found->second;
  };
  const int meter = exponent("meter");
  const int kilogram = exponent("kilogram");
  const int second = exponent("second");
  const int ampere = exponent("ampere");
  const int kelvin = exponent("kelvin");
  const int mole = exponent("mole");
  const int candela = exponent("candela");
  const int currency = exponent("currency");
  const int count = exponent("count");
  const int radian = exponent("radian");
  units::precise_unit unit{
      units::detail::unit_data{meter, kilogram, second, ampere, kelvin, mole,
                               candela, currency, count, radian, 0, 0, 0, 0}};
  const auto base = unit.base_units();
  if (base.meter() != meter || base.kg() != kilogram ||
      base.second() != second || base.ampere() != ampere ||
      base.kelvin() != kelvin || base.mole() != mole ||
      base.candela() != candela || base.currency() != currency ||
      base.count() != count || base.radian() != radian) {
    throw std::invalid_argument(
        "variable dimension exponent is not representable");
  }
  for (const auto& [name, unused] : dimensions) {
    if (name != "meter" && name != "kilogram" && name != "second" &&
        name != "ampere" && name != "kelvin" && name != "mole" &&
        name != "candela" && name != "currency" && name != "count" &&
        name != "radian") {
      throw std::invalid_argument("unknown variable dimension: " + name);
    }
  }
  return unit;
}

wpilib::expression::VariableType RuntimeType(VariableType type) {
  switch (type) {
    case VariableType::Quantity:
      return wpilib::expression::VariableType::Quantity;
    case VariableType::Pose2d:
      return wpilib::expression::VariableType::Pose2d;
    case VariableType::Translation2d:
      return wpilib::expression::VariableType::Translation2d;
    case VariableType::Polygon2d:
      return wpilib::expression::VariableType::Polygon2d;
    case VariableType::Rotation2d:
      return wpilib::expression::VariableType::Rotation2d;
    case VariableType::Transform2d:
      return wpilib::expression::VariableType::Transform2d;
    case VariableType::Twist2d:
      return wpilib::expression::VariableType::Twist2d;
    case VariableType::Rectangle2d:
      return wpilib::expression::VariableType::Rectangle2d;
    case VariableType::Ellipse2d:
      return wpilib::expression::VariableType::Ellipse2d;
  }
  throw std::invalid_argument("unsupported variable type");
}

}

struct EvaluatedVariables {
  Project project;
  wpilib::expression::EvaluationContext context;
};

EvaluatedVariables BuildVariables(const Project& project) {
  std::vector<wpilib::expression::VariableDefinition> definitions;
  definitions.reserve(project.variables.size());
  for (const auto& [id, variable] : project.variables) {
    if ((variable.type == VariableType::Quantity) !=
        variable.dimension.has_value()) {
      throw std::invalid_argument("variable dimension does not match type: " +
                                  id);
    }
    definitions.push_back({id, variable.namespacePath, variable.name,
                           variable.expression, RuntimeType(variable.type),
                           variable.dimension
                               ? std::optional{RuntimeUnit(*variable.dimension)}
                               : std::nullopt});
  }
  wpilib::expression::EvaluationContext context;
  try {
    context.SetVariables(std::move(definitions));
  } catch (const wpilib::expression::VariableEvaluationError& error) {
    throw EvaluationError{error.Id(), "expression", error.what()};
  }
  Project result = project;
  for (const auto& variable : context.GetVariables()) {
    result.variables.at(variable.definition.id).value = std::visit(
        [](const auto& value) -> VariableValue {
          using Value = std::remove_cvref_t<decltype(value)>;
          if constexpr (std::is_same_v<Value, units::precise_measurement>) {
            const double siValue = value.value() * value.units().multiplier();
            if (!std::isfinite(siValue)) {
              throw std::invalid_argument("variable value must be finite");
            }
            return siValue;
          } else {
            return value;
          }
        },
        variable.value);
  }
  return {std::move(result), std::move(context)};
}

template <typename Function>
auto EvaluateAt(const std::string& id, const std::string& field,
                Function&& function) {
  try {
    return function();
  } catch (const EvaluationError&) {
    throw;
  } catch (const std::exception& error) {
    throw EvaluationError{id, field, error.what()};
  }
}

template <typename Dimension>
void EvaluateScalar(Expression<Dimension>& expression,
                    const wpilib::expression::EvaluationContext& context,
                    const std::string& id, const std::string& field) {
  EvaluateAt(id, field, [&] {
    auto measurement =
        context.Evaluate(expression.source, RuntimeDimension<Dimension>::unit);
    auto value = FromMeasurement<Dimension>(measurement);
    if (!value) {
      throw std::invalid_argument("invalid scalar expression: " +
                                  expression.source);
    }
    expression.value = *value;
  });
}

void EvaluateConfig(RobotConfig& config,
                    const wpilib::expression::EvaluationContext& context,
                    const std::string& projectId) {
  EvaluateScalar(config.mass, context, projectId, "config.mass");
  EvaluateScalar(config.inertia, context, projectId, "config.inertia");
  EvaluateScalar(config.gearing, context, projectId, "config.gearing");
  EvaluateScalar(config.radius, context, projectId, "config.radius");
  EvaluateScalar(config.cof, context, projectId, "config.cof");
  if (config.wheels.differential) {
    EvaluateScalar(config.wheels.differential->trackwidth, context, projectId,
                   "config.wheels.differential.trackwidth");
  }
  if (config.wheels.swerve) {
    for (std::size_t index = 0; index < config.wheels.swerve->modules.size();
         ++index) {
      auto& module = config.wheels.swerve->modules[index];
      module.value = EvaluateAt(
          projectId,
          "config.wheels.swerve.modules[" + std::to_string(index) + "]",
          [&] { return context.EvaluateTranslation2d(module.source); });
    }
  }
  if (config.wheels.mecanum) {
    auto& wheels = *config.wheels.mecanum;
    wheels.frontLeft.value = EvaluateAt(
        projectId, "config.wheels.mecanum.frontLeft",
        [&] { return context.EvaluateTranslation2d(wheels.frontLeft.source); });
    wheels.frontRight.value =
        EvaluateAt(projectId, "config.wheels.mecanum.frontRight", [&] {
          return context.EvaluateTranslation2d(wheels.frontRight.source);
        });
    wheels.backLeft.value = EvaluateAt(
        projectId, "config.wheels.mecanum.backLeft",
        [&] { return context.EvaluateTranslation2d(wheels.backLeft.source); });
    wheels.backRight.value = EvaluateAt(
        projectId, "config.wheels.mecanum.backRight",
        [&] { return context.EvaluateTranslation2d(wheels.backRight.source); });
  }
  if (!config.wheels.differential && !config.wheels.swerve &&
      !config.wheels.mecanum) {
    throw EvaluationError{projectId, "config.wheels",
                          "robot requires at least one wheel configuration"};
  }
  EvaluateAt(projectId, "config.bumpers", [&] {
    config.bumpers.value = context.EvaluatePolygon2d(config.bumpers.source);
    detail::CheckPolygon(wpi::util::json{config.bumpers.value});
  });
  EvaluateScalar(config.motor.freeSpeed, context, projectId,
                 "config.motor.freeSpeed");
  EvaluateScalar(config.motor.stallTorque, context, projectId,
                 "config.motor.stallTorque");
  EvaluateScalar(config.motor.kT, context, projectId, "config.motor.kT");
  EvaluateScalar(config.motor.kV, context, projectId, "config.motor.kV");
  EvaluateScalar(config.motor.supplyLimit, context, projectId,
                 "config.motor.supplyLimit");
  EvaluateScalar(config.motor.statorLimit, context, projectId,
                 "config.motor.statorLimit");
}

void EvaluateParameters(Parameters& parameters,
                        const wpilib::expression::EvaluationContext& context,
                        const std::string& trajectoryId) {
  std::set<std::string> waypointIds;
  for (const auto& waypoint : parameters.waypoints) {
    detail::CheckUuid(waypoint.uuid);
    if (waypoint.uuid.empty() || !waypointIds.insert(waypoint.uuid).second) {
      throw EvaluationError{
          trajectoryId, "params.waypoints",
          "waypoint UUID is empty or duplicated: " + waypoint.uuid};
    }
  }
  auto validateReference = [&](const WaypointId& reference) {
    if (const auto* uuid = std::get_if<std::string>(&reference)) {
      detail::CheckUuid(*uuid);
      if (!waypointIds.contains(*uuid)) {
        throw std::invalid_argument("unknown waypoint UUID: " + *uuid);
      }
    } else if (waypointIds.empty()) {
      throw std::invalid_argument(
          "waypoint boundary requires at least one waypoint");
    } else if (std::get<WaypointBoundary>(reference) !=
                   WaypointBoundary::First &&
               std::get<WaypointBoundary>(reference) !=
                   WaypointBoundary::Last) {
      throw std::invalid_argument("invalid waypoint boundary");
    }
  };
  EvaluateScalar(parameters.targetDt, context, trajectoryId, "params.targetDt");
  for (auto& waypoint : parameters.waypoints) {
    waypoint.pose.value = EvaluateAt(waypoint.uuid, "pose", [&] {
      return context.EvaluatePose2d(waypoint.pose.source);
    });
  }
  std::set<std::string> constraintIds;
  for (auto& constraint : parameters.constraints) {
    detail::CheckUuid(constraint.uuid);
    if (constraint.uuid.empty() ||
        !constraintIds.insert(constraint.uuid).second) {
      throw EvaluationError{
          trajectoryId, "params.constraints",
          "constraint UUID is empty or duplicated: " + constraint.uuid};
    }
    EvaluateAt(constraint.uuid, "from",
               [&] { validateReference(constraint.from); });
    if (constraint.to) {
      EvaluateAt(constraint.uuid, "to",
                 [&] { validateReference(*constraint.to); });
    }
    std::visit(
        [&](auto& data) {
          using Data = std::remove_cvref_t<decltype(data)>;
          if constexpr (std::is_same_v<Data, MaxLinearVelocity> ||
                        std::is_same_v<Data, MaxAngularVelocity>) {
            EvaluateScalar(data.max, context, constraint.uuid, "data.max");
          } else if constexpr (std::is_same_v<Data, HeadingConstraint>) {
            data.heading.value =
                EvaluateAt(constraint.uuid, "data.heading", [&] {
                  return context.EvaluateRotation2d(data.heading.source);
                });
            EvaluateScalar(data.tolerance, context, constraint.uuid,
                           "data.tolerance");
          } else if constexpr (std::is_same_v<Data, KeepInCircle>) {
            data.circle.value = EvaluateAt(constraint.uuid, "data.circle", [&] {
              auto ellipse = context.EvaluateEllipse2d(data.circle.source);
              const auto radius = ellipse.XSemiAxis().value();
              if (!std::isfinite(radius) || radius <= 0 ||
                  radius != ellipse.YSemiAxis().value() ||
                  !std::isfinite(ellipse.Center().X().value()) ||
                  !std::isfinite(ellipse.Center().Y().value()) ||
                  !std::isfinite(ellipse.Rotation().Radians().value())) {
                throw std::invalid_argument(
                    "circle expression must have equal finite positive radii");
              }
              return ellipse;
            });
          }
        },
        constraint.data);
  }
}

Project EvaluateVariables(const Project& project) {
  detail::CheckUuid(project.uuid);
  for (const auto& [id, unused] : project.variables) {
    detail::CheckUuid(id);
  }
  return EvaluateAt(project.uuid, "variables",
                    [&] { return BuildVariables(project).project; });
}

Document EvaluateDocument(const Document& document) {
  detail::CheckUuid(document.project.uuid);
  for (const auto& [id, unused] : document.project.variables) {
    detail::CheckUuid(id);
  }
  if (document.project.uuid.empty() ||
      document.project.version != kProjectSchemaVersion) {
    throw EvaluationError{document.project.uuid, "project",
                          "project UUID or schema version is invalid"};
  }
  auto evaluated = EvaluateAt(document.project.uuid, "variables",
                              [&] { return BuildVariables(document.project); });
  Document result = document;
  result.project = std::move(evaluated.project);
  EvaluateConfig(result.project.config, evaluated.context, result.project.uuid);
  for (const auto& [id, original] : document.trajectories) {
    detail::CheckUuid(id);
    detail::CheckUuid(original.uuid);
    auto trajectory = original;
    if (id.empty() || id != trajectory.uuid ||
        trajectory.version != kTrajectorySchemaVersion) {
      throw EvaluationError{id, "trajectory",
                            "trajectory UUID or schema version is invalid"};
    }
    EvaluateParameters(trajectory.params, evaluated.context, trajectory.uuid);
    std::set<std::string> waypointIds;
    for (const auto& waypoint : trajectory.params.waypoints) {
      waypointIds.insert(waypoint.uuid);
    }
    std::set<std::string> eventIds;
    for (auto& event : trajectory.events) {
      detail::CheckUuid(event.uuid);
      if (event.uuid.empty() || !eventIds.insert(event.uuid).second) {
        throw EvaluationError{
            trajectory.uuid, "events",
            "event UUID is empty or duplicated: " + event.uuid};
      }
      EvaluateAt(event.uuid, "from.target", [&] {
        if (const auto* uuid = std::get_if<std::string>(&event.from.target)) {
          detail::CheckUuid(*uuid);
          if (!waypointIds.contains(*uuid)) {
            throw std::invalid_argument("unknown event waypoint UUID: " +
                                        *uuid);
          }
        } else if (waypointIds.empty() ||
                   (std::get<WaypointBoundary>(event.from.target) !=
                        WaypointBoundary::First &&
                    std::get<WaypointBoundary>(event.from.target) !=
                        WaypointBoundary::Last)) {
          throw std::invalid_argument("invalid event waypoint boundary");
        }
      });
      EvaluateScalar(event.from.offset, evaluated.context, event.uuid,
                     "from.offset");
    }
    result.trajectories = result.trajectories.set(id, std::move(trajectory));
  }
  return result;
}

std::vector<GeometryDragControl> GetGeometryDragControls(
    const Project& project, std::string_view expression,
    DragGeometryType type) {
  struct NumericToken {
    std::size_t start;
    std::size_t end;
    double value;
  };
  struct GeometryValue {
    double x;
    double y;
    double heading;
  };

  auto isWordCharacter = [](char character) {
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9') || character == '_';
  };
  std::vector<NumericToken> tokens;
  for (std::size_t index = 0; index < expression.size();) {
    const char character = expression[index];
    if ((!std::isdigit(static_cast<unsigned char>(character)) &&
         (character != '.' || index + 1 >= expression.size() ||
          !std::isdigit(
              static_cast<unsigned char>(expression[index + 1])))) ||
        (index > 0 &&
         (isWordCharacter(expression[index - 1]) ||
          expression[index - 1] == '.'))) {
      ++index;
      continue;
    }

    double value = 0;
    const auto parsed = std::from_chars(
        expression.data() + index, expression.data() + expression.size(),
        value);
    if (parsed.ec != std::errc{} || parsed.ptr == expression.data() + index) {
      ++index;
      continue;
    }
    const auto end =
        static_cast<std::size_t>(parsed.ptr - expression.data());
    tokens.push_back({index, end, value});
    index = end;
  }

  if (tokens.empty()) {
    return {};
  }

  auto evaluated = BuildVariables(project);
  auto evaluate = [&](std::string_view source) -> GeometryValue {
    if (type == DragGeometryType::Pose2d) {
      const auto pose = evaluated.context.EvaluatePose2d(source);
      return {pose.X().value(), pose.Y().value(),
              pose.Rotation().Radians().value()};
    }
    const auto translation = evaluated.context.EvaluateTranslation2d(source);
    return {translation.X().value(), translation.Y().value(), 0};
  };

  static_cast<void>(evaluate(expression));
  std::vector<GeometryDragControl> controls;
  controls.reserve(tokens.size());
  for (const auto& token : tokens) {
    const double step = std::max(1e-5, std::abs(token.value) * 1e-5);
    auto replace = [&](double value) -> std::string {
      char buffer[64];
      const auto converted = std::to_chars(
          std::begin(buffer), std::end(buffer), value,
          std::chars_format::general,
          std::numeric_limits<double>::max_digits10);
      if (converted.ec != std::errc{}) {
        return {};
      }
      std::string updated{expression};
      updated.replace(token.start, token.end - token.start,
                      buffer, converted.ptr - buffer);
      return updated;
    };

    try {
      const auto plus = evaluate(replace(token.value + step));
      const auto minus = evaluate(replace(token.value - step));
      const double scale = 1.0 / (2 * step);
      double headingDelta = plus.heading - minus.heading;
      headingDelta = std::remainder(headingDelta, 2 * std::acos(-1.0));
      const double dx = (plus.x - minus.x) * scale;
      const double dy = (plus.y - minus.y) * scale;
      const double dheading = headingDelta * scale;
      if (!std::isfinite(dx) || !std::isfinite(dy) ||
          !std::isfinite(dheading) ||
          (std::abs(dx) < 1e-10 && std::abs(dy) < 1e-10 &&
           std::abs(dheading) < 1e-10)) {
        continue;
      }
      controls.push_back(
          {token.start, token.end, token.value, dx, dy, dheading});
    } catch (const std::exception&) {
    }
  }
  return controls;
}

}
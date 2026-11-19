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

GeometryDragAnalysis GetGeometryDragControls(const Project& project,
                                             std::string_view expression,
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
  struct TransformCall {
    std::size_t methodStart;
    std::size_t argumentsStart;
    std::size_t end;
    GeometryValue start;
    GeometryValue finish;
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
          !std::isdigit(static_cast<unsigned char>(expression[index + 1])))) ||
        (index > 0 && (isWordCharacter(expression[index - 1]) ||
                       expression[index - 1] == '.'))) {
      ++index;
      continue;
    }

    double value = 0;
    const auto parsed =
        std::from_chars(expression.data() + index,
                        expression.data() + expression.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr == expression.data() + index) {
      ++index;
      continue;
    }
    const auto end = static_cast<std::size_t>(parsed.ptr - expression.data());
    tokens.push_back({index, end, value});
    index = end;
  }

  GeometryDragAnalysis analysis;
  auto evaluated = BuildVariables(project);
  auto evaluate = [&](std::string_view source) -> GeometryValue {
    switch (type) {
      case DragGeometryType::Pose2d: {
        const auto value = evaluated.context.EvaluatePose2d(source);
        return {value.X().value(), value.Y().value(),
                value.Rotation().Radians().value()};
      }
      case DragGeometryType::Translation2d: {
        const auto value = evaluated.context.EvaluateTranslation2d(source);
        return {value.X().value(), value.Y().value(), 0};
      }
      case DragGeometryType::Polygon2d: {
        const auto value = evaluated.context.EvaluatePolygon2d(source);
        double x = 0;
        double y = 0;
        for (std::size_t index = 0; index < value.Size(); ++index) {
          x += value.At(index).X().value();
          y += value.At(index).Y().value();
        }
        if (value.Size() == 0) {
          return {0, 0, 0};
        }
        const double count = static_cast<double>(value.Size());
        return {x / count, y / count, 0};
      }
      case DragGeometryType::Rotation2d: {
        const auto value = evaluated.context.EvaluateRotation2d(source);
        return {0, 0, value.Radians().value()};
      }
      case DragGeometryType::Transform2d: {
        const auto value = evaluated.context.EvaluateTransform2d(source);
        return {value.X().value(), value.Y().value(),
                value.Rotation().Radians().value()};
      }
      case DragGeometryType::Twist2d: {
        const auto value = evaluated.context.EvaluateTwist2d(source);
        return {value.dx.value(), value.dy.value(), value.dtheta.value()};
      }
      case DragGeometryType::Rectangle2d: {
        const auto value = evaluated.context.EvaluateRectangle2d(source);
        return {value.Center().X().value(), value.Center().Y().value(),
                value.Rotation().Radians().value()};
      }
      case DragGeometryType::Ellipse2d: {
        const auto value = evaluated.context.EvaluateEllipse2d(source);
        return {value.Center().X().value(), value.Center().Y().value(),
                value.Rotation().Radians().value()};
      }
    }
    throw std::invalid_argument("unsupported geometry type");
  };
  auto evaluatePaths = [&](std::string_view source) {
    using Point = GeometryFeaturePoint;
    constexpr double kHandleLength = 0.8;
    std::vector<std::vector<Point>> paths;
    auto addHeading = [&](double x, double y, double heading) {
      paths.push_back({{x, y},
                       {x + kHandleLength * std::cos(heading),
                        y + kHandleLength * std::sin(heading)}});
    };
    switch (type) {
      case DragGeometryType::Pose2d: {
        const auto value = evaluated.context.EvaluatePose2d(source);
        const double x = value.X().value();
        const double y = value.Y().value();
        paths.push_back({{x, y}});
        addHeading(x, y, value.Rotation().Radians().value());
        break;
      }
      case DragGeometryType::Translation2d: {
        const auto value = evaluated.context.EvaluateTranslation2d(source);
        paths.push_back({{value.X().value(), value.Y().value()}});
        break;
      }
      case DragGeometryType::Polygon2d: {
        const auto value = evaluated.context.EvaluatePolygon2d(source);
        std::vector<Point> vertices;
        vertices.reserve(value.Size() + 1);
        for (std::size_t index = 0; index < value.Size(); ++index) {
          vertices.push_back(
              {value.At(index).X().value(), value.At(index).Y().value()});
        }
        if (!vertices.empty()) {
          vertices.push_back(vertices.front());
        }
        paths.push_back(std::move(vertices));
        break;
      }
      case DragGeometryType::Rotation2d: {
        const auto value = evaluated.context.EvaluateRotation2d(source);
        addHeading(0, 0, value.Radians().value());
        break;
      }
      case DragGeometryType::Transform2d: {
        const auto value = evaluated.context.EvaluateTransform2d(source);
        paths.push_back({{0, 0}, {value.X().value(), value.Y().value()}});
        addHeading(value.X().value(), value.Y().value(),
                   value.Rotation().Radians().value());
        break;
      }
      case DragGeometryType::Twist2d: {
        const auto value = evaluated.context.EvaluateTwist2d(source);
        paths.push_back({{0, 0}, {value.dx.value(), value.dy.value()}});
        addHeading(0, 0, value.dtheta.value());
        break;
      }
      case DragGeometryType::Rectangle2d: {
        const auto value = evaluated.context.EvaluateRectangle2d(source);
        const double centerX = value.Center().X().value();
        const double centerY = value.Center().Y().value();
        const double halfWidth = value.XWidth().value() / 2;
        const double halfHeight = value.YWidth().value() / 2;
        const double heading = value.Rotation().Radians().value();
        const double cosine = std::cos(heading);
        const double sine = std::sin(heading);
        std::vector<Point> corners;
        for (const auto [x, y] : {std::pair{-halfWidth, -halfHeight},
                                  std::pair{halfWidth, -halfHeight},
                                  std::pair{halfWidth, halfHeight},
                                  std::pair{-halfWidth, halfHeight},
                                  std::pair{-halfWidth, -halfHeight}}) {
          corners.push_back({centerX + cosine * x - sine * y,
                             centerY + sine * x + cosine * y});
        }
        paths.push_back(std::move(corners));
        break;
      }
      case DragGeometryType::Ellipse2d: {
        const auto value = evaluated.context.EvaluateEllipse2d(source);
        constexpr std::size_t kEllipseSegments = 48;
        const double centerX = value.Center().X().value();
        const double centerY = value.Center().Y().value();
        const double xRadius = value.XSemiAxis().value();
        const double yRadius = value.YSemiAxis().value();
        const double heading = value.Rotation().Radians().value();
        const double cosine = std::cos(heading);
        const double sine = std::sin(heading);
        std::vector<Point> points;
        points.reserve(kEllipseSegments + 1);
        for (std::size_t index = 0; index <= kEllipseSegments; ++index) {
          const double angle = 2 * std::acos(-1.0) *
                               static_cast<double>(index) /
                               static_cast<double>(kEllipseSegments);
          const double x = xRadius * std::cos(angle);
          const double y = yRadius * std::sin(angle);
          points.push_back({centerX + cosine * x - sine * y,
                            centerY + sine * x + cosine * y});
        }
        paths.push_back(std::move(points));
        break;
      }
    }
    return paths;
  };
  analysis.paths = evaluatePaths(expression);

  auto findClosingParenthesis = [&](std::size_t open) {
    std::size_t depth = 0;
    char quote = '\0';
    bool escaped = false;
    for (std::size_t index = open; index < expression.size(); ++index) {
      const char character = expression[index];
      if (quote != '\0') {
        if (escaped) {
          escaped = false;
        } else if (character == '\\') {
          escaped = true;
        } else if (character == quote) {
          quote = '\0';
        }
        continue;
      }
      if (character == '"' || character == '\'') {
        quote = character;
      } else if (character == '(') {
        ++depth;
      } else if (character == ')' && --depth == 0) {
        return index;
      }
    }
    return std::string_view::npos;
  };
  auto evaluatePrefix = [&](std::size_t end) {
    return evaluate(expression.substr(0, end));
  };
  std::vector<TransformCall> calls;
  std::size_t nesting = 0;
  char quote = '\0';
  bool escaped = false;
  for (std::size_t index = 0; index < expression.size(); ++index) {
    const char character = expression[index];
    if (quote != '\0') {
      if (escaped) {
        escaped = false;
      } else if (character == '\\') {
        escaped = true;
      } else if (character == quote) {
        quote = '\0';
      }
      continue;
    }
    if (character == '"' || character == '\'') {
      quote = character;
      continue;
    }
    if (character == '(' || character == '[') {
      ++nesting;
      continue;
    }
    if (character == ')' || character == ']') {
      if (nesting > 0) {
        --nesting;
      }
      continue;
    }
    if (nesting != 0 || character != '.') {
      continue;
    }
    constexpr std::array<std::string_view, 3> kTransformMethods{
        "TransformBy", "RotateBy", "RotateAround"};
    for (const auto method : kTransformMethods) {
      const auto methodStart = index;
      const auto nameStart = index + 1;
      const auto nameEnd = nameStart + method.size();
      if (expression.substr(nameStart, method.size()) != method ||
          nameEnd >= expression.size() || expression[nameEnd] != '(') {
        continue;
      }
      const auto end = findClosingParenthesis(nameEnd);
      if (end == std::string_view::npos) {
        continue;
      }
      try {
        calls.push_back({methodStart, nameEnd + 1, end,
                         evaluatePrefix(methodStart), evaluatePrefix(end + 1)});
      } catch (const std::exception&) {
      }
      break;
    }
  }

  if (calls.empty()) {
    const auto value = evaluate(expression);
    analysis.chain.push_back(
        {value.x, value.y, value.heading, value.x, value.y, value.heading});
  } else {
    const auto& first = calls.front();
    const auto base = evaluatePrefix(first.methodStart);
    analysis.chain.push_back(
        {base.x, base.y, base.heading, base.x, base.y, base.heading});
    for (const auto& call : calls) {
      analysis.chain.push_back({call.start.x, call.start.y, call.start.heading,
                                call.finish.x, call.finish.y,
                                call.finish.heading});
    }
  }

  auto chainIndexForToken = [&](const NumericToken& token) {
    std::optional<std::size_t> selected;
    std::size_t selectedSpan = std::numeric_limits<std::size_t>::max();
    for (std::size_t callIndex = 0; callIndex < calls.size(); ++callIndex) {
      const auto& call = calls[callIndex];
      if (token.start >= call.argumentsStart && token.end <= call.end) {
        const auto span = call.end - call.argumentsStart;
        if (span < selectedSpan) {
          selected = callIndex + 1;
          selectedSpan = span;
        }
      }
    }
    return selected.value_or(0);
  };

  auto& controls = analysis.controls;
  controls.reserve(tokens.size());
  for (const auto& token : tokens) {
    const double step = std::max(1e-5, std::abs(token.value) * 1e-5);
    auto format = [&](double value) -> std::string {
      char buffer[64];
      const auto converted =
          std::to_chars(std::begin(buffer), std::end(buffer), value,
                        std::chars_format::general,
                        std::numeric_limits<double>::max_digits10);
      if (converted.ec != std::errc{}) {
        return {};
      }
      return {buffer, converted.ptr};
    };
    auto replace = [&](double value) -> std::string {
      std::string updated{expression};
      const auto replacement = format(value);
      updated.replace(token.start, token.end - token.start, replacement);
      return updated;
    };
    auto evaluateChangedPrefix = [&](double value, std::size_t end) {
      std::string updated{expression.substr(0, end)};
      const auto replacement = format(value);
      if (token.start < end && token.end <= end) {
        updated.replace(token.start, token.end - token.start, replacement);
      }
      return evaluate(updated);
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
      const auto chainIndex = chainIndexForToken(token);
      const auto& anchor = analysis.chain.at(chainIndex);
      GeometryValue segmentStart{};
      GeometryValue segmentPlus{};
      GeometryValue segmentMinus{};
      if (chainIndex == 0) {
        if (calls.empty()) {
          segmentPlus = plus;
          segmentMinus = minus;
        } else {
          segmentPlus = evaluateChangedPrefix(token.value + step,
                                              calls.front().methodStart);
          segmentMinus = evaluateChangedPrefix(token.value - step,
                                               calls.front().methodStart);
        }
      } else {
        const auto& call = calls.at(chainIndex - 1);
        segmentStart = call.start;
        segmentPlus = evaluateChangedPrefix(token.value + step, call.end + 1);
        segmentMinus = evaluateChangedPrefix(token.value - step, call.end + 1);
      }
      double segmentDx = (segmentPlus.x - segmentMinus.x) * scale;
      double segmentDy = (segmentPlus.y - segmentMinus.y) * scale;
      if (chainIndex > 0) {
        const double cosine = std::cos(segmentStart.heading);
        const double sine = std::sin(segmentStart.heading);
        const double localDx = cosine * segmentDx + sine * segmentDy;
        const double localDy = -sine * segmentDx + cosine * segmentDy;
        segmentDx = localDx;
        segmentDy = localDy;
      }
      double segmentDheading =
          std::remainder(segmentPlus.heading - segmentMinus.heading,
                         2 * std::acos(-1.0)) *
          scale;
      std::vector<GeometryDragEffect> effects;
      const auto plusPaths = evaluatePaths(replace(token.value + step));
      const auto minusPaths = evaluatePaths(replace(token.value - step));
      if (plusPaths.size() != analysis.paths.size() ||
          minusPaths.size() != analysis.paths.size()) {
        continue;
      }
      bool compatiblePaths = true;
      for (std::size_t pathIndex = 0; pathIndex < analysis.paths.size();
           ++pathIndex) {
        const auto& basePath = analysis.paths[pathIndex];
        const auto& plusPath = plusPaths[pathIndex];
        const auto& minusPath = minusPaths[pathIndex];
        if (plusPath.size() != basePath.size() ||
            minusPath.size() != basePath.size()) {
          effects.clear();
          compatiblePaths = false;
          break;
        }
        for (std::size_t pointIndex = 0; pointIndex < basePath.size();
             ++pointIndex) {
          const double effectX =
              (plusPath[pointIndex].x - minusPath[pointIndex].x) * scale;
          const double effectY =
              (plusPath[pointIndex].y - minusPath[pointIndex].y) * scale;
          if (std::isfinite(effectX) && std::isfinite(effectY) &&
              (std::abs(effectX) >= 1e-10 || std::abs(effectY) >= 1e-10)) {
            effects.push_back({pathIndex, pointIndex, effectX, effectY});
          }
        }
      }
      if (!compatiblePaths || effects.empty()) {
        continue;
      }
      controls.push_back({token.start, token.end, token.value, dx, dy, dheading,
                          segmentDx, segmentDy, segmentDheading, chainIndex,
                          anchor.startX, anchor.startY, anchor.startHeading,
                          std::move(effects)});
    } catch (const std::exception&) {
    }
  }
  return analysis;
}
}  // namespace choreo::document::evaluation
#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <wpi/math/geometry/Polygon2d.hpp>
#include <wpi/math/geometry/Pose2d.hpp>
#include <wpi/math/shape/Ellipse2d.hpp>
#include <wpi/math/shape/Rectangle2d.hpp>
#include <wpi/util/json.hpp>

#include "choreo/document/dimension.hpp"
#include "choreo/document/json_validation.hpp"

namespace choreo::document {

template <typename Value>
class Expression {
 public:
  std::string source;
  Value value{};

  wpi::util::json ToJson() const {
    wpi::util::json json;
    json.set_object();
    json["exp"] = source;
    json["val"] = value;
    if constexpr (std::is_same_v<Value, wpi::math::Pose2d>) {
      detail::CheckPose(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Translation2d>) {
      detail::CheckTranslation(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Rotation2d>) {
      detail::CheckRotation(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Polygon2d>) {
      detail::CheckPolygon(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Rectangle2d>) {
      detail::CheckRectangle(json.at("val"));
    }
    return json;
  }

  static Expression FromJson(const wpi::util::json& json) {
    detail::CheckProperties(json, {"exp", "val"});
    if constexpr (std::is_same_v<Value, wpi::math::Pose2d>) {
      detail::CheckPose(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Translation2d>) {
      detail::CheckTranslation(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Rotation2d>) {
      detail::CheckRotation(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Polygon2d>) {
      detail::CheckPolygon(json.at("val"));
    } else if constexpr (std::is_same_v<Value, wpi::math::Rectangle2d>) {
      detail::CheckRectangle(json.at("val"));
    }
    return {json.at("exp").get_string(), json.at("val").get<Value>()};
  }
};

template <int Meter, int Kilogram, int Second, int Ampere, int Kelvin, int Mole,
          int Candela, int Radian>
class Expression<dimension::Dimension<Meter, Kilogram, Second, Ampere, Kelvin,
                                      Mole, Candela, Radian>> {
 public:
  using DimensionType = dimension::Dimension<Meter, Kilogram, Second, Ampere,
                                             Kelvin, Mole, Candela, Radian>;
  using Value = typename DimensionType::Value;

  std::string source;
  Value value{};

  wpi::util::json ToJson() const {
    if (!std::isfinite(value.value())) {
      throw std::invalid_argument("expression value must be finite");
    }
    wpi::util::json json;
    json.set_object();
    json["exp"] = source;
    json["val"] = value.value();
    return json;
  }

  static Expression FromJson(const wpi::util::json& json) {
    detail::CheckProperties(json, {"exp", "val"});
    const double value = json.at("val").get_number();
    if (!std::isfinite(value)) {
      throw std::invalid_argument("expression value must be finite");
    }
    return {json.at("exp").get_string(), Value{value}};
  }
};

template <>
class Expression<wpi::math::Ellipse2d> {
 public:
  std::string source;
  wpi::math::Ellipse2d value{wpi::math::Translation2d{}, 1.0};

  wpi::util::json ToJson() const {
    if (!std::isfinite(value.XSemiAxis().value()) ||
        value.XSemiAxis().value() <= 0 ||
        !std::isfinite(value.YSemiAxis().value()) ||
        value.YSemiAxis().value() <= 0 ||
        !std::isfinite(value.Center().X().value()) ||
        !std::isfinite(value.Center().Y().value()) ||
        !std::isfinite(value.Rotation().Radians().value())) {
      throw std::invalid_argument(
          "ellipse must have finite positive semi-axes and center");
    }
    return wpi::util::json::object("exp", source, "val", value);
  }

  static Expression FromJson(const wpi::util::json& json) {
    detail::CheckProperties(json, {"exp", "val"});
    detail::CheckEllipse(json.at("val"));
    Expression result{json.at("exp").get_string(),
                      json.at("val").get<wpi::math::Ellipse2d>()};
    result.ToJson();
    return result;
  }
};

template <typename Value>
void to_json(wpi::util::json& json, const Expression<Value>& expression) {
  json = expression.ToJson();
}

template <typename Value>
void from_json(const wpi::util::json& json, Expression<Value>& expression) {
  expression = Expression<Value>::FromJson(json);
}

using TranslationExpression = Expression<wpi::math::Translation2d>;
using PoseExpression = Expression<wpi::math::Pose2d>;
using RotationExpression = Expression<wpi::math::Rotation2d>;
using PolygonExpression = Expression<wpi::math::Polygon2d>;
using EllipseExpression = Expression<wpi::math::Ellipse2d>;

}
#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <wpi/util/json.hpp>

namespace choreo::document::detail {

inline void CheckProperties(const wpi::util::json& json,
                            std::initializer_list<std::string_view> allowed) {
  for (const auto& [name, value] : json.get_object()) {
    if (std::find(allowed.begin(), allowed.end(), std::string_view{name}) ==
        allowed.end()) {
      throw std::invalid_argument("unexpected JSON property: " +
                                  std::string{name});
    }
  }
}

inline void CheckNumber(const wpi::util::json& json) {
  if (!std::isfinite(json.get_number())) {
    throw std::invalid_argument("geometry value must be finite");
  }
}

inline void CheckTranslation(const wpi::util::json& json) {
  CheckProperties(json, {"x", "y"});
  CheckNumber(json.at("x"));
  CheckNumber(json.at("y"));
}

inline void CheckRotation(const wpi::util::json& json) {
  CheckProperties(json, {"radians"});
  CheckNumber(json.at("radians"));
}

inline void CheckPose(const wpi::util::json& json) {
  CheckProperties(json, {"translation", "rotation"});
  CheckTranslation(json.at("translation"));
  CheckRotation(json.at("rotation"));
}

inline void CheckPolygon(const wpi::util::json& json) {
  CheckProperties(json, {"vertices"});
  const auto& vertices = json.at("vertices").get_array();
  if (vertices.size() < 3) {
    throw std::invalid_argument("polygon requires at least three vertices");
  }
  std::vector<std::pair<double, double>> points;
  points.reserve(vertices.size());
  for (const auto& vertex : vertices) {
    CheckTranslation(vertex);
    points.emplace_back(vertex.at("x").get_number(),
                        vertex.at("y").get_number());
  }
  double signedArea = 0;
  for (std::size_t index = 0; index < points.size(); ++index) {
    const auto [startX, startY] = points[index];
    const auto [endX, endY] = points[(index + 1) % points.size()];
    if (startX == endX && startY == endY) {
      throw std::invalid_argument("polygon has a duplicate vertex");
    }
    signedArea += startX * endY - startY * endX;
    for (const auto& [pointX, pointY] : points) {
      const auto cross = (endX - startX) * (pointY - startY) -
                         (endY - startY) * (pointX - startX);
      if (!std::isfinite(cross) || cross < 0) {
        throw std::invalid_argument(
            "polygon must be convex and counter-clockwise");
      }
    }
  }
  if (!std::isfinite(signedArea) || signedArea <= 0) {
    throw std::invalid_argument("polygon must have positive area");
  }
}

inline void CheckRectangle(const wpi::util::json& json) {
  CheckProperties(json, {"center", "xWidth", "yWidth"});
  CheckPose(json.at("center"));
  CheckNumber(json.at("xWidth"));
  CheckNumber(json.at("yWidth"));
}

inline void CheckEllipse(const wpi::util::json& json) {
  CheckProperties(json, {"center", "xSemiAxis", "ySemiAxis"});
  CheckPose(json.at("center"));
  CheckNumber(json.at("xSemiAxis"));
  CheckNumber(json.at("ySemiAxis"));
  if (json.at("xSemiAxis").get_number() <= 0 ||
      json.at("ySemiAxis").get_number() <= 0) {
    throw std::invalid_argument("ellipse semi-axes must be positive");
  }
}

}
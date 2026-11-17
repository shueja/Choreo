// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "wpi/math/geometry/Translation2d.hpp"
#include "wpi/util/json.hpp"

namespace wpi::math {

/**
 * A collection of translations representing the vertices of a 2D polygon.
 */
class Polygon2d final {
 public:
  Polygon2d() = default;
  explicit Polygon2d(std::vector<Translation2d> vertices)
      : vertices_{std::move(vertices)} {}

  const std::vector<Translation2d>& Vertices() const noexcept { return vertices_; }
  std::size_t Size() const noexcept { return vertices_.size(); }
  bool Empty() const noexcept { return vertices_.empty(); }
  Translation2d At(std::size_t index) const { return vertices_.at(index); }

  /** Returns the nearest vertex to the given point. */
  Translation2d Nearest(const Translation2d& point) const {
    if (vertices_.empty()) {
      throw std::invalid_argument("nearest requires a non-empty polygon");
    }
    return point.Nearest(std::span<const Translation2d>{vertices_});
  }

 private:
  std::vector<Translation2d> vertices_;
};

inline void to_json(wpi::util::json& json, const Polygon2d& polygon) {
  json.set_object();
  json["vertices"] = wpi::util::json{polygon.Vertices()};
}

inline void from_json(const wpi::util::json& json, Polygon2d& polygon) {
  std::vector<Translation2d> vertices;
  const auto& serializedVertices = json.at("vertices").get_array();
  vertices.reserve(serializedVertices.size());
  for (const auto& vertex : serializedVertices) {
    vertices.push_back(vertex.get<Translation2d>());
  }
  polygon = Polygon2d{std::move(vertices)};
}

}  // namespace wpi::math

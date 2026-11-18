// Copyright (c) Choreo contributors

#pragma once
#include <print>
#include <vector>

#include <choreo/document/parameters.hpp>
#include <trajopt/geometry/pose2.hpp>
#include <trajopt/path/path_builder.hpp>
#include <wpi/units/time.hpp>

#include "segment.hpp"
namespace choreo {
std::vector<Segment> convert_to_segments(const document::Parameters& params);
}  // namespace choreo

#pragma once

#include <string>

#include <immer/map.hpp>

#include "choreo/document/document.hpp"

namespace choreo::document {

class Document {
 public:
  Project project;
  immer::map<std::string, Trajectory> trajectories;
};

}
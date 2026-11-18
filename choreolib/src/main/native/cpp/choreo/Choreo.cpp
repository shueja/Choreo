#include "choreo/Choreo.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace choreo {

Trajectory Choreo::LoadTrajectory(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::invalid_argument("cannot open trajectory file: " + path.string());
  }
  std::string content{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
  if (input.bad()) {
    throw std::runtime_error("cannot read trajectory file: " + path.string());
  }
  return LoadTrajectoryString(content);
}

Trajectory Choreo::LoadTrajectoryString(std::string_view content) {
  return document::LoadRobotTrajectory(content);
}

}
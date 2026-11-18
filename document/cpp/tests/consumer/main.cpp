#include <exception>

#include "choreo/document/robot_trajectory.hpp"

#if defined(CHOREO_CONSUMER_APP)
#include "choreo/document/document_store.hpp"
#endif

int main() {
  try {
#if defined(CHOREO_CONSUMER_APP)
    choreo::document::DocumentStore store{choreo::document::Document{}};
#else
    choreo::document::LoadRobotTrajectory("{}");
#endif
  } catch (const std::exception&) {
    return 0;
  }
  return 1;
}
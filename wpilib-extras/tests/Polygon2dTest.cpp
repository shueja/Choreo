#include "wpi/math/geometry/Polygon2d.hpp"

#include <wpi/util/json.hpp>

#include <stdexcept>
#include <vector>

int main() {
  using wpi::math::Polygon2d;
  using wpi::math::Translation2d;
  using wpi::units::meter_t;

  Polygon2d polygon{std::vector<Translation2d>{
      Translation2d{meter_t{0.0}, meter_t{0.0}},
      Translation2d{meter_t{3.0}, meter_t{0.0}},
      Translation2d{meter_t{0.0}, meter_t{3.0}},
  }};
  if (polygon.Size() != 3 || polygon.Empty() ||
      polygon.At(1) != Translation2d{meter_t{3.0}, meter_t{0.0}} ||
      polygon.Nearest(Translation2d{meter_t{2.8}, meter_t{0.2}}) !=
          Translation2d{meter_t{3.0}, meter_t{0.0}}) {
    return 1;
  }
  wpi::util::json serialized = polygon;
  if (!serialized.is_object() ||
      serialized.at("vertices").get_array().size() != polygon.Size() ||
      serialized.get<Polygon2d>().Vertices() != polygon.Vertices()) {
    return 1;
  }
  try {
    Polygon2d{}.Nearest(Translation2d{});
  } catch (const std::invalid_argument&) {
    return 0;
  }
  return 1;
}

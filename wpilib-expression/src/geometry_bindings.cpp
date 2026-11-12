#include "geometry_bindings.hpp"

#include "units.hpp"

#include <wpi/math/geometry/Pose2d.hpp>
#include <wpi/math/geometry/Rotation2d.hpp>
#include <wpi/math/geometry/Transform2d.hpp>
#include <wpi/math/geometry/Translation2d.hpp>
#include <wpi/math/geometry/Twist2d.hpp>
#include <wpi/math/shape/Ellipse2d.hpp>
#include <wpi/math/shape/Rectangle2d.hpp>

#include <cmath>
#include <numbers>
#include <span>
#include <stdexcept>
#include <typeinfo>
#include <vector>

namespace wpilib::expression::detail {
namespace {

using Pose2d = wpi::math::Pose2d;
using Translation2d = wpi::math::Translation2d;
using Rotation2d = wpi::math::Rotation2d;
using Transform2d = wpi::math::Transform2d;
using Twist2d = wpi::math::Twist2d;
using Ellipse2d = wpi::math::Ellipse2d;
using Rectangle2d = wpi::math::Rectangle2d;
using wpi::units::meter_t;
using wpi::units::radian_t;

template <typename UnitValue>
void RegisterMeasurementConversion(chaiscript::ChaiScript& engine,
                                   const units::precise_unit& unit) {
  engine.add(chaiscript::type_conversion<units::precise_measurement, UnitValue>([](const units::precise_measurement& value) {
    auto converted = TryAs<UnitValue>(value);
    if (!converted) {
      throw chaiscript::exception::bad_boxed_cast(
          chaiscript::user_type<units::precise_measurement>(), typeid(UnitValue),
          "quantity has an incompatible dimension");
    }
    return *converted;
  }));
  engine.add(chaiscript::type_conversion<UnitValue, units::precise_measurement>([unit](const UnitValue& value) {
    try {
      return From(value.value(), unit);
    } catch (const std::domain_error&) {
      throw chaiscript::exception::bad_boxed_cast(
          chaiscript::user_type<UnitValue>(), typeid(units::precise_measurement),
          "unit value cannot be converted to a quantity");
    }
  }));
}

double GeometryScalar(double value) {
  if (!std::isfinite(value)) {
    throw std::domain_error("geometry scalar must be finite");
  }
  return value;
}

double GeometryScalar(const units::precise_measurement& value) {
  if (value.units().base_units() != units::precise::one.base_units()) {
    throw std::invalid_argument("geometry scalar must be dimensionless");
  }
  return GeometryScalar(wpilib::expression::detail::SiValue(value));
}

double GeometryDivisor(double value) {
  if (GeometryScalar(value) == 0.0) {
    throw std::domain_error("division by zero");
  }
  return value;
}

double GeometryDivisor(const units::precise_measurement& value) {
  return GeometryDivisor(GeometryScalar(value));
}

template <typename Geometry>
Geometry NearestGeometry(const Geometry& origin,
                         const std::vector<chaiscript::Boxed_Value>& values) {
  if (values.empty()) {
    throw std::invalid_argument("nearest requires a non-empty collection");
  }
  std::vector<Geometry> candidates;
  candidates.reserve(values.size());
  for (const auto& value : values) {
    candidates.push_back(chaiscript::boxed_cast<Geometry>(value));
  }
  return origin.Nearest(std::span<const Geometry>{candidates});
}

}  // namespace

void ConfigureGeometryBindings(chaiscript::ChaiScript& engine) {
#define BIND_METHOD(type, method) engine.add(chaiscript::fun(&type::method), #method)
#define BIND_CONSTRUCTOR(type, ...) engine.add(chaiscript::constructor<type(__VA_ARGS__)>(), #type)
#define BIND_LAMBDA(name, ...) engine.add(chaiscript::fun(__VA_ARGS__), #name)
// Implicitly copies are used for methods and accessors that return objects by value rather than by reference.
// This ensures that ChaiScript receives a copy of the object, preventing dangling references and maintaining value semantics within the scripting environment.
#define BIND_COPY_METHOD(type, method) \
  BIND_LAMBDA(method, [](const type& object) { return object.method(); })
#define BIND_COPY_ACCESS(type, member) \
  BIND_LAMBDA(member, [](const type& object) { return object.member; })
#define BIND_EQUALITY(type) \
  BIND_LAMBDA(==, [](const type& left, const type& right) { return left == right; }); \
  BIND_LAMBDA(!=, [](const type& left, const type& right) { return left != right; })
#define BIND_SCALING(type) \
  BIND_LAMBDA(*, [](const type& object, double scalar) { return object * GeometryScalar(scalar); }); \
  BIND_LAMBDA(*, [](const type& object, const units::precise_measurement& scalar) { return object * GeometryScalar(scalar); }); \
  BIND_LAMBDA(/, [](const type& object, double scalar) { return object / GeometryDivisor(scalar); }); \
  BIND_LAMBDA(/, [](const type& object, const units::precise_measurement& scalar) { return object / GeometryDivisor(scalar); })

  engine.add(chaiscript::user_type<Pose2d>(), "Pose2d");
  engine.add(chaiscript::user_type<Translation2d>(), "Translation2d");
  engine.add(chaiscript::user_type<Rotation2d>(), "Rotation2d");
  engine.add(chaiscript::user_type<Transform2d>(), "Transform2d");
  engine.add(chaiscript::user_type<Twist2d>(), "Twist2d");
  engine.add(chaiscript::user_type<Rectangle2d>(), "Rectangle2d");
  engine.add(chaiscript::user_type<Ellipse2d>(), "Ellipse2d");
  engine.add(chaiscript::user_type<wpi::util::array<Translation2d, 2>>(), "EllipseFocalPoints");

  RegisterMeasurementConversion<wpi::units::meter_t>(engine, units::precise::m);
  RegisterMeasurementConversion<wpi::units::radian_t>(engine, units::precise::rad);
  RegisterMeasurementConversion<wpi::units::degree_t>(
      engine, units::precise::rad * (std::numbers::pi / 180.0));
  RegisterMeasurementConversion<wpi::units::square_meter_t>(engine, units::precise::m.pow(2));

  BIND_LAMBDA(Pose2d, [](meter_t x, meter_t y, radian_t angle) {
    return Pose2d{x, y, Rotation2d{angle}};
  });
  BIND_CONSTRUCTOR(Pose2d, meter_t, meter_t, Rotation2d);
  BIND_CONSTRUCTOR(Pose2d, Translation2d, Rotation2d);
  BIND_CONSTRUCTOR(Pose2d);
  BIND_METHOD(Pose2d, X);
  BIND_METHOD(Pose2d, Y);
  // These make copies instead of references.
  BIND_COPY_METHOD(Pose2d, Translation);
  BIND_COPY_METHOD(Pose2d, Rotation);
  BIND_LAMBDA(Distance, [](const Pose2d& pose, const Pose2d& other) {
    return pose.Translation().Distance(other.Translation());
  });
  BIND_METHOD(Pose2d, RotateBy);
  BIND_METHOD(Pose2d, RotateAround);
  BIND_METHOD(Pose2d, RelativeTo);
  BIND_METHOD(Pose2d, TransformBy);
  BIND_LAMBDA(Nearest, &NearestGeometry<Pose2d>);
  BIND_LAMBDA(+, [](const Pose2d& pose, const Transform2d& transform) { return pose + transform; });
  BIND_LAMBDA(-, [](const Pose2d& left, const Pose2d& right) { return left - right; });
  BIND_EQUALITY(Pose2d);
  BIND_SCALING(Pose2d);

  BIND_CONSTRUCTOR(Translation2d, meter_t, meter_t);
  BIND_CONSTRUCTOR(Translation2d, meter_t, const Rotation2d&);
  BIND_CONSTRUCTOR(Translation2d);
  BIND_METHOD(Translation2d, X);
  BIND_METHOD(Translation2d, Y);
  BIND_METHOD(Translation2d, Distance);
  BIND_METHOD(Translation2d, SquaredDistance);
  BIND_METHOD(Translation2d, RotateBy);
  BIND_METHOD(Translation2d, RotateAround);
  BIND_METHOD(Translation2d, Norm);
  BIND_METHOD(Translation2d, SquaredNorm);
  BIND_METHOD(Translation2d, Dot);
  BIND_METHOD(Translation2d, Cross);
  BIND_LAMBDA(Nearest, &NearestGeometry<Translation2d>);
  BIND_LAMBDA(Angle, [](const Translation2d& translation) {
    auto angle = translation.Angle();
    if (!angle) {
      throw std::domain_error("translation angle is undefined");
    }
    return *angle;
  });
  BIND_LAMBDA(+, [](const Translation2d& left, const Translation2d& right) { return left + right; });
  BIND_LAMBDA(-, [](const Translation2d& left, const Translation2d& right) { return left - right; });
  BIND_LAMBDA(-, [](const Translation2d& value) { return -value; });
  BIND_EQUALITY(Translation2d);
  BIND_SCALING(Translation2d);

  BIND_CONSTRUCTOR(Rotation2d, radian_t);
  BIND_CONSTRUCTOR(Rotation2d, double, double);
  BIND_CONSTRUCTOR(Rotation2d);
  BIND_METHOD(Rotation2d, Radians);
  BIND_METHOD(Rotation2d, Degrees);
  BIND_METHOD(Rotation2d, RotateBy);
  BIND_METHOD(Rotation2d, RelativeTo);
  BIND_METHOD(Rotation2d, Cos);
  BIND_METHOD(Rotation2d, Sin);
  BIND_METHOD(Rotation2d, Tan);
  BIND_LAMBDA(+, [](const Rotation2d& left, const Rotation2d& right) { return left + right; });
  BIND_LAMBDA(-, [](const Rotation2d& left, const Rotation2d& right) { return left - right; });
  BIND_LAMBDA(-, [](const Rotation2d& value) { return -value; });
  BIND_EQUALITY(Rotation2d);
  BIND_SCALING(Rotation2d);

  BIND_LAMBDA(Transform2d, [](meter_t x, meter_t y, radian_t angle) {
    return Transform2d{x, y, Rotation2d{angle}};
  });
  BIND_CONSTRUCTOR(Transform2d, meter_t, meter_t, Rotation2d);
  BIND_CONSTRUCTOR(Transform2d, Translation2d, Rotation2d);
  BIND_CONSTRUCTOR(Transform2d, const Pose2d&, const Pose2d&);
  BIND_CONSTRUCTOR(Transform2d);
  BIND_METHOD(Transform2d, X);
  BIND_METHOD(Transform2d, Y);
  BIND_COPY_METHOD(Transform2d, Translation);
  BIND_COPY_METHOD(Transform2d, Rotation);
  BIND_METHOD(Transform2d, Inverse);
  BIND_METHOD(Transform2d, Log);
  BIND_LAMBDA(+, [](const Transform2d& left, const Transform2d& right) { return left + right; });
  BIND_EQUALITY(Transform2d);
  BIND_SCALING(Transform2d);

  BIND_CONSTRUCTOR(Twist2d, meter_t, meter_t, radian_t);
  BIND_CONSTRUCTOR(Twist2d);
  BIND_COPY_ACCESS(Twist2d, dx);
  BIND_COPY_ACCESS(Twist2d, dy);
  BIND_COPY_ACCESS(Twist2d, dtheta);
  BIND_METHOD(Twist2d, Exp);
  BIND_EQUALITY(Twist2d);
  BIND_LAMBDA(*, [](const Twist2d& twist, double scalar) { return twist * GeometryScalar(scalar); });
  BIND_LAMBDA(*, [](const Twist2d& twist, const units::precise_measurement& scalar) { return twist * GeometryScalar(scalar); });

  BIND_CONSTRUCTOR(Rectangle2d, const Pose2d&, meter_t, meter_t);
  BIND_CONSTRUCTOR(Rectangle2d, const Translation2d&, const Translation2d&);
  BIND_COPY_METHOD(Rectangle2d, Center);
  BIND_COPY_METHOD(Rectangle2d, Rotation);
  BIND_METHOD(Rectangle2d, XWidth);
  BIND_METHOD(Rectangle2d, YWidth);
  BIND_METHOD(Rectangle2d, TransformBy);
  BIND_METHOD(Rectangle2d, RotateBy);
  BIND_METHOD(Rectangle2d, Intersects);
  BIND_METHOD(Rectangle2d, Contains);
  BIND_METHOD(Rectangle2d, Distance);
  BIND_METHOD(Rectangle2d, Nearest);
  BIND_LAMBDA(==, [](const Rectangle2d& left, const Rectangle2d& right) { return left == right; });
  BIND_LAMBDA(!=, [](const Rectangle2d& left, const Rectangle2d& right) { return left != right; });

  BIND_CONSTRUCTOR(Ellipse2d, const Pose2d&, meter_t, meter_t);
  BIND_CONSTRUCTOR(Ellipse2d, const Translation2d&, double);
  BIND_COPY_METHOD(Ellipse2d, Center);
  BIND_COPY_METHOD(Ellipse2d, Rotation);
  BIND_METHOD(Ellipse2d, XSemiAxis);
  BIND_METHOD(Ellipse2d, YSemiAxis);
  BIND_METHOD(Ellipse2d, FocalPoints);
  BIND_LAMBDA([], [](const wpi::util::array<Translation2d, 2>& points, int index) {
    if (index < 0 || index >= 2) {
      throw std::out_of_range("ellipse focal point index must be 0 or 1");
    }
    return points[static_cast<std::size_t>(index)];
  });
  BIND_METHOD(Ellipse2d, TransformBy);
  BIND_METHOD(Ellipse2d, RotateBy);
  BIND_METHOD(Ellipse2d, Intersects);
  BIND_METHOD(Ellipse2d, Contains);
  BIND_METHOD(Ellipse2d, Distance);
  BIND_METHOD(Ellipse2d, Nearest);
  BIND_LAMBDA(==, [](const Ellipse2d& left, const Ellipse2d& right) { return left == right; });
  BIND_LAMBDA(!=, [](const Ellipse2d& left, const Ellipse2d& right) { return left != right; });

#undef BIND_METHOD
#undef BIND_CONSTRUCTOR
#undef BIND_LAMBDA
#undef BIND_COPY_METHOD
#undef BIND_COPY_ACCESS
#undef BIND_EQUALITY
#undef BIND_SCALING
}

}  // namespace wpilib::expression::detail
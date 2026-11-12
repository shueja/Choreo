#pragma once

#include <chaiscript/chaiscript.hpp>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include <wpi/math/geometry/Pose2d.hpp>
#include <wpi/math/geometry/Rotation2d.hpp>
#include <wpi/math/geometry/Transform2d.hpp>
#include <wpi/math/geometry/Translation2d.hpp>
#include <wpi/math/geometry/Twist2d.hpp>
#include <wpi/math/shape/Ellipse2d.hpp>
#include <wpi/math/shape/Rectangle2d.hpp>

#include "wpilib-expression/quantity.hpp"

namespace wpilib::expression::detail {

using Pose2d = wpi::math::Pose2d;
using Translation2d = wpi::math::Translation2d;
using Rotation2d = wpi::math::Rotation2d;
using Transform2d = wpi::math::Transform2d;
using Twist2d = wpi::math::Twist2d;
using Rectangle2d = wpi::math::Rectangle2d;
using Ellipse2d = wpi::math::Ellipse2d;
struct NamespaceValues;
using NamespacePtr = std::shared_ptr<const NamespaceValues>;
using VariableValue = std::variant<Quantity, Pose2d, Translation2d, Rotation2d,
                                   Transform2d, Twist2d, Rectangle2d, Ellipse2d, NamespacePtr>;
using Variables = std::map<std::string, VariableValue>;

struct NamespaceValues {
    Variables members;
};

void StoreVariable(Variables& variables, std::string name, VariableValue value);
void ConfigureEngine(chaiscript::ChaiScript& engine, const Variables& variables);
void ConfigureGlobals(chaiscript::ChaiScript& engine, const Variables& variables);

}  // namespace wpilib::expression::detail
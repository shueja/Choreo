#pragma once

#include <ratio>

#include <wpi/units/base.hpp>

namespace choreo::document::dimension {

template <int Meter = 0, int Kilogram = 0, int Second = 0, int Ampere = 0,
          int Kelvin = 0, int Mole = 0, int Candela = 0, int Radian = 0>
class Dimension {
 public:
  using BaseUnit =
      wpi::units::base_unit<std::ratio<Meter>, std::ratio<Kilogram>,
                            std::ratio<Second>, std::ratio<Radian>,
                            std::ratio<Ampere>, std::ratio<Kelvin>,
                            std::ratio<Mole>, std::ratio<Candela>>;
  using Value = wpi::units::unit_t<wpi::units::unit<std::ratio<1>, BaseUnit>>;
};

using Dimensionless = Dimension<>;
using Length = Dimension<1>;
using Mass = Dimension<0, 1>;
using MomentOfInertia = Dimension<2, 1>;
using Time = Dimension<0, 0, 1>;
using Current = Dimension<0, 0, 0, 1>;
using Angle = Dimension<0, 0, 0, 0, 0, 0, 0, 1>;
using LinearVelocity = Dimension<1, 0, -1>;
using AngularVelocity = Dimension<0, 0, -1, 0, 0, 0, 0, 1>;
using Torque = Dimension<2, 1, -2>;
using TorqueConstant = Dimension<2, 1, -2, -1>;
using BackEmfConstant = Dimension<2, 1, -2, -1, 0, 0, 0, -1>;

}
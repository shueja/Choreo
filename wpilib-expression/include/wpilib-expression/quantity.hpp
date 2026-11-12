#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <type_traits>

#include <wpi/units/base.hpp>
#include <units/units.hpp>

namespace wpilib::expression {

struct Dimension {
  std::array<int, 5> exponents{};

  friend bool operator==(const Dimension&, const Dimension&) = default;
};

namespace detail {

template <typename Ratio>
constexpr bool MatchesExponent(int exponent) {
  return Ratio::den == 1 && Ratio::num == exponent;
}

template <typename BaseUnit>
constexpr bool MatchesDimension(const Dimension& dimension) {
  return MatchesExponent<typename BaseUnit::meter_ratio>(dimension.exponents[0]) &&
         MatchesExponent<typename BaseUnit::kilogram_ratio>(dimension.exponents[1]) &&
         MatchesExponent<typename BaseUnit::second_ratio>(dimension.exponents[2]) &&
         MatchesExponent<typename BaseUnit::ampere_ratio>(dimension.exponents[3]) &&
         MatchesExponent<typename BaseUnit::radian_ratio>(dimension.exponents[4]) &&
         MatchesExponent<typename BaseUnit::kelvin_ratio>(0) &&
         MatchesExponent<typename BaseUnit::mole_ratio>(0) &&
         MatchesExponent<typename BaseUnit::candela_ratio>(0) &&
         MatchesExponent<typename BaseUnit::byte_ratio>(0);
}

}  // namespace detail

class Quantity {
 public:
  double siValue{};
  Dimension dimension{};

  static Quantity From(double value, std::string_view unit);
  double In(std::string_view unit) const;
  const units::precise_measurement& measurement() const { return measurement_; }

  template <typename UnitValue>
  std::optional<UnitValue> TryAs() const {
    static_assert(wpi::units::traits::is_unit_t<UnitValue>::value,
                  "TryAs requires a WPILib unit_t container type");
    if constexpr (!wpi::units::traits::has_linear_scale<UnitValue>::value) {
      return std::nullopt;
    } else {
      using UnitTag = typename wpi::units::traits::unit_t_traits<UnitValue>::unit_type;
      using BaseUnit = typename wpi::units::traits::base_unit_of<
          typename wpi::units::traits::unit_traits<UnitTag>::base_unit_type>;
      using SiUnit = wpi::units::unit<std::ratio<1>, BaseUnit>;

      if (!detail::MatchesDimension<BaseUnit>(dimension)) {
        return std::nullopt;
      }

      double converted = wpi::units::convert<SiUnit, UnitTag>(siValue);
      if (!std::isfinite(converted)) {
        return std::nullopt;
      }
      return UnitValue{converted};
    }
  }

  template <typename UnitValue>
  bool TryAs(UnitValue& result) const {
    auto converted = TryAs<UnitValue>();
    if (!converted) {
      return false;
    }
    result = *converted;
    return true;
  }

  Quantity() = default;
  Quantity(double value, Dimension dimension);

 private:
  friend Quantity MakeQuantity(const units::precise_measurement& measurement);
  Quantity(units::precise_measurement measurement, Dimension dimension);
  units::precise_measurement measurement_{};
};

}  // namespace wpilib::expression
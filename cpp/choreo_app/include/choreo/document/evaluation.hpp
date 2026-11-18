#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <units/units.hpp>

#include "choreo/document/document.hpp"
#include "choreo/document/expression.hpp"
#include "choreo/document/store.hpp"

namespace choreo::document::evaluation {

class EvaluationError : public std::invalid_argument {
 public:
  EvaluationError(std::string objectId, std::string field, std::string message)
      : std::invalid_argument(objectId + ": " + field + ": " + message),
        objectId_{std::move(objectId)},
        field_{std::move(field)} {}
  const std::string& ObjectId() const noexcept { return objectId_; }
  const std::string& Field() const noexcept { return field_; }

 private:
  std::string objectId_;
  std::string field_;
};

Project EvaluateVariables(const Project& project);
Document EvaluateDocument(const Document& document);
Document RenameVariable(const Document& document, const std::string& id,
                        const std::string& newName);

template <typename Dimension>
struct RuntimeDimension;

template <int Meter, int Kilogram, int Second, int Ampere, int Kelvin, int Mole,
          int Candela, int Radian>
struct RuntimeDimension<dimension::Dimension<Meter, Kilogram, Second, Ampere,
                                             Kelvin, Mole, Candela, Radian>> {
  static constexpr units::precise_unit unit{
      units::detail::unit_data{Meter, Kilogram, Second, Ampere, Kelvin, Mole,
                               Candela, 0, 0, Radian, 0, 0, 0, 0}};
  static_assert(unit.base_units().meter() == Meter &&
                    unit.base_units().kg() == Kilogram &&
                    unit.base_units().second() == Second &&
                    unit.base_units().ampere() == Ampere &&
                    unit.base_units().kelvin() == Kelvin &&
                    unit.base_units().mole() == Mole &&
                    unit.base_units().candela() == Candela &&
                    unit.base_units().radian() == Radian,
                "dimension exponent is not representable by LLNL units");
};

template <typename Dimension>
std::optional<typename Dimension::Value> FromMeasurement(
    const units::precise_measurement& measurement) {
  if (measurement.units().base_units() !=
      RuntimeDimension<Dimension>::unit.base_units()) {
    return std::nullopt;
  }
  const double siValue = measurement.value() * measurement.units().multiplier();
  if (!std::isfinite(siValue)) {
    return std::nullopt;
  }
  return typename Dimension::Value{siValue};
}

}
#pragma once

namespace p135::units {

// Strongly typed SI quantities. Conversion from display units belongs at UI/import boundaries.
template <class Tag>
class Quantity {
public:
    constexpr Quantity() = default;
    explicit constexpr Quantity(double siValue) : value_(siValue) {}

    [[nodiscard]] constexpr double si() const noexcept { return value_; }

    friend constexpr bool operator==(Quantity, Quantity) = default;

private:
    double value_{0.0};
};

struct VoltageTag {};
struct CurrentTag {};
struct ResistanceTag {};
struct InductanceTag {};
struct CapacitanceTag {};
struct FrequencyTag {};
struct PowerTag {};
struct LengthTag {};
struct AreaTag {};
struct VolumeTag {};
struct MassTag {};
struct DensityTag {};
struct ResistivityTag {};
struct TemperatureCoefficientTag {};
struct ElectricFieldStrengthTag {};
struct TemperatureKelvinTag {};
struct TemperatureDifferenceTag {};
struct FluxDensityTag {};
struct MagneticFieldStrengthTag {};
struct TimeTag {};
struct AngleTag {};

using Voltage = Quantity<VoltageTag>;                         // V
using Current = Quantity<CurrentTag>;                         // A
using Resistance = Quantity<ResistanceTag>;                   // ohm
using Inductance = Quantity<InductanceTag>;                   // H
using Capacitance = Quantity<CapacitanceTag>;                 // F
using Frequency = Quantity<FrequencyTag>;                     // Hz
using Power = Quantity<PowerTag>;                             // W
using Length = Quantity<LengthTag>;                           // m
using Area = Quantity<AreaTag>;                               // m^2
using Volume = Quantity<VolumeTag>;                            // m^3
using Mass = Quantity<MassTag>;                               // kg
using Density = Quantity<DensityTag>;                         // kg/m^3
using Resistivity = Quantity<ResistivityTag>;                 // ohm*m
using TemperatureCoefficient = Quantity<TemperatureCoefficientTag>; // 1/K
using ElectricFieldStrength = Quantity<ElectricFieldStrengthTag>; // V/m
using TemperatureKelvin = Quantity<TemperatureKelvinTag>;     // K absolute
using TemperatureDifference = Quantity<TemperatureDifferenceTag>; // K delta
using FluxDensity = Quantity<FluxDensityTag>;                 // T
using MagneticFieldStrength = Quantity<MagneticFieldStrengthTag>; // A/m
using Time = Quantity<TimeTag>;                               // s
using Angle = Quantity<AngleTag>;                             // rad

} // namespace p135::units

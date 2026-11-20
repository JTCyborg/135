#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/material_catalog.hpp"

#include <array>
#include <complex>
#include <cstdint>
#include <vector>

namespace p135::domain {

struct WindingHarmonicCurrent {
    std::uint32_t order{};
    units::Frequency frequency;
    // Total RMS current phasor in each phase winding; parallel paths are
    // already combined in this current and are not multiplied again.
    std::array<std::complex<double>, 3> phaseRmsCurrent{};
};

struct WindingCopperLossRequest {
    ConductorMaterial material;
    std::uint32_t turns{};
    std::uint32_t parallelConductors{1};
    units::Length meanTurnLength;
    units::Area bareConductorCrossSection;
    units::TemperatureKelvin windingTemperature;
    std::vector<WindingHarmonicCurrent> harmonicCurrents;
};

struct WindingHarmonicCopperLoss {
    std::uint32_t order{};
    units::Frequency frequency;
    std::array<units::Power, 3> phaseLoss{};
    units::Power totalThreePhaseLoss;
};

struct WindingCopperLossResult {
    units::Resistance resistanceAt20C;
    units::Resistance resistanceAtOperatingTemperature;
    units::Resistivity resistivityAtOperatingTemperature;
    units::Length totalCopperLengthPerParallelPath;
    std::vector<WindingHarmonicCopperLoss> harmonicLosses;
    units::Power totalThreePhaseCopperLoss;
    // This is a DC-resistance model: skin, proximity, circulating-current,
    // lead and joint losses are not included.
    bool includesAcEffects{false};
};

/// Computes three-phase winding I^2R losses from explicit conductor geometry
/// and harmonic RMS currents. No wire size, mean turn length or temperature
/// is inferred; AC resistance effects are deliberately not approximated.
[[nodiscard]] Result<WindingCopperLossResult> calculateWindingCopperLoss(
    const WindingCopperLossRequest& request);

} // namespace p135::domain

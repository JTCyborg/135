#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/filter/analysis.hpp"

#include <complex>
#include <cstdint>
#include <vector>

namespace p135::filter {

struct HarmonicVoltage {
    std::uint32_t order{};
    /// Per-phase RMS positive-sequence Thevenin voltage phasor.
    std::complex<double> phaseVoltageRms{};
};

struct HarmonicAnalysisRequest {
    FilterDesign design;
    SourceImpedance source;
    /// Must include exactly one fundamental (order 1) entry.
    std::vector<HarmonicVoltage> sourceVoltageSpectrum;
};

struct HarmonicAnalysisPoint {
    std::uint32_t order{};
    units::Frequency frequency;
    double sourceVoltageMagnitudeV{};
    double busVoltageMagnitudeV{};
    double filterCurrentMagnitudeA{};
    double sourceCurrentMagnitudeA{};
    double filterImpedanceMagnitudeOhm{};
};

struct BranchHarmonicStress {
    std::uint32_t branchId{};
    /// Root-sum-square RMS current across the supplied spectrum.
    double totalRmsCurrentA{};
    double capacitorRmsVoltageV{};
    double inductorRmsVoltageV{};
    double dampingResistorRmsVoltageV{};
    /// Sum of I_h^2 * R; no fundamental-frequency thermal derating is applied.
    double dampingResistorLossW{};
};

struct HarmonicAnalysisResult {
    std::vector<HarmonicAnalysisPoint> points;
    std::vector<BranchHarmonicStress> branchStresses;
    double busVoltageThd{};
    double sourceCurrentThd{};
    double busFundamentalVoltageV{};
    double sourceFundamentalCurrentA{};
};

/// Calculates a balanced positive-sequence harmonic response using a per-phase
/// Thevenin source and ideal shunt RLC branches. THD is a ratio (e.g. 0.05 = 5%).
[[nodiscard]] Result<HarmonicAnalysisResult> analyzeHarmonics(
    const HarmonicAnalysisRequest& request);

} // namespace p135::filter

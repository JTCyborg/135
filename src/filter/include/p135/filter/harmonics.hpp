#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/filter/analysis.hpp"

#include <array>
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace p135::filter {

enum class HarmonicSequence { Positive, Negative };

struct HarmonicVoltage {
    std::uint32_t order{};
    HarmonicSequence sequence{HarmonicSequence::Positive};
    /// Phase-A RMS Thevenin voltage phasor for the declared balanced sequence.
    std::complex<double> phaseVoltageRms{};
};

/// Balanced harmonic current drawn by one nonlinear load at the PCC.
/// Positive phasor direction is load consumption (current leaving the bus).
struct HarmonicCurrentInjection {
    std::string sourceId;
    std::uint32_t order{};
    HarmonicSequence sequence{HarmonicSequence::Positive};
    /// Phase-A RMS current phasor; multiple loads are superposed as complex values.
    std::complex<double> phaseCurrentRms{};
};

struct HarmonicAnalysisRequest {
    FilterDesign design;
    SourceImpedance source;
    /// Must include exactly one fundamental (order 1) entry.
    std::vector<HarmonicVoltage> sourceVoltageSpectrum;
    /// Optional nonlinear-load current spectrum. Each order/sequence must have a
    /// corresponding voltage-spectrum entry (use a zero voltage phasor when absent).
    std::vector<HarmonicCurrentInjection> loadCurrentInjections;
};

struct HarmonicAnalysisPoint {
    std::uint32_t order{};
    units::Frequency frequency;
    double sourceVoltageMagnitudeV{};
    double busVoltageMagnitudeV{};
    double filterCurrentMagnitudeA{};
    double sourceCurrentMagnitudeA{};
    double loadCurrentMagnitudeA{};
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

struct BranchHarmonicPoint {
    std::uint32_t branchId{};
    std::uint32_t order{};
    units::Frequency frequency;
    double capacitorCurrentRmsA{};
    /// Complex winding-current phasors for phases A/B/C at this harmonic.
    std::array<std::complex<double>, 3> phaseCurrentRms{};
};

struct HarmonicAnalysisResult {
    std::vector<HarmonicAnalysisPoint> points;
    std::vector<BranchHarmonicPoint> branchPoints;
    std::vector<BranchHarmonicStress> branchStresses;
    double busVoltageThd{};
    double sourceCurrentThd{};
    double busFundamentalVoltageV{};
    double sourceFundamentalCurrentA{};
};

/// Calculates a balanced positive-/negative-sequence harmonic response using a per-phase
/// Thevenin source and ideal shunt RLC branches. THD is a ratio (e.g. 0.05 = 5%).
[[nodiscard]] Result<HarmonicAnalysisResult> analyzeHarmonics(
    const HarmonicAnalysisRequest& request);

} // namespace p135::filter

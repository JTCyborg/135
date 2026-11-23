#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/filter/synthesis.hpp"

#include <cstddef>
#include <vector>

namespace p135::filter {

/// Simplified per-phase Thevenin source model: R_source + j*w*L_source.
struct SourceImpedance {
    units::Resistance resistance;
    units::Inductance inductance;
};

struct FrequencyResponseRequest {
    FilterDesign design;
    SourceImpedance source;
    units::Frequency startFrequency;
    units::Frequency stopFrequency;
    std::size_t points{401};
    bool logarithmicSpacing{true};
};

struct FrequencyResponsePoint {
    units::Frequency frequency;
    /// Magnitude of the complete shunt filter bank driving-point impedance, in ohms.
    double filterImpedanceMagnitudeOhm{};
    /// Magnitude of source impedance in the per-phase Thevenin model, in ohms.
    double sourceImpedanceMagnitudeOhm{};
    /// Magnitude of source || filter impedance at the point of common coupling, in ohms.
    double busImpedanceMagnitudeOhm{};
    /// Phase angle of source || filter impedance, in radians.
    double busImpedancePhaseRadians{};
    /// Magnitude of grid current divided by a unit load-current injection at this frequency.
    /// This is a small-signal transfer characteristic, not a measured operating harmonic.
    double gridCurrentTransferMagnitudeRatio{};
};

struct ResonancePeak {
    units::Frequency frequency;
    double busImpedanceMagnitudeOhm{};
};

struct FrequencyResponseResult {
    std::vector<FrequencyResponsePoint> points;
    /// Discrete local maxima of the bus impedance curve, in ascending frequency order.
    std::vector<ResonancePeak> resonancePeaks;
};

/// Calculates the per-phase driving-point impedance of the shunt filter and source
/// over a frequency range. This is an impedance scan, not a harmonic-compliance check.
[[nodiscard]] Result<FrequencyResponseResult> analyzeFrequencyResponse(
    const FrequencyResponseRequest& request);

} // namespace p135::filter

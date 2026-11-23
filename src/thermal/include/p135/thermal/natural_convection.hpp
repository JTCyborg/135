#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <string>

namespace p135::thermal {

/// Explicit thermophysical properties of the surrounding fluid at film
/// temperature. Values must come from a traceable source; this API does not
/// silently substitute standard-air properties.
struct NaturalConvectionFluidProperties {
    double thermalConductivityWPerMK{};
    double kinematicViscosityM2PerS{};
    double thermalDiffusivityM2PerS{};
    double prandtlNumber{};
    std::string sourceReference;
};

/// Geometry and boundary conditions for an isolated isothermal vertical plate.
/// Area is the actually exposed area of one face (or the sum of faces with
/// equivalent conditions); characteristic length is the vertical height.
struct VerticalPlateNaturalConvectionRequest {
    units::Area exposedArea;
    units::Length verticalCharacteristicLength;
    units::TemperatureKelvin ambientTemperature;
    units::TemperatureKelvin surfaceTemperature;
    NaturalConvectionFluidProperties fluid;
    std::string geometrySourceReference;
    bool unobstructedQuiescentAmbient{false};
};

struct NaturalConvectionResult {
    double filmTemperatureK{};
    double rayleighNumber{};
    double nusseltNumber{};
    double heatTransferCoefficientWPerM2K{};
    double thermalResistanceKPerW{};
    std::string correlation;
    std::string status;
    std::string explanation;
};

/// Churchill-Chu correlation for free convection from an isolated vertical
/// plate. Returns a preliminary h and Rtheta only when the explicit inputs
/// satisfy the correlation's range and boundary-condition requirements.
/// This is not a winding-surface model and does not account for enclosure,
/// neighboring surfaces, radiation, supports, or non-isothermal geometry.
[[nodiscard]] Result<NaturalConvectionResult> analyzeVerticalPlateNaturalConvection(
    const VerticalPlateNaturalConvectionRequest& request);

} // namespace p135::thermal

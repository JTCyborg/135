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


/// Orientation of a heated horizontal plate relative to gravity.
enum class HorizontalPlateOrientation {
    HeatedFaceUpward,
    HeatedFaceDownward
};

/// Horizontal plate boundary conditions. Characteristic length is derived
/// from exposed area / perimeter; perimeter must describe the exposed outline.
struct HorizontalPlateNaturalConvectionRequest {
    units::Area exposedArea;
    units::Length exposedPerimeter;
    units::TemperatureKelvin ambientTemperature;
    units::TemperatureKelvin surfaceTemperature;
    NaturalConvectionFluidProperties fluid;
    std::string geometrySourceReference;
    HorizontalPlateOrientation orientation{HorizontalPlateOrientation::HeatedFaceUpward};
    bool unobstructedQuiescentAmbient{false};
};

/// McAdams horizontal-plate free-convection correlations for a heated face:
/// upward-facing: Nu=0.54 Ra^(1/4) [1e4,1e7], 0.15 Ra^(1/3) [1e7,1e11];
/// downward-facing: Nu=0.27 Ra^(1/4) [1e5,1e10].
/// Results are preliminary and do not model winding enclosures or radiation.
[[nodiscard]] Result<NaturalConvectionResult> analyzeHorizontalPlateNaturalConvection(
    const HorizontalPlateNaturalConvectionRequest& request);

} // namespace p135::thermal

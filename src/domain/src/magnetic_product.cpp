#include "p135/domain/magnetic_product.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace p135::domain {
namespace {

Result<void> invalid(std::string message, std::string field,
                     ErrorCode code = ErrorCode::InvalidInput)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "MAGNETIC_PRODUCT_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<void>::failure(std::move(error));
}

bool positive(double value) { return std::isfinite(value) && value > 0.0; }
bool nonNegative(double value) { return std::isfinite(value) && value >= 0.0; }

} // namespace

Result<void> validate(const MagneticProduct& product)
{
    if (product.name.empty())
        return invalid("Product name is required.", "name");
    if (!positive(product.core.effectiveArea.si()))
        return invalid("Effective core area Ae must be positive.", "core.effectiveArea");
    if (!positive(product.core.windowArea.si()))
        return invalid("Core window area Aw must be positive.", "core.windowArea");
    if (!positive(product.core.meanMagneticPath.si()))
        return invalid("Mean magnetic path le must be positive.", "core.meanMagneticPath");
    if (!nonNegative(product.core.airGap.si()))
        return invalid("Air gap must be finite and non-negative.", "core.airGap");
    if (!std::isfinite(product.core.stackingFactor) ||
        product.core.stackingFactor <= 0.0 || product.core.stackingFactor > 1.0)
        return invalid("Stacking factor must be in the interval (0, 1].",
                       "core.stackingFactor");

    if (product.coreMaterial.id.empty() || product.coreMaterial.grade.empty())
        return invalid("Core material ID and grade are required.", "coreMaterial");
    if (!positive(product.coreMaterial.density.si()))
        return invalid("Core material density must be positive.", "coreMaterial.density");
    if (product.coreMaterial.bhCurve.size() < 2)
        return invalid("At least two measured B-H points are required.",
                       "coreMaterial.bhCurve", ErrorCode::MissingMaterialProperty);

    double previousH = -1.0;
    double previousB = -1.0;
    for (std::size_t i = 0; i < product.coreMaterial.bhCurve.size(); ++i) {
        const auto& point = product.coreMaterial.bhCurve[i];
        const double h = point.fieldStrength.si();
        const double b = point.fluxDensity.si();
        if (!nonNegative(h) || !nonNegative(b) || h <= previousH || b < previousB)
            return invalid("B-H points must be finite, non-negative and ordered by increasing H with non-decreasing B.",
                           "coreMaterial.bhCurve[" + std::to_string(i) + "]");
        previousH = h;
        previousB = b;
    }

    if (product.windings.empty())
        return invalid("At least one winding is required.", "windings");

    for (std::size_t i = 0; i < product.windings.size(); ++i) {
        const auto& winding = product.windings[i];
        const auto prefix = "windings[" + std::to_string(i) + "]";
        if (winding.name.empty())
            return invalid("Winding name is required.", prefix + ".name");
        if (winding.turns == 0 || winding.turnsPerLayer == 0 || winding.layers == 0 ||
            winding.parallelConductors == 0)
            return invalid("Turns, turns per layer, layers and parallel conductor count must be non-zero.",
                           prefix);
        if (static_cast<std::uint64_t>(winding.turnsPerLayer) * winding.layers < winding.turns)
            return invalid("Specified layer capacity is smaller than total turns.",
                           prefix + ".layers");

        const auto& conductor = winding.conductor;
        if (conductor.id.empty() || conductor.material.empty())
            return invalid("Conductor ID and material are required.", prefix + ".conductor");
        if (!positive(conductor.bareCrossSection.si()) ||
            !positive(conductor.resistivityAt20C) ||
            !positive(conductor.temperatureCoefficient))
            return invalid("Conductor area, resistivity and temperature coefficient must be positive.",
                           prefix + ".conductor");
        if (!nonNegative(conductor.insulationBuild.si()))
            return invalid("Conductor insulation build must be non-negative.",
                           prefix + ".conductor.insulationBuild");
        if (conductor.shape == ConductorShape::Rectangular ||
            conductor.shape == ConductorShape::Busbar ||
            conductor.shape == ConductorShape::Foil) {
            if (!positive(conductor.bareWidth.si()) || !positive(conductor.bareThickness.si()))
                return invalid("Rectangular conductor width and thickness are required.",
                               prefix + ".conductor");
        }

        for (std::size_t j = 0; j < winding.insulation.size(); ++j) {
            const auto& layer = winding.insulation[j];
            if (layer.material.empty() || !positive(layer.thickness.si()))
                return invalid("Insulation material and positive thickness are required.",
                               prefix + ".insulation[" + std::to_string(j) + "]");
            if (layer.dielectricStrength &&
                !positive(*layer.dielectricStrength))
                return invalid("Dielectric strength must be positive when specified.",
                               prefix + ".insulation[" + std::to_string(j) + "].dielectricStrength");
        }

        if (winding.bobbin.used) {
            if (winding.bobbin.material.empty() ||
                !positive(winding.bobbin.innerWidth.si()) ||
                !positive(winding.bobbin.innerHeight.si()) ||
                !positive(winding.bobbin.wallThickness.si()) ||
                !nonNegative(winding.bobbin.flangeThickness.si()))
                return invalid("A used bobbin requires material, positive inner dimensions and wall thickness.",
                               prefix + ".bobbin");
        }
    }

    if (!positive(product.cooling.ambientTemperature.si()) ||
        !positive(product.cooling.maximumWindingTemperature.si()) ||
        product.cooling.maximumWindingTemperature.si() <= product.cooling.ambientTemperature.si())
        return invalid("Maximum winding temperature must exceed a positive ambient temperature.",
                       "cooling");

    return Result<void>::success();
}

} // namespace p135::domain

#include "p135/domain/three_limb_core_mass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>

namespace p135::domain {
namespace {

Result<ThreeLimbCoreMassResult> invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "THREE_LIMB_CORE_MASS_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<ThreeLimbCoreMassResult>::failure(std::move(error));
}

} // namespace

Result<ThreeLimbCoreMassResult> calculateThreeLimbCoreMass(
    const ThreeLimbCoreMassRequest& request)
{
    if (request.geometry == nullptr)
        return invalid("Physical core geometry is required.", "geometry");
    if (request.material == nullptr)
        return invalid("A revision-bound core material is required.", "material");

    const auto& geometry = *request.geometry;
    const auto& material = *request.material;
    const auto geometryValidation = validate(geometry);
    if (!geometryValidation)
        return Result<ThreeLimbCoreMassResult>::failure(geometryValidation.error());

    if (std::any_of(geometry.joints.begin(), geometry.joints.end(),
            [](const CoreJointPhysicalGeometry& joint) {
                return joint.construction != CoreJointType::Butt;
            })) {
        return invalid(
            "Step-Lap core mass is not calculated from nominal dimensions: packet-resolved "
            "CAD union volumes and a region ownership map are required to avoid assigning "
            "interleaved joint steel to the wrong magnetic region.",
            "core.joints");
    }

    if (!std::isfinite(material.density.si()) || material.density.si() <= 0.0)
        return invalid("Core material density must be finite and positive.", "material.density");
    if (!std::isfinite(request.stackingFactor) ||
        request.stackingFactor <= 0.0 || request.stackingFactor > 1.0)
        return invalid("Stacking factor must be finite and in the interval (0, 1].",
                       "stackingFactor");

    if (material.id.empty() || request.materialRevisionReference.empty() ||
        request.densitySourceReference.empty() ||
        request.stackingFactorSourceReference.empty() ||
        request.dimensionalSource.empty() || request.dimensionalRevision.empty()) {
        return invalid(
            "Material identity/revision, density source, stacking-factor source and "
            "dimensional source/revision are all required for mass provenance.",
            "provenance");
    }

    std::array<double, 7> grossVolumes{};
    std::array<double, 7> removedAirVolumes{};

    // Partition rule: each limb owns its full vertical column, including the
    // top/bottom joint intersection blocks. Each yoke owns only the clear
    // window span between adjacent limbs. This makes the seven volumes
    // disjoint for the explicitly validated equal-depth butt-joint geometry.
    for (std::size_t limb = 0; limb < 3; ++limb) {
        grossVolumes[limb] =
            geometry.limbWidths[limb].si() *
            geometry.limbDepths[limb].si() *
            geometry.coreOverallHeight.si();
    }

    for (std::size_t yoke = 0; yoke < 4; ++yoke) {
        const std::size_t window = yoke % 2;
        grossVolumes[3 + yoke] =
            geometry.windowWidths[window].si() *
            geometry.yokeThicknesses[yoke].si() *
            geometry.yokeDepths[yoke].si();
    }

    for (std::size_t gapIndex = 0; gapIndex < geometry.airGaps.size(); ++gapIndex) {
        const auto& gap = geometry.airGaps[gapIndex];
        const double gapLength = gap.length.si();
        const std::size_t segment = static_cast<std::size_t>(gap.segment);
        if (segment >= grossVolumes.size())
            return invalid("Air gap references an unknown core segment.",
                           "core.airGaps[" + std::to_string(gapIndex) + "].segment");

        if (segment < 3) {
            removedAirVolumes[segment] +=
                gapLength * geometry.limbWidths[segment].si() *
                geometry.limbDepths[segment].si();
            continue;
        }

        const std::size_t yoke = segment - 3;
        const std::size_t window = yoke % 2;
        const double windowStart = window == 0
            ? geometry.limbWidths[0].si()
            : 0.5 * geometry.limbWidths[1].si();
        const double windowEnd = windowStart + geometry.windowWidths[window].si();
        const double gapStart = gap.positionFromSegmentStart.si();
        const double gapEnd = gapStart + gapLength;
        const double tolerance = std::max(1.0e-9, windowEnd * 1.0e-6);
        if (gapStart < windowStart - tolerance || gapEnd > windowEnd + tolerance) {
            return invalid(
                "A yoke air gap must lie wholly inside the clear window span. "
                "Gaps crossing a limb/yoke joint cannot be assigned to one magnetic region.",
                "core.airGaps[" + std::to_string(gapIndex) + "]");
        }
        removedAirVolumes[segment] +=
            gapLength * geometry.yokeThicknesses[yoke].si() *
            geometry.yokeDepths[yoke].si();
    }

    ThreeLimbCoreMassResult result;
    double totalVolume = 0.0;
    double totalMass = 0.0;
    for (std::size_t region = 0; region < grossVolumes.size(); ++region) {
        const double netVolume = grossVolumes[region] - removedAirVolumes[region];
        if (!std::isfinite(netVolume) || netVolume <= 0.0)
            return invalid("Net steel volume must be finite and positive.",
                           "regions[" + std::to_string(region) + "].volume");

        const double steelVolume = netVolume * request.stackingFactor;
        const double mass = steelVolume * material.density.si();
        if (!std::isfinite(steelVolume) || steelVolume <= 0.0 ||
            !std::isfinite(mass) || mass <= 0.0)
            return invalid("Calculated steel volume and mass must be finite and positive.",
                           "regions[" + std::to_string(region) + "]");

        result.netSteelVolumes[region] = units::Volume{steelVolume};
        result.regionMasses[region] = units::Mass{mass};
        result.regionMassSourceReferences[region] =
            "geometry=" + request.dimensionalSource + "@" + request.dimensionalRevision +
            "; material=" + request.materialRevisionReference +
            "; density=" + request.densitySourceReference +
            "; stackingFactor=" + request.stackingFactorSourceReference +
            "; partition=LIMB_OWNS_JOINTS; region=" + std::to_string(region);

        totalVolume += steelVolume;
        totalMass += mass;
    }

    if (!std::isfinite(totalVolume) || !std::isfinite(totalMass) ||
        totalVolume <= 0.0 || totalMass <= 0.0)
        return invalid("Total core volume and mass must be finite and positive.", "total");

    result.totalNetSteelVolume = units::Volume{totalVolume};
    result.totalMass = units::Mass{totalMass};
    return Result<ThreeLimbCoreMassResult>::success(std::move(result));
}

Result<ThreeLimbCoreLossRequest> bindThreeLimbCoreMass(
    const ThreeLimbCoreLossRequest& operatingPoint,
    const ThreeLimbCoreMassResult& mass)
{
    ThreeLimbCoreLossRequest bound = operatingPoint;
    bound.coreMassVerificationStatus = mass.verificationStatus;
    for (std::size_t region = 0; region < mass.regionMasses.size(); ++region) {
        const double value = mass.regionMasses[region].si();
        if (!std::isfinite(value) || value <= 0.0 ||
            mass.regionMassSourceReferences[region].empty()) {
            Error error;
            error.code = ErrorCode::InvalidInput;
            error.message = "Calculated core region mass or its provenance is missing.";
            error.diagnostics.push_back(Diagnostic{
                Severity::Error, "THREE_LIMB_CORE_MASS_BINDING_INVALID",
                error.message, "regions[" + std::to_string(region) + "]",
                std::nullopt, std::nullopt
            });
            return Result<ThreeLimbCoreLossRequest>::failure(std::move(error));
        }
        bound.regionMasses[region] = mass.regionMasses[region];
        bound.regionMassSourceReferences[region] =
            mass.regionMassSourceReferences[region];
    }
    return Result<ThreeLimbCoreLossRequest>::success(std::move(bound));
}

} // namespace p135::domain

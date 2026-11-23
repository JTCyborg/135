#include "p135/thermal/reactor_thermal_network.hpp"

#include <cmath>
#include <string>
#include <type_traits>
#include <utility>

namespace p135::thermal {
namespace {

Result<ReactorThermalNetworkResult> fail(
    ErrorCode code, std::string message, std::string field)
{
    return Result<ReactorThermalNetworkResult>::failure(Error{
        code,
        message,
        {Diagnostic{Severity::Error, "REACTOR_THERMAL_NETWORK_INVALID",
            std::move(message), std::move(field), std::nullopt, std::nullopt}}
    });
}

} // namespace

Result<ReactorThermalNetworkResult> analyzeReactorThermalNetwork(
    const ReactorThermalNetworkRequest& request)
{
    if (request.windingLossSourceReference.empty())
        return fail(ErrorCode::MissingRequiredData,
            "A source reference for the winding copper-loss study is required.",
            "windingLossSourceReference");

    static const std::array<const char*, 7> coreIds{
        "core.limb_a", "core.limb_b", "core.limb_c",
        "core.yoke_top_ab", "core.yoke_top_bc",
        "core.yoke_bottom_ab", "core.yoke_bottom_bc"
    };
    static const std::array<const char*, 7> coreLabels{
        "Core limb A", "Core limb B", "Core limb C",
        "Top yoke AB", "Top yoke BC",
        "Bottom yoke AB", "Bottom yoke BC"
    };
    static const std::array<const char*, 3> windingIds{
        "winding.phase_a", "winding.phase_b", "winding.phase_c"
    };
    static const std::array<const char*, 3> windingLabels{
        "Winding phase A", "Winding phase B", "Winding phase C"
    };

    ThermalNetworkRequest networkRequest;
    networkRequest.ambientTemperature = request.ambientTemperature;
    networkRequest.links = request.links;
    networkRequest.ambientLinks = request.ambientLinks;

    ReactorThermalNetworkResult result;
    result.naturalConvectionSurfaces.reserve(request.naturalConvectionSurfaces.size());
    for (std::size_t i = 0; i < request.naturalConvectionSurfaces.size(); ++i) {
        const auto& boundary = request.naturalConvectionSurfaces[i];
        if (boundary.nodeId.empty() || boundary.boundaryModelSourceReference.empty())
            return fail(ErrorCode::MissingRequiredData,
                "Every natural-convection surface requires a target node and boundary-model source reference.",
                "naturalConvectionSurfaces[" + std::to_string(i) + "]");

        const double ambient = request.ambientTemperature.si();
        const bool ambientMatches = std::visit(
            [ambient](const auto& surfaceRequest) {
                const double surfaceAmbient = surfaceRequest.ambientTemperature.si();
                return std::isfinite(surfaceAmbient) &&
                    std::abs(surfaceAmbient - ambient) <= 1.0e-9 * (1.0 + std::abs(ambient));
            }, boundary.surface);
        if (!ambientMatches)
            return fail(ErrorCode::InvalidInput,
                "Natural-convection surface ambient temperature must match the reactor network ambient temperature.",
                "naturalConvectionSurfaces[" + std::to_string(i) + "].ambientTemperature");

        const auto convection = std::visit(
            [](const auto& surfaceRequest) -> Result<NaturalConvectionResult> {
                using SurfaceRequest = std::decay_t<decltype(surfaceRequest)>;
                if constexpr (std::is_same_v<SurfaceRequest, VerticalPlateNaturalConvectionRequest>)
                    return analyzeVerticalPlateNaturalConvection(surfaceRequest);
                else
                    return analyzeHorizontalPlateNaturalConvection(surfaceRequest);
            }, boundary.surface);
        if (!convection)
            return Result<ReactorThermalNetworkResult>::failure(convection.error());

        const auto ambientLink = makeNaturalConvectionAmbientLink(
            boundary.nodeId, convection.value(), boundary.boundaryModelSourceReference);
        if (!ambientLink)
            return Result<ReactorThermalNetworkResult>::failure(ambientLink.error());

        networkRequest.ambientLinks.push_back(ambientLink.value());
        result.naturalConvectionSurfaces.push_back(
            ReactorNaturalConvectionSurfaceResult{boundary.nodeId, convection.value()});
    }

    for (std::size_t i = 0; i < coreIds.size(); ++i) {
        const auto& region = request.coreLosses.regions[i];
        const double watts = region.totalLoss.si();
        if (!std::isfinite(watts) || watts < 0.0 ||
            region.lossDataReference.empty() ||
            region.coreMassSourceReference.empty())
            return fail(ErrorCode::MissingRequiredData,
                "Every core region requires a finite non-negative loss and both loss-data and mass provenance.",
                "coreLosses.regions");

        ThermalNetworkNodeInput node;
        node.id = coreIds[i];
        node.label = coreLabels[i];
        node.loss = region.totalLoss;
        node.maximumTemperature = request.coreMaximumTemperatures[i];
        node.lossSourceReference = "lossData=" + region.lossDataReference +
            ";mass=" + region.coreMassSourceReference;
        if (!request.coreMaterialRevisionReference.empty())
            node.lossSourceReference += ";material=" + request.coreMaterialRevisionReference;
        node.temperatureLimitSourceReference =
            request.coreTemperatureLimitSourceReferences[i];
        networkRequest.nodes.push_back(std::move(node));
    }

    for (const auto& harmonic : request.windingCopperLosses.harmonicLosses) {
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const double watts = harmonic.phaseLoss[phase].si();
            if (!std::isfinite(watts) || watts < 0.0)
                return fail(ErrorCode::InvalidInput,
                    "Winding harmonic phase losses must be finite and non-negative.",
                    "windingCopperLosses.harmonicLosses");
            result.phaseWindingLosses[phase] =
                units::Power{result.phaseWindingLosses[phase].si() + watts};
        }
    }

    if (request.windingCopperLosses.harmonicLosses.empty())
        return fail(ErrorCode::MissingRequiredData,
            "At least one harmonic phase-loss result is required.",
            "windingCopperLosses.harmonicLosses");

    const double phaseSum = result.phaseWindingLosses[0].si() +
        result.phaseWindingLosses[1].si() + result.phaseWindingLosses[2].si();
    const double reportedTotal = request.windingCopperLosses.totalThreePhaseCopperLoss.si();
    const double tolerance = 1.0e-8 * (1.0 + std::abs(reportedTotal));
    if (!std::isfinite(reportedTotal) || reportedTotal < 0.0 ||
        std::abs(phaseSum - reportedTotal) > tolerance)
        return fail(ErrorCode::InvalidInput,
            "Summed per-phase harmonic copper losses do not match the reported three-phase total.",
            "windingCopperLosses.totalThreePhaseCopperLoss");

    for (std::size_t phase = 0; phase < windingIds.size(); ++phase) {
        ThermalNetworkNodeInput node;
        node.id = windingIds[phase];
        node.label = windingLabels[phase];
        node.loss = result.phaseWindingLosses[phase];
        node.maximumTemperature = request.windingMaximumTemperatures[phase];
        node.lossSourceReference = request.windingLossSourceReference;
        if (!request.conductorMaterialRevisionReference.empty())
            node.lossSourceReference += ";material=" + request.conductorMaterialRevisionReference;
        node.temperatureLimitSourceReference =
            request.windingTemperatureLimitSourceReferences[phase];
        networkRequest.nodes.push_back(std::move(node));
    }

    const auto solved = analyzeThermalNetwork(networkRequest);
    if (!solved)
        return Result<ReactorThermalNetworkResult>::failure(solved.error());

    result.network = solved.value();
    const bool materialsReleased =
        request.coreMaterialDesignReleaseStatus == "VERIFIED" &&
        request.conductorMaterialDesignReleaseStatus == "VERIFIED";
    if (!materialsReleased) {
        result.network.modelProvenanceComplete = false;
        for (auto& node : result.network.nodes) {
            const auto calculatedStatus = node.status;
            node.status = AssessmentStatus::NotVerified;
            node.explanation =
                "NOT_VERIFIED: material dataset design-release status is not VERIFIED; " +
                node.explanation + " (preliminary thermal comparison status=" +
                (calculatedStatus == AssessmentStatus::Pass ? "PASS" :
                 calculatedStatus == AssessmentStatus::Fail ? "FAIL" : "NOT_VERIFIED") +
                ").";
        }
    }

    return Result<ReactorThermalNetworkResult>::success(std::move(result));
}

} // namespace p135::thermal

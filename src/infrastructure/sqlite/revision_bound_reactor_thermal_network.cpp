#include "p135/infrastructure/sqlite/revision_bound_reactor_thermal_network.hpp"

#include <string>
#include <utility>

namespace p135::infrastructure::sqlite {

Result<RevisionBoundReactorThermalNetworkResult>
analyzeRevisionBoundReactorThermalNetwork(
    const SqliteMaterialRevisionStore& store,
    const std::string& coreMaterialRevisionId,
    const domain::ThreeLimbCoreLossRequest& coreLossRequest,
    const RevisionBoundWindingCopperLossRequest& windingLossRequest,
    const thermal::ReactorThermalNetworkConfiguration& thermalConfiguration)
{
    const auto coreRevision = loadCoreMaterialRevision(store, coreMaterialRevisionId);
    if (!coreRevision)
        return Result<RevisionBoundReactorThermalNetworkResult>::failure(
            coreRevision.error());

    const auto coreLosses = domain::calculateThreeLimbCoreLossPower(
        coreRevision.value().material, coreLossRequest);
    if (!coreLosses)
        return Result<RevisionBoundReactorThermalNetworkResult>::failure(
            coreLosses.error());

    const auto windingLosses = calculateRevisionBoundWindingCopperLoss(
        store, windingLossRequest);
    if (!windingLosses)
        return Result<RevisionBoundReactorThermalNetworkResult>::failure(
            windingLosses.error());

    thermal::ReactorThermalNetworkRequest thermalRequest;
    static_cast<thermal::ReactorThermalNetworkConfiguration&>(thermalRequest) =
        thermalConfiguration;
    thermalRequest.coreLosses = coreLosses.value();
    thermalRequest.windingCopperLosses = windingLosses.value().copperLoss;

    const auto& coreSnapshot = coreRevision.value();
    const auto& conductorSnapshot = windingLosses.value().conductorMaterialRevision;
    thermalRequest.coreMaterialRevisionReference =
        coreSnapshot.revisionId + "#" + coreSnapshot.contentHash;
    thermalRequest.conductorMaterialRevisionReference =
        conductorSnapshot.revisionId + "#" + conductorSnapshot.contentHash;
    thermalRequest.windingLossSourceReference =
        "revision-bound-copper-loss:" + conductorSnapshot.revisionId +
        "#" + conductorSnapshot.contentHash;
    const bool coreLossQualityAcceptable =
        coreSnapshot.coreLossStatus == "MANUFACTURER_TABULATED" ||
        coreSnapshot.coreLossStatus == "MEASURED_TABULAR";
    thermalRequest.coreMaterialDesignReleaseStatus =
        coreSnapshot.designReleaseStatus == "VERIFIED" && coreLossQualityAcceptable
            ? "VERIFIED"
            : "NOT_VERIFIED (designReleaseStatus=" + coreSnapshot.designReleaseStatus +
              "; coreLossStatus=" + coreSnapshot.coreLossStatus + ")";
    thermalRequest.conductorMaterialDesignReleaseStatus =
        conductorSnapshot.designReleaseStatus;

    const auto thermalResult = thermal::analyzeReactorThermalNetwork(thermalRequest);
    if (!thermalResult)
        return Result<RevisionBoundReactorThermalNetworkResult>::failure(
            thermalResult.error());

    RevisionBoundReactorThermalNetworkResult result;
    result.coreMaterialRevision = coreSnapshot;
    result.conductorMaterialRevision = conductorSnapshot;
    result.thermal = thermalResult.value();
    return Result<RevisionBoundReactorThermalNetworkResult>::success(
        std::move(result));
}

Result<RevisionBoundReactorThermalNetworkResult>
analyzeRevisionBoundReactorThermalNetwork(
    const SqliteMaterialRevisionStore& store,
    const std::string& coreMaterialRevisionId,
    const domain::ThreeLimbCoreLossRequest& operatingPoint,
    const RevisionBoundCoreMassInputs& coreMassInputs,
    const RevisionBoundWindingCopperLossRequest& windingLossRequest,
    const thermal::ReactorThermalNetworkConfiguration& thermalConfiguration)
{
    const auto coreRevision = loadCoreMaterialRevision(store, coreMaterialRevisionId);
    if (!coreRevision)
        return Result<RevisionBoundReactorThermalNetworkResult>::failure(
            coreRevision.error());

    domain::ThreeLimbCoreMassRequest massRequest;
    massRequest.geometry = &coreMassInputs.geometry;
    massRequest.material = &coreRevision.value().material;
    massRequest.stackingFactor = coreMassInputs.stackingFactor;
    massRequest.dimensionalSource = coreMassInputs.dimensionalSource;
    massRequest.dimensionalRevision = coreMassInputs.dimensionalRevision;
    massRequest.materialRevisionReference =
        coreRevision.value().revisionId + "#" + coreRevision.value().contentHash;
    massRequest.densitySourceReference =
        "core-material-revision:" + coreRevision.value().revisionId + "#" +
        coreRevision.value().contentHash + "/material.density";
    massRequest.stackingFactorSourceReference =
        coreMassInputs.stackingFactorSourceReference;

    const auto calculatedMass = domain::calculateThreeLimbCoreMass(massRequest);
    if (!calculatedMass)
        return Result<RevisionBoundReactorThermalNetworkResult>::failure(
            calculatedMass.error());

    const auto boundOperatingPoint = domain::bindThreeLimbCoreMass(
        operatingPoint, calculatedMass.value());
    if (!boundOperatingPoint)
        return Result<RevisionBoundReactorThermalNetworkResult>::failure(
            boundOperatingPoint.error());

    // The existing revision-bound path remains the single implementation of
    // loss-quality gating, winding copper loss and thermal-network assembly.
    auto analyzed = analyzeRevisionBoundReactorThermalNetwork(
        store, coreMaterialRevisionId, boundOperatingPoint.value(),
        windingLossRequest, thermalConfiguration);
    if (!analyzed)
        return analyzed;
    analyzed.value().calculatedCoreMass = calculatedMass.value();
    return analyzed;
}

} // namespace p135::infrastructure::sqlite

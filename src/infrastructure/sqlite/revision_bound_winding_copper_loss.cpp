#include "p135/infrastructure/sqlite/revision_bound_winding_copper_loss.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace p135::infrastructure::sqlite {
namespace {

Error bridgeError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "REVISION_BOUND_WINDING_LOSS_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

} // namespace

Result<RevisionBoundWindingCopperLossResult>
calculateRevisionBoundWindingCopperLoss(
    const SqliteMaterialRevisionStore& store,
    const RevisionBoundWindingCopperLossRequest& request)
{
    if (request.filterBranchId == 0)
        return Result<RevisionBoundWindingCopperLossResult>::failure(bridgeError(
            ErrorCode::InvalidInput, "A non-zero filter branch ID is required.",
            "filterBranchId"));

    const auto material =
        loadConductorMaterialRevision(store, request.conductorMaterialRevisionId);
    if (!material)
        return Result<RevisionBoundWindingCopperLossResult>::failure(material.error());

    std::vector<domain::WindingHarmonicCurrent> currents;
    for (const auto& point : request.harmonicAnalysis.branchPoints) {
        if (point.branchId != request.filterBranchId)
            continue;
        currents.push_back(domain::WindingHarmonicCurrent{
            point.order, point.frequency, point.phaseCurrentRms
        });
    }
    if (currents.empty())
        return Result<RevisionBoundWindingCopperLossResult>::failure(bridgeError(
            ErrorCode::MissingRequiredData,
            "No harmonic current data exists for the selected filter branch.",
            "harmonicAnalysis.branchPoints"));

    domain::WindingCopperLossRequest lossRequest;
    lossRequest.material = material.value().material;
    lossRequest.turns = request.turns;
    lossRequest.parallelConductors = request.parallelConductors;
    lossRequest.meanTurnLength = request.meanTurnLength;
    lossRequest.bareConductorCrossSection = request.bareConductorCrossSection;
    lossRequest.windingTemperature = request.windingTemperature;
    lossRequest.harmonicCurrents = std::move(currents);

    const auto loss = domain::calculateWindingCopperLoss(lossRequest);
    if (!loss)
        return Result<RevisionBoundWindingCopperLossResult>::failure(loss.error());

    RevisionBoundWindingCopperLossResult result;
    result.conductorMaterialRevision = material.value();
    result.copperLoss = loss.value();
    return Result<RevisionBoundWindingCopperLossResult>::success(std::move(result));
}

} // namespace p135::infrastructure::sqlite

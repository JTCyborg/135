#include "p135/infrastructure/sqlite/revision_bound_winding_electrothermal.hpp"

#include <utility>

namespace p135::infrastructure::sqlite {

Result<RevisionBoundWindingElectrothermalResult>
analyzeRevisionBoundWindingElectrothermal(
    const SqliteMaterialRevisionStore& store,
    const std::string& conductorMaterialRevisionId,
    const thermal::WindingElectrothermalRequest& request)
{
    const auto material =
        loadConductorMaterialRevision(store, conductorMaterialRevisionId);
    if (!material)
        return Result<RevisionBoundWindingElectrothermalResult>::failure(material.error());

    auto boundRequest = request;
    boundRequest.electrical.material = material.value().material;
    boundRequest.materialRevisionReference =
        material.value().revisionId + "#" + material.value().contentHash;

    const auto analysis = thermal::analyzeWindingElectrothermal(boundRequest);
    if (!analysis)
        return Result<RevisionBoundWindingElectrothermalResult>::failure(analysis.error());

    RevisionBoundWindingElectrothermalResult result;
    result.conductorMaterialRevision = material.value();
    result.electrothermal = analysis.value();

    // A converged thermal estimate is not a design release. Keep the result
    // NOT_VERIFIED until the immutable material revision itself is released.
    if (result.electrothermal.converged &&
        material.value().designReleaseStatus != "VERIFIED") {
        result.electrothermal.status = thermal::AssessmentStatus::NotVerified;
        result.electrothermal.explanation =
            "NOT_VERIFIED: thermal iteration converged, but the selected conductor material revision is not design-released (" +
            material.value().designReleaseStatus + "). " +
            result.electrothermal.explanation;
    }
    return Result<RevisionBoundWindingElectrothermalResult>::success(std::move(result));
}

} // namespace p135::infrastructure::sqlite

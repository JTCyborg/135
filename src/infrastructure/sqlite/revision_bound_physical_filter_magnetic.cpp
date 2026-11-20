#include "p135/infrastructure/sqlite/revision_bound_physical_filter_magnetic.hpp"

#include <utility>

namespace p135::infrastructure::sqlite {

Result<RevisionBoundPhysicalFilterMagneticResult>
analyzePhysicalFilterReactorMagneticCycleWithMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& materialRevisionId,
    const application::PhysicalFilterReactorMagneticRequest& request)
{
    const auto material = loadCoreMaterialRevision(store, materialRevisionId);
    if (!material)
        return Result<RevisionBoundPhysicalFilterMagneticResult>::failure(material.error());

    auto boundRequest = request;
    boundRequest.filterMagnetic.magnetic.material = material.value().material;

    const auto analysis = application::analyzePhysicalFilterReactorMagneticCycle(boundRequest);
    if (!analysis)
        return Result<RevisionBoundPhysicalFilterMagneticResult>::failure(analysis.error());

    RevisionBoundPhysicalFilterMagneticResult result;
    result.materialRevision = material.value();
    result.magneticAnalysis = analysis.value();
    return Result<RevisionBoundPhysicalFilterMagneticResult>::success(std::move(result));
}

} // namespace p135::infrastructure::sqlite

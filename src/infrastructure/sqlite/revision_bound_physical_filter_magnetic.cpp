#include "p135/infrastructure/sqlite/revision_bound_physical_filter_magnetic.hpp"

#include <utility>
#include <string>

namespace p135::infrastructure::sqlite {
namespace {

Error materialQualityError(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::MissingMaterialProperty;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CORE_MATERIAL_BH_QUALITY_NOT_VERIFIED", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

bool hasVerifiedBhCurveStatus(const std::string& status)
{
    // Only source-tabulated manufacturer curves or traceable measurement
    // datasets qualify. Digitized typical graphs and missing datasets remain
    // useful for exploration, but are not accepted for a verified design run.
    return status == "MANUFACTURER_TABULATED" ||
           status == "MEASURED_TABULAR";
}

} // namespace

Result<RevisionBoundPhysicalFilterMagneticResult>
analyzePhysicalFilterReactorMagneticCycleWithMaterialRevision(
    const SqliteMaterialRevisionStore& store,
    const std::string& materialRevisionId,
    const application::PhysicalFilterReactorMagneticRequest& request)
{
    const auto material = loadCoreMaterialRevision(store, materialRevisionId);
    if (!material)
        return Result<RevisionBoundPhysicalFilterMagneticResult>::failure(material.error());

    const bool isEstimatedBhModel =
        material.value().bhCurveModelStatus == "ESTIMATED_FROM_SPARSE_MAGNETIC_ANCHORS" ||
        material.value().bhCurveModelStatus == "ESTIMATED_FROM_SATURATION_PARAMETERS";
    const bool estimatedModelAllowed = request.allowEstimatedBhModel && isEstimatedBhModel;
    if (!hasVerifiedBhCurveStatus(material.value().bhCurveStatus) && !estimatedModelAllowed) {
        return Result<RevisionBoundPhysicalFilterMagneticResult>::failure(
            materialQualityError(
                "The selected material revision does not contain a verified tabulated B-H curve "
                "(bhCurveStatus=" + material.value().bhCurveStatus +
                "). Approximate digitization or missing B-H data cannot be used for a verified "
                "magnetic design calculation. Enable allowEstimatedBhModel only for exploratory use; "
                "the generated branch remains NOT_VERIFIED.",
                "materialRevision.bhCurveStatus"));
    }

    auto boundRequest = request;
    boundRequest.filterMagnetic.magnetic.material = material.value().material;

    const auto analysis = application::analyzePhysicalFilterReactorMagneticCycle(boundRequest);
    if (!analysis)
        return Result<RevisionBoundPhysicalFilterMagneticResult>::failure(analysis.error());

    if (!analysis.value().magneticAnalysis.magneticCycle.withinBhCurveRange) {
        Error error;
        error.code = ErrorCode::ModelOutOfDomain;
        error.message =
            "At least one limb or yoke operating point exceeds the selected revision's "
            "tabulated B-H range. The solver's permeability asymptote is exploratory only "
            "and cannot be accepted as a verified material result.";
        error.diagnostics.push_back(Diagnostic{
            Severity::Error, "CORE_MATERIAL_BH_RANGE_EXCEEDED", error.message,
            "magneticAnalysis.magneticCycle.withinBhCurveRange",
            std::nullopt, std::nullopt
        });
        return Result<RevisionBoundPhysicalFilterMagneticResult>::failure(std::move(error));
    }

    RevisionBoundPhysicalFilterMagneticResult result;
    result.materialRevision = material.value();
    result.magneticAnalysis = analysis.value();
    return Result<RevisionBoundPhysicalFilterMagneticResult>::success(std::move(result));
}

} // namespace p135::infrastructure::sqlite

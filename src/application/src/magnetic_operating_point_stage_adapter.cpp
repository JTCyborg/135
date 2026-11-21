#include "p135/application/magnetic_operating_point_stage_adapter.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace p135::application {
namespace {
Error invalidArtifact(const char* message)
{
    return {ErrorCode::InvalidInput, message, {}};
}

bool positiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}
}

Result<std::string> makeMagneticOperatingPointStageArtifact(
    const PhysicalFilterReactorMagneticRequest& request,
    const PhysicalFilterReactorMagneticResult& result)
{
    const auto physicalValidation = domain::validate(request.physicalCore);
    if (!physicalValidation)
        return Result<std::string>::failure(physicalValidation.error());

    if (!positiveFinite(request.stackingFactor) || request.stackingFactor > 1.0 ||
        request.filterMagnetic.filterBranchId == 0 ||
        request.dimensionalSource.empty() || request.dimensionalRevision.empty() ||
        result.magneticAnalysis.filterBranchId != request.filterMagnetic.filterBranchId ||
        request.filterMagnetic.magnetic.material.id.empty() ||
        request.filterMagnetic.magnetic.material.sourceReference.empty() ||
        request.filterMagnetic.magnetic.material.datasetRevision.empty())
        return Result<std::string>::failure(invalidArtifact(
            "Magnetic operating point requires a valid stacking factor, matching filter branch and revision-bound B-H material."));

    const auto& cycle = result.magneticAnalysis.magneticCycle;
    if (!cycle.converged || !cycle.withinBhCurveRange || cycle.samplesUsed < 72 ||
        !std::isfinite(cycle.relativePeakChange) || cycle.relativePeakChange < 0.0)
        return Result<std::string>::failure(invalidArtifact(
            "Magnetic operating point is NOT_VERIFIED: cycle must converge within the measured B-H range."));

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    output << "schema=project135.magnetic-operating-point.v1\n";
    output << "filter_branch_id=" << result.magneticAnalysis.filterBranchId << '\n';
    output << "dimensional_source=" << request.dimensionalSource << '\n';
    output << "dimensional_revision=" << request.dimensionalRevision << '\n';
    output << "stacking_factor=" << request.stackingFactor << '\n';
    output << "core_material_id=" << request.filterMagnetic.magnetic.material.id << '\n';
    output << "bh_source_reference=" << request.filterMagnetic.magnetic.material.sourceReference << '\n';
    output << "bh_dataset_revision=" << request.filterMagnetic.magnetic.material.datasetRevision << '\n';
    output << "samples_used=" << cycle.samplesUsed << '\n';
    output << "relative_peak_change=" << cycle.relativePeakChange << '\n';
    output << "converged=true\nwithin_bh_curve_range=true\n";
    output << "excitation_count=" << result.magneticAnalysis.excitations.size() << '\n';
    for (std::size_t i = 0; i < result.magneticAnalysis.excitations.size(); ++i) {
        const auto& excitation = result.magneticAnalysis.excitations[i];
        if (excitation.order == 0)
            return Result<std::string>::failure(invalidArtifact(
                "Magnetic operating point contains a zero-order AC excitation."));
        output << "excitation." << i << ".order=" << excitation.order << '\n';
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const auto value = excitation.phaseRmsPhasors[phase];
            if (!std::isfinite(value.real()) || !std::isfinite(value.imag()))
                return Result<std::string>::failure(invalidArtifact(
                    "Magnetic operating point contains a non-finite harmonic phasor."));
            output << "excitation." << i << ".phase." << phase
                   << ".rms_real_A=" << value.real() << '\n';
            output << "excitation." << i << ".phase." << phase
                   << ".rms_imag_A=" << value.imag() << '\n';
        }
    }
    for (std::size_t i = 0; i < cycle.peakAbsoluteFluxDensity.size(); ++i) {
        const double peak = cycle.peakAbsoluteFluxDensity[i].si();
        if (!positiveFinite(peak))
            return Result<std::string>::failure(invalidArtifact(
                "Magnetic operating point contains a non-positive or non-finite peak flux density."));
        output << "peak_absolute_flux_density_" << i << "_T=" << peak << '\n';
    }
    return Result<std::string>::success(output.str());
}

} // namespace p135::application

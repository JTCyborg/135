#include "p135/application/filter_reactor_magnetic.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

namespace p135::application {
namespace {
Error invalid(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "FILTER_REACTOR_MAGNETIC_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}
}

Result<FilterReactorMagneticResult>
analyzeFilterReactorMagneticCycle(const FilterReactorMagneticRequest& request)
{
    if (request.filterBranchId == 0)
        return Result<FilterReactorMagneticResult>::failure(
            invalid("A non-zero filter branch ID is required.", "filterBranchId"));

    const auto& points = request.harmonicAnalysis.branchPoints;
    std::unordered_map<std::uint32_t, std::array<std::complex<double>, 3>> byOrder;
    for (const auto& point : points) {
        if (point.branchId != request.filterBranchId)
            continue;
        if (point.order == 0 || !std::isfinite(point.frequency.si()) ||
            point.frequency.si() <= 0.0)
            return Result<FilterReactorMagneticResult>::failure(
                invalid("Selected branch contains an invalid harmonic order or frequency.",
                        "harmonicAnalysis.branchPoints"));
        for (const auto& current : point.phaseCurrentRms) {
            if (!std::isfinite(current.real()) || !std::isfinite(current.imag()))
                return Result<FilterReactorMagneticResult>::failure(
                    invalid("Selected branch contains a non-finite phase-current phasor.",
                            "harmonicAnalysis.branchPoints"));
        }
        if (!byOrder.emplace(point.order, point.phaseCurrentRms).second)
            return Result<FilterReactorMagneticResult>::failure(
                invalid("Selected branch contains duplicate harmonic orders.",
                        "harmonicAnalysis.branchPoints"));
    }

    if (byOrder.empty())
        return Result<FilterReactorMagneticResult>::failure(
            invalid("No per-phase harmonic current data exists for the selected filter branch.",
                    "filterBranchId"));
    if (!byOrder.contains(1))
        return Result<FilterReactorMagneticResult>::failure(
            invalid("Selected filter branch has no fundamental current; magnetic cycle sampling requires order 1.",
                    "harmonicAnalysis.branchPoints"));

    FilterReactorMagneticResult result;
    result.filterBranchId = request.filterBranchId;
    result.excitations.reserve(byOrder.size());
    for (const auto& point : points) {
        if (point.branchId != request.filterBranchId)
            continue;
        result.excitations.push_back(domain::ThreeLimbMagneticHarmonicExcitation{
            point.order, point.phaseCurrentRms
        });
    }
    std::sort(result.excitations.begin(), result.excitations.end(),
        [](const auto& lhs, const auto& rhs) { return lhs.order < rhs.order; });

    domain::ThreeLimbMagneticCycleSamplingRequest cycleRequest;
    cycleRequest.magnetic = request.magnetic;
    cycleRequest.harmonicExcitations = result.excitations;
    cycleRequest.relativePeakTolerance = request.relativePeakTolerance;
    cycleRequest.maximumSamples = request.maximumSamples;

    const auto magnetic = domain::sampleThreeLimbMagneticCycle(cycleRequest);
    if (!magnetic)
        return Result<FilterReactorMagneticResult>::failure(magnetic.error());
    result.magneticCycle = magnetic.value();
    return Result<FilterReactorMagneticResult>::success(std::move(result));
}

} // namespace p135::application

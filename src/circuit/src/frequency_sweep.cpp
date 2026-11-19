#include "p135/circuit/solver.hpp"

#include <cmath>
#include <utility>

namespace p135::circuit {
namespace {

Error makeSweepError(ErrorCode code, std::string message) {
    return Error{code, std::move(message), {}};
}

} // namespace

Result<FrequencyResponse> sweep(const FrequencySweep& request,
                                const CancellationToken& cancellation) {
    if (request.study.analysis != AnalysisKind::AcSteadyState)
        return Result<FrequencyResponse>::failure(
            makeSweepError(ErrorCode::InvalidInput,
                           "Frequency sweep requires an AC steady-state study."));

    const double start = request.startFrequency.si();
    const double stop = request.stopFrequency.si();
    if (!std::isfinite(start) || !std::isfinite(stop) ||
        start <= 0.0 || stop <= 0.0 || stop < start) {
        return Result<FrequencyResponse>::failure(
            makeSweepError(ErrorCode::InvalidInput,
                           "Sweep frequencies must be finite, positive, and stop >= start."));
    }
    if (request.points < 2 || request.points > 100000) {
        return Result<FrequencyResponse>::failure(
            makeSweepError(ErrorCode::InvalidInput,
                           "Sweep point count must be between 2 and 100000."));
    }

    FrequencyResponse response;
    response.points.reserve(request.points);

    const double logStart = std::log(start);
    const double logStop = std::log(stop);
    for (std::size_t i = 0; i < request.points; ++i) {
        if (cancellation.isCancellationRequested())
            return Result<FrequencyResponse>::failure(
                makeSweepError(ErrorCode::Cancelled, "Frequency sweep cancelled."));

        const double fraction = static_cast<double>(i) /
                                static_cast<double>(request.points - 1);
        const double frequency = request.spacing == SweepSpacing::Linear
            ? start + (stop - start) * fraction
            : std::exp(logStart + (logStop - logStart) * fraction);

        Study pointStudy = request.study;
        pointStudy.frequency = units::Frequency{frequency};
        auto pointResult = solve(pointStudy, cancellation);
        if (!pointResult)
            return Result<FrequencyResponse>::failure(pointResult.error());

        response.points.push_back(
            FrequencySweepPoint{units::Frequency{frequency}, pointResult.value()});
    }

    return Result<FrequencyResponse>::success(std::move(response));
}

} // namespace p135::circuit

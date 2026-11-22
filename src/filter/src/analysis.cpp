#include "p135/filter/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace p135::filter {
namespace {

using Complex = std::complex<double>;
constexpr double kTwoPi = 6.283185307179586476925286766559;

Error invalid(std::string message)
{
    return Error{ErrorCode::InvalidInput, std::move(message), {}};
}

bool finitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool finiteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool finite(Complex value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

Result<Complex> parallelImpedance(Complex first, Complex second)
{
    const Complex denominator = first + second;
    if (!finite(denominator) || std::abs(denominator) <=
        std::numeric_limits<double>::min()) {
        return Result<Complex>::failure(
            invalid("Parallel impedance is undefined for the supplied network."));
    }
    const Complex value = first * second / denominator;
    if (!finite(value))
        return Result<Complex>::failure(
            invalid("Calculated impedance is outside the supported numeric domain."));
    return Result<Complex>::success(value);
}

} // namespace

Result<FrequencyResponseResult> analyzeFrequencyResponse(
    const FrequencyResponseRequest& request)
{
    const double fundamental = request.design.network.fundamentalFrequency.si();
    const double sourceResistance = request.source.resistance.si();
    const double sourceInductance = request.source.inductance.si();
    const double start = request.startFrequency.si();
    const double stop = request.stopFrequency.si();

    if (!finitePositive(fundamental))
        return Result<FrequencyResponseResult>::failure(
            invalid("Filter fundamental frequency must be finite and positive."));
    if (request.design.network.connection != Connection::Wye &&
        request.design.network.connection != Connection::Delta)
        return Result<FrequencyResponseResult>::failure(
            invalid("Unsupported filter connection."));
    if (!finiteNonNegative(sourceResistance) ||
        !finiteNonNegative(sourceInductance))
        return Result<FrequencyResponseResult>::failure(
            invalid("Source resistance and inductance must be finite and non-negative."));
    if (!finitePositive(start) || !finitePositive(stop) || stop < start)
        return Result<FrequencyResponseResult>::failure(
            invalid("Scan frequencies must be finite, positive, and stop >= start."));
    if (request.points < 3 || request.points > 100000)
        return Result<FrequencyResponseResult>::failure(
            invalid("Frequency scan point count must be between 3 and 100000."));
    if (request.design.branches.empty())
        return Result<FrequencyResponseResult>::failure(
            invalid("Filter design must contain at least one branch."));

    std::unordered_set<std::uint32_t> branchIds;
    for (const auto& branch : request.design.branches) {
        if (branch.id == 0 || !branchIds.insert(branch.id).second ||
            !finitePositive(branch.capacitance.si()) ||
            !finitePositive(branch.inductance.si()) ||
            !finitePositive(branch.dampingResistance.si())) {
            return Result<FrequencyResponseResult>::failure(
                invalid("Filter design contains an invalid or duplicate branch."));
        }
    }

    FrequencyResponseResult response;
    response.points.reserve(request.points);

    const double logStart = std::log(start);
    const double logStop = std::log(stop);
    for (std::size_t index = 0; index < request.points; ++index) {
        const double fraction = static_cast<double>(index) /
                                static_cast<double>(request.points - 1);
        // Preserve user-specified endpoints exactly; exp(log(start)) can
        // otherwise differ from start by a few ULPs.
        const double frequency = index == 0 ? start
            : (index + 1 == request.points ? stop
                : (request.logarithmicSpacing
                    ? std::exp(logStart + (logStop - logStart) * fraction)
                    : start + (stop - start) * fraction));
        const double omega = kTwoPi * frequency;

        Complex bankAdmittance{};
        for (const auto& branch : request.design.branches) {
            const double resistance = branch.dampingResistance.si();
            const double inductiveReactance = omega * branch.inductance.si();
            const double capacitiveReactance =
                1.0 / (omega * branch.capacitance.si());
            const Complex branchImpedance{
                resistance, inductiveReactance - capacitiveReactance
            };
            if (!finite(branchImpedance) ||
                std::abs(branchImpedance) <= std::numeric_limits<double>::min()) {
                return Result<FrequencyResponseResult>::failure(
                    invalid("A branch impedance is undefined at a scan frequency."));
            }
            bankAdmittance += 1.0 / branchImpedance;
        }

        // A balanced Delta bank has a per-phase Y-equivalent with
        // three times the branch-bank admittance (Z_Y = Z_Delta / 3).
        if (request.design.network.connection == Connection::Delta)
            bankAdmittance *= 3.0;

        if (!finite(bankAdmittance) ||
            std::abs(bankAdmittance) <= std::numeric_limits<double>::min()) {
            return Result<FrequencyResponseResult>::failure(
                invalid("Filter bank impedance is outside the supported numeric domain."));
        }

        const Complex filterImpedance = 1.0 / bankAdmittance;
        const Complex sourceImpedance{
            sourceResistance, omega * sourceInductance
        };
        const auto busResult = parallelImpedance(filterImpedance, sourceImpedance);
        if (!busResult)
            return Result<FrequencyResponseResult>::failure(busResult.error());

        const Complex busImpedance = busResult.value();
        response.points.push_back(FrequencyResponsePoint{
            units::Frequency{frequency},
            std::abs(filterImpedance),
            std::abs(sourceImpedance),
            std::abs(busImpedance),
            std::arg(busImpedance)
        });
    }

    for (std::size_t index = 1; index + 1 < response.points.size(); ++index) {
        const double previous =
            response.points[index - 1].busImpedanceMagnitudeOhm;
        const double current =
            response.points[index].busImpedanceMagnitudeOhm;
        const double next =
            response.points[index + 1].busImpedanceMagnitudeOhm;
        if (current > previous && current >= next) {
            response.resonancePeaks.push_back(ResonancePeak{
                response.points[index].frequency, current
            });
        }
    }

    return Result<FrequencyResponseResult>::success(std::move(response));
}

} // namespace p135::filter

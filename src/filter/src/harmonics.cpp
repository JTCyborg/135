#include "p135/filter/harmonics.hpp"

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
constexpr double kSqrtThree = 1.7320508075688772935274463415059;

Error invalid(std::string message)
{
    return Error{ErrorCode::InvalidInput, std::move(message), {}};
}

bool positiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool nonNegativeFinite(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool finite(Complex value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

} // namespace

Result<HarmonicAnalysisResult> analyzeHarmonics(
    const HarmonicAnalysisRequest& request)
{
    const double fundamental = request.design.network.fundamentalFrequency.si();
    const double sourceR = request.source.resistance.si();
    const double sourceL = request.source.inductance.si();

    if (!positiveFinite(fundamental))
        return Result<HarmonicAnalysisResult>::failure(
            invalid("Fundamental frequency must be finite and positive."));
    if (request.design.network.connection != Connection::Wye &&
        request.design.network.connection != Connection::Delta)
        return Result<HarmonicAnalysisResult>::failure(
            invalid("Unsupported filter connection."));
    if (!nonNegativeFinite(sourceR) || !nonNegativeFinite(sourceL) ||
        (sourceR == 0.0 && sourceL == 0.0))
        return Result<HarmonicAnalysisResult>::failure(
            invalid("Source resistance and inductance must be finite, non-negative, and not both zero."));
    if (request.design.branches.empty() || request.sourceVoltageSpectrum.empty())
        return Result<HarmonicAnalysisResult>::failure(
            invalid("At least one filter branch and one source harmonic are required."));

    std::unordered_set<std::uint32_t> branchIds;
    for (const auto& branch : request.design.branches) {
        if (branch.id == 0 || !branchIds.insert(branch.id).second ||
            !positiveFinite(branch.capacitance.si()) ||
            !positiveFinite(branch.inductance.si()) ||
            !positiveFinite(branch.dampingResistance.si()))
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Filter design contains an invalid or duplicate branch."));
    }

    std::unordered_set<std::uint32_t> harmonicOrders;
    bool hasFundamental = false;
    for (const auto& harmonic : request.sourceVoltageSpectrum) {
        if (harmonic.order == 0 || !harmonicOrders.insert(harmonic.order).second ||
            !finite(harmonic.phaseVoltageRms) ||
            (harmonic.sequence != HarmonicSequence::Positive &&
             harmonic.sequence != HarmonicSequence::Negative))
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Harmonic orders must be unique and positive, sequence must be positive or negative, and voltage phasors must be finite."));
        hasFundamental = hasFundamental || harmonic.order == 1;
    }
    if (!hasFundamental)
        return Result<HarmonicAnalysisResult>::failure(
            invalid("Source voltage spectrum must contain the fundamental (order 1)."));

    std::unordered_set<std::string> loadSourceIds;
    for (const auto& injection : request.loadCurrentInjections) {
        if (injection.sourceId.empty() || !loadSourceIds.insert(injection.sourceId).second ||
            injection.order == 0 || !finite(injection.phaseCurrentRms) ||
            (injection.sequence != HarmonicSequence::Positive &&
             injection.sequence != HarmonicSequence::Negative))
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Load-current injections require unique non-empty source IDs, positive orders, valid sequences and finite phasors."));
        const bool matchingVoltageOrder = std::any_of(
            request.sourceVoltageSpectrum.begin(), request.sourceVoltageSpectrum.end(),
            [&injection](const auto& voltage) {
                return voltage.order == injection.order && voltage.sequence == injection.sequence;
            });
        if (!matchingVoltageOrder)
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Every load-current injection order/sequence must exist in the voltage spectrum; use a zero voltage phasor if the grid voltage harmonic is absent."));
    }

    HarmonicAnalysisResult result;
    result.points.reserve(request.sourceVoltageSpectrum.size());
    result.branchPoints.reserve(request.sourceVoltageSpectrum.size() * request.design.branches.size());
    result.branchStresses.reserve(request.design.branches.size());

    std::vector<double> branchCurrentSquared(request.design.branches.size(), 0.0);
    std::vector<double> capacitorVoltageSquared(request.design.branches.size(), 0.0);
    std::vector<double> inductorVoltageSquared(request.design.branches.size(), 0.0);
    std::vector<double> resistorVoltageSquared(request.design.branches.size(), 0.0);
    std::vector<double> resistorLoss(request.design.branches.size(), 0.0);
    double harmonicBusVoltageSquared = 0.0;
    double harmonicSourceCurrentSquared = 0.0;
    double fundamentalBusVoltage = 0.0;
    double fundamentalSourceCurrent = 0.0;

    for (const auto& harmonic : request.sourceVoltageSpectrum) {
        const double frequency = fundamental * static_cast<double>(harmonic.order);
        const double omega = kTwoPi * frequency;
        if (!positiveFinite(frequency) || !positiveFinite(omega))
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Harmonic frequency is outside the supported numeric domain."));

        Complex bankAdmittance{};
        std::vector<Complex> branchImpedances;
        branchImpedances.reserve(request.design.branches.size());
        for (const auto& branch : request.design.branches) {
            const double r = branch.dampingResistance.si();
            const double xl = omega * branch.inductance.si();
            const double xc = 1.0 / (omega * branch.capacitance.si());
            const Complex z{r, xl - xc};
            if (!finite(z) || std::abs(z) <= std::numeric_limits<double>::min())
                return Result<HarmonicAnalysisResult>::failure(
                    invalid("A branch impedance is outside the supported numeric domain."));
            branchImpedances.push_back(z);
            bankAdmittance += 1.0 / z;
        }

        // Convert a balanced Delta bank to its per-phase Y equivalent.
        if (request.design.network.connection == Connection::Delta)
            bankAdmittance *= 3.0;
        if (!finite(bankAdmittance) ||
            std::abs(bankAdmittance) <= std::numeric_limits<double>::min())
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Filter bank admittance is outside the supported numeric domain."));

        // For a balanced, transposed network the positive- and negative-sequence
        // per-phase driving-point impedances are identical in this scalar model.
        // Sequence is retained in the input contract for traceability; zero sequence
        // is intentionally unsupported because its path depends on grounding/topology.
        const Complex sourceImpedance{sourceR, omega * sourceL};
        const Complex divider = Complex{1.0, 0.0} + sourceImpedance * bankAdmittance;
        if (!finite(divider) || std::abs(divider) <= std::numeric_limits<double>::min())
            return Result<HarmonicAnalysisResult>::failure(
                invalid("The source/filter voltage divider is singular."));
        Complex loadCurrent{};
        for (const auto& injection : request.loadCurrentInjections) {
            if (injection.order == harmonic.order && injection.sequence == harmonic.sequence)
                loadCurrent += injection.phaseCurrentRms;
        }
        // KCL at the PCC: (Vs - Vbus)/Zs = Yfilter*Vbus + Iload.
        // The load current is a sink from the bus; its phasor is therefore
        // subtracted from the Thevenin voltage before applying the divider.
        const Complex busVoltage =
            (harmonic.phaseVoltageRms - sourceImpedance * loadCurrent) / divider;
        const Complex sourceCurrent =
            (harmonic.phaseVoltageRms - busVoltage) / sourceImpedance;
        const Complex filterCurrent = busVoltage * bankAdmittance;
        if (!finite(busVoltage) || !finite(sourceCurrent) || !finite(filterCurrent) || !finite(loadCurrent))
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Calculated harmonic response is outside the supported numeric domain."));

        const double busMagnitude = std::abs(busVoltage);
        const double sourceCurrentMagnitude = std::abs(sourceCurrent);
        if (harmonic.order == 1) {
            fundamentalBusVoltage = busMagnitude;
            fundamentalSourceCurrent = sourceCurrentMagnitude;
        } else {
            harmonicBusVoltageSquared += busMagnitude * busMagnitude;
            harmonicSourceCurrentSquared +=
                sourceCurrentMagnitude * sourceCurrentMagnitude;
        }

        result.points.push_back(HarmonicAnalysisPoint{
            harmonic.order,
            units::Frequency{frequency},
            std::abs(harmonic.phaseVoltageRms),
            busMagnitude,
            std::abs(filterCurrent),
            sourceCurrentMagnitude,
            std::abs(loadCurrent),
            1.0 / std::abs(bankAdmittance)
        });

        const double deltaBranchFactor =
            request.design.network.connection == Connection::Delta ? kSqrtThree : 1.0;
        for (std::size_t index = 0; index < request.design.branches.size(); ++index) {
            const auto& branch = request.design.branches[index];
            const Complex branchCurrent =
                deltaBranchFactor * busVoltage / branchImpedances[index];
            const double currentMagnitude = std::abs(branchCurrent);
            result.branchPoints.push_back(BranchHarmonicPoint{
                branch.id, harmonic.order, units::Frequency{frequency}, currentMagnitude
            });
            const double capacitorVoltage = currentMagnitude /
                (omega * branch.capacitance.si());
            const double inductorVoltage = currentMagnitude *
                omega * branch.inductance.si();
            const double resistorVoltage =
                currentMagnitude * branch.dampingResistance.si();

            branchCurrentSquared[index] += currentMagnitude * currentMagnitude;
            capacitorVoltageSquared[index] += capacitorVoltage * capacitorVoltage;
            inductorVoltageSquared[index] += inductorVoltage * inductorVoltage;
            resistorVoltageSquared[index] += resistorVoltage * resistorVoltage;
            resistorLoss[index] += currentMagnitude * currentMagnitude *
                                   branch.dampingResistance.si();

            if (!std::isfinite(branchCurrentSquared[index]) ||
                !std::isfinite(capacitorVoltageSquared[index]) ||
                !std::isfinite(inductorVoltageSquared[index]) ||
                !std::isfinite(resistorVoltageSquared[index]) ||
                !std::isfinite(resistorLoss[index]))
                return Result<HarmonicAnalysisResult>::failure(
                    invalid("Aggregated component stress is outside the supported numeric domain."));
        }
        if (!std::isfinite(harmonicBusVoltageSquared) ||
            !std::isfinite(harmonicSourceCurrentSquared))
            return Result<HarmonicAnalysisResult>::failure(
                invalid("Aggregated harmonic distortion is outside the supported numeric domain."));
    }

    if (!positiveFinite(fundamentalBusVoltage) ||
        !positiveFinite(fundamentalSourceCurrent))
        return Result<HarmonicAnalysisResult>::failure(
            invalid("Fundamental bus voltage and source current must be non-zero to calculate THD."));

    result.busFundamentalVoltageV = fundamentalBusVoltage;
    result.sourceFundamentalCurrentA = fundamentalSourceCurrent;
    result.busVoltageThd = std::sqrt(harmonicBusVoltageSquared) /
                           fundamentalBusVoltage;
    result.sourceCurrentThd = std::sqrt(harmonicSourceCurrentSquared) /
                              fundamentalSourceCurrent;
    if (!std::isfinite(result.busVoltageThd) ||
        !std::isfinite(result.sourceCurrentThd))
        return Result<HarmonicAnalysisResult>::failure(
            invalid("Calculated THD is outside the supported numeric domain."));

    for (std::size_t index = 0; index < request.design.branches.size(); ++index) {
        result.branchStresses.push_back(BranchHarmonicStress{
            request.design.branches[index].id,
            std::sqrt(branchCurrentSquared[index]),
            std::sqrt(capacitorVoltageSquared[index]),
            std::sqrt(inductorVoltageSquared[index]),
            std::sqrt(resistorVoltageSquared[index]),
            resistorLoss[index]
        });
    }

    return Result<HarmonicAnalysisResult>::success(std::move(result));
}

} // namespace p135::filter

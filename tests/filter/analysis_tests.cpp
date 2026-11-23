#include "p135/filter/analysis.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <complex>
#include <cmath>
#include <numbers>

using namespace p135;
using namespace p135::filter;

namespace {

FilterDesign makeDesign()
{
    FilterSpecification specification;
    specification.network.lineToLineVoltage = units::Voltage{400.0};
    specification.network.fundamentalFrequency = units::Frequency{50.0};
    specification.network.connection = Connection::Wye;
    specification.branches.push_back({
        1, "5th harmonic", 5, units::Power{100000.0}, 50.0
    });
    const auto result = synthesize(specification);
    assert(result);
    return result.value();
}

FrequencyResponseRequest validRequest()
{
    FrequencyResponseRequest request;
    request.design = makeDesign();
    request.source = {units::Resistance{0.08}, units::Inductance{0.001}};
    request.startFrequency = units::Frequency{50.0};
    request.stopFrequency = units::Frequency{1000.0};
    request.points = 2001;
    return request;
}

} // namespace

int main()
{
    {
        const auto request = validRequest();
        const auto result = analyzeFrequencyResponse(request);
        assert(result);
        assert(result.value().points.size() == 2001);
        assert(result.value().points.front().frequency.si() == 50.0);
        assert(std::abs(result.value().points.back().frequency.si() - 1000.0) < 1.0e-9);
        assert(result.value().points.front().filterImpedanceMagnitudeOhm > 0.0);
        const double expectedSourceMagnitude = std::hypot(
            0.08, 2.0 * std::numbers::pi * 50.0 * 0.001);
        assert(std::abs(result.value().points.front().sourceImpedanceMagnitudeOhm -
                        expectedSourceMagnitude) < 1.0e-12);
        assert(result.value().points.front().busImpedanceMagnitudeOhm > 0.0);
        assert(std::isfinite(result.value().points.front().busImpedancePhaseRadians));
        const auto& branch = request.design.branches.front();
        const double omega = 2.0 * std::numbers::pi * 50.0;
        const std::complex<double> sourceZ{0.08, omega * 0.001};
        const std::complex<double> filterZ{
            branch.dampingResistance.si(),
            omega * branch.inductance.si() - 1.0 / (omega * branch.capacitance.si())
        };
        const double expectedTransfer = 1.0 / std::abs(1.0 + sourceZ / filterZ);
        assert(std::abs(result.value().points.front().gridCurrentTransferMagnitudeRatio -
                        expectedTransfer) < 1.0e-12);
        assert(!result.value().resonancePeaks.empty());

        const auto strongest = std::max_element(
            result.value().resonancePeaks.begin(), result.value().resonancePeaks.end(),
            [](const ResonancePeak& left, const ResonancePeak& right) {
                return left.busImpedanceMagnitudeOhm < right.busImpedanceMagnitudeOhm;
            });
        assert(strongest != result.value().resonancePeaks.end());
        assert(strongest->frequency.si() > 50.0);
        assert(strongest->frequency.si() < 1000.0);
    }

    {
        auto wyeRequest = validRequest();
        wyeRequest.startFrequency = units::Frequency{100.0};
        wyeRequest.stopFrequency = units::Frequency{100.0};
        wyeRequest.points = 3;
        const auto wye = analyzeFrequencyResponse(wyeRequest);
        assert(wye);

        auto deltaRequest = wyeRequest;
        deltaRequest.design.network.connection = Connection::Delta;
        const auto delta = analyzeFrequencyResponse(deltaRequest);
        assert(delta);
        assert(std::abs(delta.value().points[1].filterImpedanceMagnitudeOhm * 3.0 -
                        wye.value().points[1].filterImpedanceMagnitudeOhm) < 1.0e-9);
    }

    {
        auto request = validRequest();
        request.logarithmicSpacing = false;
        request.points = 101;
        const auto result = analyzeFrequencyResponse(request);
        assert(result);
        assert(std::abs(result.value().points[50].frequency.si() - 525.0) < 1.0e-9);
    }

    {
        // Source R and L are independently represented in the frequency scan.
        auto resistiveRequest = validRequest();
        resistiveRequest.source = {units::Resistance{0.08}, units::Inductance{0.0}};
        const auto resistive = analyzeFrequencyResponse(resistiveRequest);
        assert(resistive);
        assert(std::abs(resistive.value().points.front().sourceImpedanceMagnitudeOhm -
                        0.08) < 1.0e-12);

        auto inductiveRequest = validRequest();
        inductiveRequest.source = {units::Resistance{0.0}, units::Inductance{0.001}};
        const auto inductive = analyzeFrequencyResponse(inductiveRequest);
        assert(inductive);
        const double expectedInductiveMagnitude =
            2.0 * std::numbers::pi * 50.0 * 0.001;
        assert(std::abs(inductive.value().points.front().sourceImpedanceMagnitudeOhm -
                        expectedInductiveMagnitude) < 1.0e-12);
        assert(std::abs(resistive.value().points.front().busImpedanceMagnitudeOhm -
                        inductive.value().points.front().busImpedanceMagnitudeOhm) > 1.0e-9);
    }
    {
        // An ideal source has zero Thevenin impedance, but the filter-bank
        // driving-point impedance remains a valid and useful frequency curve.
        auto request = validRequest();
        request.source = {units::Resistance{0.0}, units::Inductance{0.0}};
        const auto idealSource = analyzeFrequencyResponse(request);
        assert(idealSource);
        assert(idealSource.value().points.size() == request.points);
        assert(idealSource.value().points.front().filterImpedanceMagnitudeOhm > 0.0);
        assert(idealSource.value().points.front().sourceImpedanceMagnitudeOhm == 0.0);
        assert(idealSource.value().points.front().busImpedanceMagnitudeOhm == 0.0);
        for (const auto& point : idealSource.value().points)
            assert(point.gridCurrentTransferMagnitudeRatio == 1.0);
    }
    {
        // Parameter sweep: each case deliberately changes the network, branch,
        // frequency window and sampling density. Validate the solver against
        // an independently evaluated complex-network expression at three
        // positions in every sweep, not just against one golden fixture.
        struct Scenario {
            double sourceR;
            double sourceL;
            double branchR;
            double branchL;
            double branchC;
            double startHz;
            double stopHz;
            std::size_t points;
        };
        const std::array<Scenario, 4> scenarios{{
            {0.03, 0.00035, 0.004, 0.00016, 0.0022, 25.0, 800.0, 257},
            {0.12, 0.0018, 0.009, 0.00031, 0.0014, 40.0, 1600.0, 513},
            {0.0, 0.0007, 0.015, 0.00009, 0.0031, 60.0, 2400.0, 129},
            {0.2, 0.0, 0.003, 0.00048, 0.00082, 10.0, 3000.0, 1025}
        }};
        for (const auto& scenario : scenarios) {
            auto request = validRequest();
            request.source = {units::Resistance{scenario.sourceR},
                              units::Inductance{scenario.sourceL}};
            request.design.branches.front().dampingResistance =
                units::Resistance{scenario.branchR};
            request.design.branches.front().inductance =
                units::Inductance{scenario.branchL};
            request.design.branches.front().capacitance =
                units::Capacitance{scenario.branchC};
            request.startFrequency = units::Frequency{scenario.startHz};
            request.stopFrequency = units::Frequency{scenario.stopHz};
            request.points = scenario.points;

            const auto response = analyzeFrequencyResponse(request);
            assert(response);
            assert(response.value().points.size() == scenario.points);
            assert(std::abs(response.value().points.front().frequency.si() -
                            scenario.startHz) < 1.0e-9);
            assert(std::abs(response.value().points.back().frequency.si() -
                            scenario.stopHz) < 1.0e-8);
            const auto& branch = request.design.branches.front();
            for (const std::size_t index : {std::size_t{0}, scenario.points / 2,
                                             scenario.points - 1}) {
                const auto& point = response.value().points[index];
                const double frequency = point.frequency.si();
                const double omega = 2.0 * std::numbers::pi * frequency;
                const std::complex<double> sourceZ{
                    scenario.sourceR, omega * scenario.sourceL};
                const std::complex<double> filterZ{
                    branch.dampingResistance.si(),
                    omega * branch.inductance.si() -
                        1.0 / (omega * branch.capacitance.si())};
                const double expectedBus = std::abs(
                    sourceZ * filterZ / (sourceZ + filterZ));
                const double expectedTransfer =
                    1.0 / std::abs(1.0 + sourceZ / filterZ);
                assert(std::abs(point.busImpedanceMagnitudeOhm - expectedBus) <
                       1.0e-9 * std::max(1.0, expectedBus));
                assert(std::abs(point.gridCurrentTransferMagnitudeRatio -
                                expectedTransfer) <
                       1.0e-10 * std::max(1.0, expectedTransfer));
            }
        }
    }

    {
        auto request = validRequest();
        request.points = 2;
        assert(!analyzeFrequencyResponse(request));
    }
    {
        auto request = validRequest();
        request.design.branches.clear();
        assert(!analyzeFrequencyResponse(request));
    }
    {
        auto request = validRequest();
        request.design.branches.push_back(request.design.branches.front());
        assert(!analyzeFrequencyResponse(request));
    }

    return 0;
}

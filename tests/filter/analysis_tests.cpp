#include "p135/filter/analysis.hpp"

#include <algorithm>
#include <cassert>
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
        const auto result = analyzeFrequencyResponse(validRequest());
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

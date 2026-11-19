#include "p135/filter/harmonics.hpp"

#include <cassert>
#include <cmath>
#include <complex>

using namespace p135;
using namespace p135::filter;

namespace {

FilterDesign makeDesign(Connection connection = Connection::Wye)
{
    FilterSpecification specification;
    specification.network.lineToLineVoltage = units::Voltage{400.0};
    specification.network.fundamentalFrequency = units::Frequency{50.0};
    specification.network.connection = connection;
    specification.branches.push_back({
        1, "5th harmonic", 5, units::Power{100000.0}, 50.0
    });
    const auto design = synthesize(specification);
    assert(design);
    return design.value();
}

HarmonicAnalysisRequest validRequest(Connection connection = Connection::Wye)
{
    HarmonicAnalysisRequest request;
    request.design = makeDesign(connection);
    request.source = {units::Resistance{0.08}, units::Inductance{0.001}};
    request.sourceVoltageSpectrum = {
        {1, {230.9401076758503, 0.0}},
        {5, {10.0, -2.0}},
        {7, {5.0, 1.0}}
    };
    return request;
}

} // namespace

int main()
{
    {
        const auto result = analyzeHarmonics(validRequest());
        assert(result);
        assert(result.value().points.size() == 3);
        assert(result.value().branchStresses.size() == 1);
        assert(result.value().busFundamentalVoltageV > 0.0);
        assert(result.value().sourceFundamentalCurrentA > 0.0);
        assert(std::isfinite(result.value().busVoltageThd));
        assert(std::isfinite(result.value().sourceCurrentThd));
        assert(result.value().busVoltageThd >= 0.0);
        assert(result.value().sourceCurrentThd >= 0.0);
        const auto& stress = result.value().branchStresses.front();
        assert(stress.branchId == 1);
        assert(stress.totalRmsCurrentA > 0.0);
        assert(stress.capacitorRmsVoltageV > 0.0);
        assert(stress.inductorRmsVoltageV > 0.0);
        assert(stress.dampingResistorRmsVoltageV > 0.0);
        assert(stress.dampingResistorLossW > 0.0);
    }
    {
        const auto wye = analyzeHarmonics(validRequest(Connection::Wye));
        const auto delta = analyzeHarmonics(validRequest(Connection::Delta));
        assert(wye && delta);
        assert(delta.value().branchStresses.front().totalRmsCurrentA >
               wye.value().branchStresses.front().totalRmsCurrentA);
    }
    {
        auto request = validRequest();
        request.sourceVoltageSpectrum.erase(request.sourceVoltageSpectrum.begin());
        assert(!analyzeHarmonics(request));
    }
    {
        auto request = validRequest();
        request.sourceVoltageSpectrum.push_back(request.sourceVoltageSpectrum.back());
        assert(!analyzeHarmonics(request));
    }
    {
        auto request = validRequest();
        request.source = {units::Resistance{0.0}, units::Inductance{0.0}};
        assert(!analyzeHarmonics(request));
    }
    {
        auto request = validRequest();
        request.sourceVoltageSpectrum.front().phaseVoltageRms = {0.0, 0.0};
        assert(!analyzeHarmonics(request));
    }
    return 0;
}

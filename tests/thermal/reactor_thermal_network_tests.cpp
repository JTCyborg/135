#include "p135/thermal/reactor_thermal_network.hpp"

#include <cassert>
#include <cmath>
#include <string>

using namespace p135;
using namespace p135::thermal;

int main()
{
    ReactorThermalNetworkRequest request;
    request.ambientTemperature = units::TemperatureKelvin{293.15};
    for (std::size_t i = 0; i < 7; ++i) {
        auto& region = request.coreLosses.regions[i];
        region.totalLoss = units::Power{1.0};
        region.lossDataReference = "core-loss-dataset-R1";
        region.coreMassSourceReference = "core-mass-record-" + std::to_string(i);
        request.coreMaximumTemperatures[i] = units::TemperatureKelvin{400.0};
        request.coreTemperatureLimitSourceReferences[i] = "core-limit-source-R1";
    }

    domain::WindingHarmonicCopperLoss harmonic;
    harmonic.order = 1;
    harmonic.frequency = units::Frequency{50.0};
    harmonic.phaseLoss = {
        units::Power{1.0}, units::Power{2.0}, units::Power{3.0}
    };
    harmonic.totalThreePhaseLoss = units::Power{6.0};
    request.windingCopperLosses.harmonicLosses.push_back(harmonic);
    request.windingCopperLosses.totalThreePhaseCopperLoss = units::Power{6.0};
    request.windingLossSourceReference = "winding-copper-study-R1";
    request.coreMaterialDesignReleaseStatus = "VERIFIED";
    request.conductorMaterialDesignReleaseStatus = "VERIFIED";

    for (std::size_t i = 0; i < 3; ++i) {
        request.windingMaximumTemperatures[i] = units::TemperatureKelvin{400.0};
        request.windingTemperatureLimitSourceReferences[i] = "winding-limit-source-R1";
    }

    const std::array<std::string, 10> ids{
        "core.limb_a", "core.limb_b", "core.limb_c",
        "core.yoke_top_ab", "core.yoke_top_bc",
        "core.yoke_bottom_ab", "core.yoke_bottom_bc",
        "winding.phase_a", "winding.phase_b", "winding.phase_c"
    };
    for (const auto& id : ids)
        request.ambientLinks.push_back({id, 1.0, "measured-thermal-path-R1"});
    request.links.push_back({"core.limb_a", "winding.phase_a", 0.5,
                             "measured-core-winding-coupling-R1"});

    const auto result = analyzeReactorThermalNetwork(request);
    assert(result);
    assert(result.value().network.solved);
    assert(result.value().network.nodes.size() == 10);
    assert(result.value().network.modelProvenanceComplete);
    assert(result.value().phaseWindingLosses[0].si() == 1.0);
    assert(result.value().phaseWindingLosses[1].si() == 2.0);
    assert(result.value().phaseWindingLosses[2].si() == 3.0);
    for (const auto& node : result.value().network.nodes)
        assert(node.status == AssessmentStatus::Pass);

    auto unreleased = request;
    unreleased.conductorMaterialDesignReleaseStatus = "NOT_VERIFIED";
    const auto gated = analyzeReactorThermalNetwork(unreleased);
    assert(gated);
    assert(!gated.value().network.modelProvenanceComplete);
    for (const auto& node : gated.value().network.nodes)
        assert(node.status == AssessmentStatus::NotVerified);

    auto unqualifiedOverTemperature = request;
    for (auto& limit : unqualifiedOverTemperature.coreMaximumTemperatures)
        limit = units::TemperatureKelvin{294.0};
    const auto preliminaryFailure = analyzeReactorThermalNetwork(unqualifiedOverTemperature);
    assert(preliminaryFailure);
    for (std::size_t i = 0; i < preliminaryFailure.value().network.nodes.size(); ++i) {
        const auto& node = preliminaryFailure.value().network.nodes[i];
        if (i < 7) {
            assert(node.status == AssessmentStatus::Fail);
        } else {
            assert(node.status == AssessmentStatus::Pass);
        }
    }

    auto missingSource = request;
    missingSource.windingLossSourceReference.clear();
    assert(!analyzeReactorThermalNetwork(missingSource));

    auto inconsistentTotal = request;
    inconsistentTotal.windingCopperLosses.totalThreePhaseCopperLoss = units::Power{7.0};
    assert(!analyzeReactorThermalNetwork(inconsistentTotal));

    return 0;
}

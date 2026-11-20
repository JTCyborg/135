#include "p135/domain/core_loss.hpp"

#include <cassert>
#include <cmath>
#include <string>

using namespace p135;
using namespace p135::domain;

int main()
{
    CoreMaterial material;
    material.id = "CORE-7-REGION";
    material.sourceReference = "manufacturer-datasheet";
    material.datasetRevision = "R1";
    material.lossDataReference = "manufacturer-loss-table-R1";
    material.lossData.push_back({
        units::Frequency{100.0},
        units::FluxDensity{0.5},
        units::TemperatureKelvin{293.15},
        units::CoreLossDensity{4.0}
    });

    ThreeLimbCoreLossRequest request;
    request.frequency = units::Frequency{100.0};
    for (std::size_t i = 0; i < 7; ++i) {
        request.peakFluxDensities[i] = units::FluxDensity{0.5};
        request.temperatures[i] = units::TemperatureKelvin{293.15};
        request.regionMasses[i] = units::Mass{static_cast<double>(i + 1)};
        request.regionMassSourceReferences[i] = "core-region-mass-record-" + std::to_string(i + 1);
    }

    const auto result = calculateThreeLimbCoreLossPower(material, request);
    assert(result);
    assert(std::abs(result.value().totalLoss.si() - 112.0) < 1.0e-12);
    assert(result.value().regions.size() == 7);
    assert(std::abs(result.value().regions[6].totalLoss.si() - 28.0) < 1.0e-12);
    assert(result.value().regions[6].coreMassSourceReference ==
           request.regionMassSourceReferences[6]);

    auto missingMassProvenance = request;
    missingMassProvenance.regionMassSourceReferences[4].clear();
    const auto rejected = calculateThreeLimbCoreLossPower(material, missingMassProvenance);
    assert(!rejected);
    assert(rejected.error().code == ErrorCode::MissingRequiredData);

    auto outsideDomain = request;
    outsideDomain.peakFluxDensities[2] = units::FluxDensity{0.6};
    const auto rejectedOutside = calculateThreeLimbCoreLossPower(material, outsideDomain);
    assert(!rejectedOutside);
    assert(rejectedOutside.error().code == ErrorCode::ModelOutOfDomain);

    return 0;
}

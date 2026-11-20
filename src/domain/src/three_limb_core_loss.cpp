#include "p135/domain/core_loss.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace p135::domain {

Result<ThreeLimbCoreLossResult> calculateThreeLimbCoreLossPower(
    const CoreMaterial& material,
    const ThreeLimbCoreLossRequest& request)
{
    ThreeLimbCoreLossResult result;
    double totalWatts = 0.0;

    for (std::size_t i = 0; i < request.regionMasses.size(); ++i) {
        CoreLossPowerRequest regionRequest;
        regionRequest.frequency = request.frequency;
        regionRequest.peakFluxDensity = request.peakFluxDensities[i];
        regionRequest.temperature = request.temperatures[i];
        regionRequest.coreMass = request.regionMasses[i];
        regionRequest.coreMassSourceReference = request.regionMassSourceReferences[i];

        const auto region = calculateCoreLossPower(material, regionRequest);
        if (!region)
            return Result<ThreeLimbCoreLossResult>::failure(region.error());

        result.regions[i] = region.value();
        totalWatts += region.value().totalLoss.si();
        if (!std::isfinite(totalWatts))
            return Result<ThreeLimbCoreLossResult>::failure(Error{
                ErrorCode::InvalidInput,
                "Summed three-limb core loss is not finite.",
                {Diagnostic{Severity::Error, "THREE_LIMB_CORE_LOSS_SUM_INVALID",
                    "Summed three-limb core loss is not finite.", "totalLoss",
                    std::nullopt, std::nullopt}}
            });
    }

    result.totalLoss = units::Power{totalWatts};
    return Result<ThreeLimbCoreLossResult>::success(std::move(result));
}

} // namespace p135::domain

#include "p135/application/physical_filter_reactor_magnetic.hpp"

#include <utility>

namespace p135::application {

Result<PhysicalFilterReactorMagneticResult>
analyzePhysicalFilterReactorMagneticCycle(
    const PhysicalFilterReactorMagneticRequest& request)
{
    const auto geometry = domain::buildThreeLimbMagneticGeometry(
        request.physicalCore, request.stackingFactor);
    if (!geometry)
        return Result<PhysicalFilterReactorMagneticResult>::failure(geometry.error());

    auto magneticRequest = request.filterMagnetic;
    magneticRequest.magnetic.geometry = geometry.value();

    const auto analysis = analyzeFilterReactorMagneticCycle(magneticRequest);
    if (!analysis)
        return Result<PhysicalFilterReactorMagneticResult>::failure(analysis.error());

    PhysicalFilterReactorMagneticResult result;
    result.effectiveMagneticGeometry = geometry.value();
    result.magneticAnalysis = analysis.value();
    return Result<PhysicalFilterReactorMagneticResult>::success(std::move(result));
}

} // namespace p135::application

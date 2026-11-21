#pragma once

#include "p135/application/physical_filter_reactor_magnetic.hpp"
#include "p135/core/result.hpp"

#include <string>

namespace p135::application {

/// Serializes a converged, in-range physical three-limb magnetic operating
/// point. Extrapolated or unconverged cycles remain NOT_VERIFIED.
[[nodiscard]] Result<std::string> makeMagneticOperatingPointStageArtifact(
    const PhysicalFilterReactorMagneticRequest& request,
    const PhysicalFilterReactorMagneticResult& result);

} // namespace p135::application

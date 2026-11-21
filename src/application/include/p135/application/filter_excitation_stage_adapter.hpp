#pragma once

#include "p135/core/result.hpp"
#include "p135/filter/synthesis.hpp"

#include <string>

namespace p135::application {

/// Validates and serializes the actual filter synthesis output for the
/// filter.excitation dependency stage. This is an electrical artifact only;
/// it does not certify magnetic, winding, thermal or mechanical design.
[[nodiscard]] Result<std::string> makeFilterExcitationStageArtifact(
    const filter::FilterDesign& design);

} // namespace p135::application

#pragma once

#include "p135/domain/winding_electrical.hpp"
#include "p135/filter/harmonics.hpp"
#include "p135/infrastructure/sqlite/conductor_material_revision_loader.hpp"

#include <cstdint>
#include <string>

namespace p135::infrastructure::sqlite {

struct RevisionBoundWindingCopperLossRequest {
    std::string conductorMaterialRevisionId;
    std::uint32_t filterBranchId{};
    filter::HarmonicAnalysisResult harmonicAnalysis;
    std::uint32_t turns{};
    std::uint32_t parallelConductors{1};
    units::Length meanTurnLength;
    units::Area bareConductorCrossSection;
    units::TemperatureKelvin windingTemperature;
};

struct RevisionBoundWindingCopperLossResult {
    ConductorMaterialRevisionSnapshot conductorMaterialRevision;
    domain::WindingCopperLossResult copperLoss;
};

/// Loads the exact conductor-material revision and uses the selected filter
/// branch's phase RMS harmonic currents. Geometry and winding temperature
/// remain explicit caller inputs.
[[nodiscard]] Result<RevisionBoundWindingCopperLossResult>
calculateRevisionBoundWindingCopperLoss(
    const SqliteMaterialRevisionStore& store,
    const RevisionBoundWindingCopperLossRequest& request);

} // namespace p135::infrastructure::sqlite

#pragma once

#include "p135/infrastructure/sqlite/conductor_material_revision_loader.hpp"
#include "p135/infrastructure/sqlite/core_material_revision_loader.hpp"
#include "p135/infrastructure/sqlite/revision_bound_winding_copper_loss.hpp"
#include "p135/domain/three_limb_core_mass.hpp"
#include "p135/thermal/reactor_thermal_network.hpp"

namespace p135::infrastructure::sqlite {

struct RevisionBoundCoreMassInputs {
    domain::ThreeLimbCorePhysicalGeometry geometry;
    double stackingFactor{};
    std::string dimensionalSource;
    std::string dimensionalRevision;
    std::string stackingFactorSourceReference;
};

struct RevisionBoundReactorThermalNetworkResult {
    CoreMaterialRevisionSnapshot coreMaterialRevision;
    ConductorMaterialRevisionSnapshot conductorMaterialRevision;
    thermal::ReactorThermalNetworkResult thermal;
};

/// End-to-end revision-bound reactor thermal analysis:
/// 1) loads the exact core revision and calculates all seven core-loss regions;
/// 2) loads the exact conductor revision and calculates three-phase harmonic copper loss;
/// 3) assembles and solves the ten-node thermal network.
/// Material revisions and hashes are taken from SQLite, never from caller strings.
/// Thermal conductances, ambient temperature, temperature limits and their
/// provenance remain explicit configuration inputs.
[[nodiscard]] Result<RevisionBoundReactorThermalNetworkResult>
analyzeRevisionBoundReactorThermalNetwork(
    const SqliteMaterialRevisionStore& store,
    const std::string& coreMaterialRevisionId,
    const domain::ThreeLimbCoreLossRequest& coreLossRequest,
    const RevisionBoundWindingCopperLossRequest& windingLossRequest,
    const thermal::ReactorThermalNetworkConfiguration& thermalConfiguration);

/// Revision-bound overload that derives the seven physical core masses from
/// the supplied construction geometry and exact core-material revision before
/// evaluating losses. The operating-point Bpeak/temperature values are kept;
/// caller-supplied masses and mass references are replaced.
[[nodiscard]] Result<RevisionBoundReactorThermalNetworkResult>
analyzeRevisionBoundReactorThermalNetwork(
    const SqliteMaterialRevisionStore& store,
    const std::string& coreMaterialRevisionId,
    const domain::ThreeLimbCoreLossRequest& operatingPoint,
    const RevisionBoundCoreMassInputs& coreMassInputs,
    const RevisionBoundWindingCopperLossRequest& windingLossRequest,
    const thermal::ReactorThermalNetworkConfiguration& thermalConfiguration);

} // namespace p135::infrastructure::sqlite

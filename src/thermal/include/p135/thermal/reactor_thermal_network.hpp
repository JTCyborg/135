#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/core_loss.hpp"
#include "p135/domain/winding_electrical.hpp"
#include "p135/thermal/thermal_network.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace p135::thermal {

/// Thermal boundary conditions and explicit network topology. No values are
/// inferred from material, geometry, or a nominal cooling method.
struct ReactorThermalNetworkConfiguration {
    units::TemperatureKelvin ambientTemperature{293.15};
    std::array<std::optional<units::TemperatureKelvin>, 7> coreMaximumTemperatures{};
    std::array<std::string, 7> coreTemperatureLimitSourceReferences{};
    std::array<std::optional<units::TemperatureKelvin>, 3> windingMaximumTemperatures{};
    std::array<std::string, 3> windingTemperatureLimitSourceReferences{};
    std::vector<ThermalConductanceLink> links;
    std::vector<ThermalAmbientLink> ambientLinks;
};

struct ReactorThermalNetworkRequest : ReactorThermalNetworkConfiguration {
    domain::ThreeLimbCoreLossResult coreLosses;
    domain::WindingCopperLossResult windingCopperLosses;

    /// Source reference for the exact copper-loss calculation/revision.
    std::string windingLossSourceReference;
    /// Exact immutable material revision references in the form revisionId#SHA256.
    /// These are provenance labels; callers should prefer the revision-bound
    /// infrastructure adapter, which loads them from the revision store.
    std::string coreMaterialRevisionReference;
    std::string conductorMaterialRevisionReference;
    std::string coreMaterialDesignReleaseStatus;
    std::string conductorMaterialDesignReleaseStatus;
};

struct ReactorThermalNetworkResult {
    ThermalNetworkResult network;
    std::array<units::Power, 3> phaseWindingLosses{};
};

/// Builds seven core-region and three phase-winding thermal nodes and solves
/// the explicitly supplied coupled conductance network. It does not infer
/// conductances, cooling paths, limits, or material release state.
[[nodiscard]] Result<ReactorThermalNetworkResult> analyzeReactorThermalNetwork(
    const ReactorThermalNetworkRequest& request);

} // namespace p135::thermal

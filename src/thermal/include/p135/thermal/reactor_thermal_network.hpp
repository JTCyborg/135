#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/core_loss.hpp"
#include "p135/domain/winding_electrical.hpp"
#include "p135/thermal/thermal_network.hpp"
#include "p135/thermal/natural_convection.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>
#include <variant>

namespace p135::thermal {

/// One explicitly described exposed reactor surface. The selected isolated-plate
/// correlation is evaluated by the reactor network adapter; the surface is
/// never inferred from the reactor CAD or nominal cooling method.
struct ReactorNaturalConvectionSurfaceBoundary {
    std::string nodeId;
    std::variant<VerticalPlateNaturalConvectionRequest,
                 HorizontalPlateNaturalConvectionRequest> surface;
    std::string boundaryModelSourceReference;
};

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
    std::vector<ReactorNaturalConvectionSurfaceBoundary> naturalConvectionSurfaces;
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

struct ReactorNaturalConvectionSurfaceResult {
    std::string nodeId;
    NaturalConvectionResult convection;
};

struct ReactorThermalNetworkResult {
    ThermalNetworkResult network;
    std::array<units::Power, 3> phaseWindingLosses{};
    std::vector<ReactorNaturalConvectionSurfaceResult> naturalConvectionSurfaces;
};

/// Builds seven core-region and three phase-winding thermal nodes, evaluates
/// every explicitly supplied isolated-plate surface, converts its R-theta to
/// an ambient link, and solves the coupled conductance network. It does not
/// infer surface geometry, fluid properties, cooling paths, limits, or release state.
[[nodiscard]] Result<ReactorThermalNetworkResult> analyzeReactorThermalNetwork(
    const ReactorThermalNetworkRequest& request);

} // namespace p135::thermal

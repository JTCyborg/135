#pragma once

#include "p135/cad/three_limb_core.hpp"

#include <TopoDS_Shape.hxx>

#include <array>
#include <string>
#include <vector>

namespace p135::cad {

// Geometry detail is explicit so the UI cannot present envelopes or simplified
// segments as detailed, manufacturing-ready laminations/conductors.
enum class AssemblyGeometryRepresentation {
    PhysicalSolid,
    SimplifiedCoreSegment,
    WindingPackEnvelope,
    DetailedConductorTurn
};

enum class AssemblySurfaceFinish {
    NeutralMatte,
    ElectricalSteelSatin,
    CopperPolished,
    InsulatorMatte
};

enum class AssemblyComponentKind {
    Assembly,
    CoreGroup,
    CoreSegment,
    WindingGroup,
    PhaseGroup,
    Bobbin,
    WindingEnvelope,
    ConductorTurn
};

struct AssemblyPresentationNode;

struct AssemblyPresentationDisplayItem {
    std::string id;
    TopoDS_Shape shape;
    double transparency{0.0};
    // Presentation palette only; RGB values are not engineering material data.
    std::array<double, 3> baseColor{0.72, 0.75, 0.79};
    AssemblySurfaceFinish surfaceFinish{AssemblySurfaceFinish::NeutralMatte};
    std::string materialRevisionId; // Empty when the design has no pinned revision.
};

// Flatten the semantic tree into renderable leaf solids. Parent visibility
// suppresses descendants; parent and child transparency are composited.
// Group compounds are intentionally excluded to prevent duplicate rendering.
[[nodiscard]] std::vector<AssemblyPresentationDisplayItem>
collectAssemblyPresentationDisplayItems(const AssemblyPresentationNode& root);

struct AssemblyPresentationNode {
    std::string id;                 // Stable machine identity; never translated.
    std::string displayName;        // Localizable UI label.
    AssemblyComponentKind kind{AssemblyComponentKind::Assembly};
    AssemblyGeometryRepresentation representation{AssemblyGeometryRepresentation::PhysicalSolid};
    TopoDS_Shape shape;             // Null for logical grouping nodes.
    bool visible{true};
    double transparency{0.0};      // 0 = opaque, 1 = fully transparent.
    std::string materialRevisionId; // Catalog revision used by this component, if selected.
    AssemblySurfaceFinish surfaceFinish{AssemblySurfaceFinish::NeutralMatte};
    std::vector<AssemblyPresentationNode> children;
};

// Creates a semantic tree over the existing OCCT assembly builder output.
// The current core segments and winding envelopes are deliberately marked as
// simplified/envelope geometry, not as detailed lamination or conductor solids.
[[nodiscard]] Result<AssemblyPresentationNode> buildThreeLimbReactorPresentation(
    const domain::ReactorPhysicalGeometry& geometry);

[[nodiscard]] Result<void> validateAssemblyPresentation(
    const AssemblyPresentationNode& root);

// Exports the complete validated assembly B-Rep, independent of current UI visibility.
[[nodiscard]] Result<void> exportAssemblyStep(
    const AssemblyPresentationNode& root, const std::string& filePath);

// Returns false when the stable component ID is not present.
[[nodiscard]] bool setAssemblyComponentVisibility(
    AssemblyPresentationNode& root, const std::string& componentId, bool visible);

// Transparency is a presentation-only value in [0, 1]; it never modifies B-Rep.
[[nodiscard]] Result<void> setAssemblyComponentTransparency(
    AssemblyPresentationNode& root, const std::string& componentId, double transparency);

} // namespace p135::cad

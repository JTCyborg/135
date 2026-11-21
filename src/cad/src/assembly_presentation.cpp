#include "p135/cad/assembly_presentation.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <TopoDS_Iterator.hxx>
#include <STEPControl_Writer.hxx>
#include <STEPControl_StepModelType.hxx>
#include <IFSelect_ReturnStatus.hxx>

#include <array>
#include <cmath>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace p135::cad {
namespace {

Error presentationError(ErrorCode code, std::string message, std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "CAD_PRESENTATION_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return error;
}

bool validateNode(
    const AssemblyPresentationNode& node,
    std::unordered_set<std::string>& ids,
    std::string& failure)
{
    if (node.id.empty()) {
        failure = "Every assembly presentation node must have a stable non-empty ID.";
        return false;
    }
    if (!ids.insert(node.id).second) {
        failure = "Duplicate assembly presentation component ID: " + node.id;
        return false;
    }
    if (node.displayName.empty()) {
        failure = "Every assembly presentation node must have a display name: " + node.id;
        return false;
    }
    if (!std::isfinite(node.transparency) ||
        node.transparency < 0.0 || node.transparency > 1.0) {
        failure = "Transparency must be finite and in the inclusive range [0, 1]: " + node.id;
        return false;
    }
    if (!node.shape.IsNull() && !BRepCheck_Analyzer(node.shape).IsValid()) {
        failure = "Assembly presentation contains an invalid OCCT shape: " + node.id;
        return false;
    }
    for (const auto& child : node.children) {
        if (!validateNode(child, ids, failure)) return false;
    }
    return true;
}

bool setVisibility(
    AssemblyPresentationNode& node, const std::string& id, bool visible)
{
    if (node.id == id) {
        node.visible = visible;
        return true;
    }
    for (auto& child : node.children) {
        if (setVisibility(child, id, visible)) return true;
    }
    return false;
}

bool setTransparency(
    AssemblyPresentationNode& node, const std::string& id, double transparency)
{
    if (node.id == id) {
        node.transparency = transparency;
        return true;
    }
    for (auto& child : node.children) {
        if (setTransparency(child, id, transparency)) return true;
    }
    return false;
}

std::array<double, 3> presentationColor(const std::string& id)
{
    // Temporary visual palette keyed by semantic component role. These colors
    // are deliberately not inferred as verified material assignments.
    if (id.rfind("core.", 0) == 0) return {0.48, 0.55, 0.63}; // laminated steel
    if (id.rfind("bobbin.", 0) == 0) return {0.88, 0.78, 0.56}; // insulating former
    if (id.rfind("winding.", 0) == 0) return {0.78, 0.39, 0.16}; // copper envelope
    return {0.72, 0.75, 0.79};
}

void collectDisplayItems(
    const AssemblyPresentationNode& node,
    bool ancestorsVisible,
    double inheritedTransparency,
    std::vector<AssemblyPresentationDisplayItem>& items)
{
    const bool effectiveVisible = ancestorsVisible && node.visible;
    if (!effectiveVisible) return;

    const double effectiveTransparency =
        1.0 - (1.0 - inheritedTransparency) * (1.0 - node.transparency);

    if (node.children.empty() && !node.shape.IsNull()) {
        items.push_back(AssemblyPresentationDisplayItem{
            node.id, node.shape, effectiveTransparency, presentationColor(node.id)
        });
        return;
    }

    for (const auto& child : node.children)
        collectDisplayItems(child, effectiveVisible, effectiveTransparency, items);
}

} // namespace

Result<AssemblyPresentationNode> buildThreeLimbReactorPresentation(
    const domain::ReactorPhysicalGeometry& geometry)
{
    const auto assembly = buildThreeLimbReactorAssemblyWithWindingEnvelopesShape(geometry);
    if (!assembly) return Result<AssemblyPresentationNode>::failure(assembly.error());

    std::vector<TopoDS_Shape> topLevel;
    for (TopoDS_Iterator it(assembly.value()); it.More(); it.Next())
        topLevel.push_back(it.Value());

    // Contract of buildThreeLimbReactorAssemblyWithWindingEnvelopesShape:
    // [core compound, bobbin A, winding A, bobbin B, winding B, bobbin C, winding C].
    if (topLevel.size() != 7) {
        return Result<AssemblyPresentationNode>::failure(presentationError(
            ErrorCode::SolverFailure,
            "The OCCT reactor assembly does not match the expected semantic component layout.",
            "assembly"));
    }

    AssemblyPresentationNode root;
    root.id = "reactor";
    root.displayName = geometry.productId.empty() ? "Three-phase reactor" : geometry.productId;
    root.kind = AssemblyComponentKind::Assembly;
    root.shape = assembly.value();

    AssemblyPresentationNode core;
    core.id = "core.laminations";
    core.displayName = "Core (simplified physical segments)";
    core.kind = AssemblyComponentKind::CoreGroup;
    core.representation = AssemblyGeometryRepresentation::SimplifiedCoreSegment;
    core.shape = topLevel[0];

    constexpr std::array<const char*, 7> coreIds{
        "core.limb.A", "core.limb.B", "core.limb.C",
        "core.yoke.top.AB", "core.yoke.top.BC",
        "core.yoke.bottom.AB", "core.yoke.bottom.BC"
    };
    constexpr std::array<const char*, 7> coreNames{
        "Limb A", "Limb B", "Limb C",
        "Top yoke A-B", "Top yoke B-C",
        "Bottom yoke A-B", "Bottom yoke B-C"
    };
    std::size_t coreComponentCount = 0;
    for (TopoDS_Iterator it(topLevel[0]); it.More(); it.Next()) {
        if (coreComponentCount >= coreIds.size()) {
            return Result<AssemblyPresentationNode>::failure(presentationError(
                ErrorCode::SolverFailure,
                "The core builder returned more components than the semantic assembly contract supports.",
                "core"));
        }
        AssemblyPresentationNode component;
        component.id = coreIds[coreComponentCount];
        component.displayName = coreNames[coreComponentCount];
        component.kind = AssemblyComponentKind::CoreSegment;
        component.representation = AssemblyGeometryRepresentation::SimplifiedCoreSegment;
        component.shape = it.Value();
        core.children.push_back(std::move(component));
        ++coreComponentCount;
    }
    if (coreComponentCount != coreIds.size()) {
        return Result<AssemblyPresentationNode>::failure(presentationError(
            ErrorCode::SolverFailure,
            "The core builder must return exactly three limbs and four yoke segments.",
            "core"));
    }
    root.children.push_back(std::move(core));

    AssemblyPresentationNode windingGroup;
    windingGroup.id = "windings";
    windingGroup.displayName = "Winding system";
    windingGroup.kind = AssemblyComponentKind::WindingGroup;

    constexpr std::array<const char*, 3> phaseIds{"L1", "L2", "L3"};
    for (std::size_t phase = 0; phase < phaseIds.size(); ++phase) {
        const std::string phaseId = phaseIds[phase];
        const std::size_t bobbinIndex = 1 + 2 * phase;
        const std::size_t windingIndex = bobbinIndex + 1;

        AssemblyPresentationNode phaseNode;
        phaseNode.id = "phase." + phaseId;
        phaseNode.displayName = "Phase " + phaseId;
        phaseNode.kind = AssemblyComponentKind::PhaseGroup;

        AssemblyPresentationNode bobbin;
        bobbin.id = "bobbin." + phaseId;
        bobbin.displayName = "Bobbin " + phaseId;
        bobbin.kind = AssemblyComponentKind::Bobbin;
        bobbin.representation = AssemblyGeometryRepresentation::PhysicalSolid;
        bobbin.shape = topLevel[bobbinIndex];

        AssemblyPresentationNode winding;
        winding.id = "winding." + phaseId + ".envelope";
        winding.displayName = "Winding " + phaseId + " (envelope only)";
        winding.kind = AssemblyComponentKind::WindingEnvelope;
        winding.representation = AssemblyGeometryRepresentation::WindingPackEnvelope;
        winding.shape = topLevel[windingIndex];

        phaseNode.children.push_back(std::move(bobbin));
        phaseNode.children.push_back(std::move(winding));
        windingGroup.children.push_back(std::move(phaseNode));
    }
    root.children.push_back(std::move(windingGroup));

    const auto validation = validateAssemblyPresentation(root);
    if (!validation) return Result<AssemblyPresentationNode>::failure(validation.error());
    return Result<AssemblyPresentationNode>::success(std::move(root));
}

std::vector<AssemblyPresentationDisplayItem>
collectAssemblyPresentationDisplayItems(const AssemblyPresentationNode& root)
{
    std::vector<AssemblyPresentationDisplayItem> items;
    collectDisplayItems(root, true, 0.0, items);
    return items;
}

Result<void> exportAssemblyStep(
    const AssemblyPresentationNode& root, const std::string& filePath)
{
    const auto validation = validateAssemblyPresentation(root);
    if (!validation) return validation;
    if (filePath.empty()) {
        return Result<void>::failure(presentationError(
            ErrorCode::InvalidInput, "A non-empty STEP output path is required.", "step.filePath"));
    }
    if (root.shape.IsNull() || !BRepCheck_Analyzer(root.shape).IsValid()) {
        return Result<void>::failure(presentationError(
            ErrorCode::SolverFailure, "The complete assembly B-Rep is invalid; STEP export was blocked.", "step.assembly"));
    }
    STEPControl_Writer writer;
    const auto transferStatus = writer.Transfer(root.shape, STEPControl_AsIs);
    if (transferStatus != IFSelect_RetDone) {
        return Result<void>::failure(presentationError(
            ErrorCode::SolverFailure, "OCCT failed to transfer the validated assembly to STEP.", "step.transfer"));
    }
    const auto writeStatus = writer.Write(filePath.c_str());
    if (writeStatus != IFSelect_RetDone) {
        return Result<void>::failure(presentationError(
            ErrorCode::SolverFailure, "OCCT failed to write the STEP file.", "step.write"));
    }
    return Result<void>::success();
}

Result<void> validateAssemblyPresentation(const AssemblyPresentationNode& root)
{
    std::unordered_set<std::string> ids;
    std::string failure;
    if (!validateNode(root, ids, failure)) {
        return Result<void>::failure(presentationError(
            ErrorCode::InvalidInput, std::move(failure), "assemblyPresentation"));
    }
    if (root.shape.IsNull()) {
        return Result<void>::failure(presentationError(
            ErrorCode::InvalidInput,
            "The assembly presentation root must reference a non-null assembly shape.",
            "assemblyPresentation.root"));
    }
    return Result<void>::success();
}

bool setAssemblyComponentVisibility(
    AssemblyPresentationNode& root, const std::string& componentId, bool visible)
{
    return setVisibility(root, componentId, visible);
}

Result<void> setAssemblyComponentTransparency(
    AssemblyPresentationNode& root, const std::string& componentId, double transparency)
{
    if (!std::isfinite(transparency) || transparency < 0.0 || transparency > 1.0) {
        return Result<void>::failure(presentationError(
            ErrorCode::InvalidInput,
            "Transparency must be finite and in the inclusive range [0, 1].",
            "assemblyPresentation.transparency"));
    }
    if (!setTransparency(root, componentId, transparency)) {
        return Result<void>::failure(presentationError(
            ErrorCode::InvalidInput,
            "Assembly presentation component ID was not found: " + componentId,
            "assemblyPresentation.componentId"));
    }
    return Result<void>::success();
}

} // namespace p135::cad

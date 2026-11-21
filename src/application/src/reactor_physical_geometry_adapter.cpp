#include "p135/application/reactor_design_inputs.hpp"

#include <array>
#include <string>
#include <utility>

namespace p135::application {
namespace {
Result<domain::ReactorPhysicalGeometry> geometryFailure(std::string message, std::string field)
{
    Error error;
    error.code = ErrorCode::InvalidInput;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "PHYSICAL_GEOMETRY_INPUT_MISSING", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<domain::ReactorPhysicalGeometry>::failure(std::move(error));
}
}

Result<domain::ReactorPhysicalGeometry> makeReactorPhysicalGeometry(
    const ReactorDesignInputs& in, const std::string& productId)
{
    using domain::units::Length;
    using domain::CoreSegment;
    using domain::CoreJointInterface;
    using domain::CoreJointType;
    using domain::CoreJointPhysicalGeometry;
    using domain::AirGapPhysicalGeometry;
    using domain::BobbinPhysicalGeometry;
    using domain::WindingPackPhysicalGeometry;

    if (productId.empty()) return geometryFailure("A project/product ID is required.", "productId");
    const auto required = [](const auto& value, const char* field) -> bool { return value.has_value(); };
    const auto requireArray = [&](const auto& value, const char* field) -> bool {
        return required(value, field);
    };
    if (!in.dimensionalSource || !in.dimensionalRevision ||
        in.dimensionalSource->empty() || in.dimensionalRevision->empty())
        return geometryFailure("Physical dimensions require an explicit source and revision.", "dimensionalProvenance");
    if (!in.coreLaminationConstruction || in.coreLaminationConstruction->empty())
        return geometryFailure("Core lamination/packet construction description is required.", "core.laminationConstruction");
    if (!in.coreJointConstructions)
        return geometryFailure("All eight core joint interfaces must be specified.", "core.joints");
    if (!in.limbWidthsM || !in.limbStackDepthsM || !in.windowWidthsM || !in.windowHeightsM ||
        !in.yokeThicknessesM || !in.yokeDepthsM || !in.limbCenterSpacingsM ||
        !in.coreOverallHeightM || !in.coreOverallWidthM)
        return geometryFailure("All explicit core physical dimensions are required before CAD generation.", "core.dimensions");
    if (!in.bobbinInnerWidthsM || !in.bobbinInnerHeightsM || !in.bobbinAxialLengthsM ||
        !in.bobbinWallThicknessesM || !in.bobbinFlangeThicknessesM ||
        !in.bobbinFlangeOuterWidthsM || !in.bobbinFlangeOuterHeightsM ||
        !in.windingRadialBuildsM || !in.windingAxialHeightsM || !in.windingEndClearancesM)
        return geometryFailure("All three bobbin and winding-pack dimension sets are required.", "windings.dimensions");
    if (!in.bobbinMaterialId || in.bobbinMaterialId->empty())
        return geometryFailure("An explicit bobbin material identifier is required.", "bobbins.material");
    if (!in.windingTechnology || in.windingTechnology->empty() ||
        !in.windingLayers || !in.windingSections)
        return geometryFailure("Winding technology, layer count and section count are required.", "windings.construction");
    if (in.coreJointConstructions->size() != 8)
        return geometryFailure("Exactly eight joint construction values are required.", "core.joints");

    domain::ReactorPhysicalGeometry out;
    out.productId = productId;
    out.dimensionalSource = *in.dimensionalSource;
    out.dimensionalRevision = *in.dimensionalRevision;
    auto& core = out.core;
    for (std::size_t i = 0; i < 3; ++i) {
        core.limbWidths[i] = Length((*in.limbWidthsM)[i]);
        core.limbDepths[i] = Length((*in.limbStackDepthsM)[i]);
        core.limbCenterSpacings[i] = Length((*in.limbCenterSpacingsM)[i]);
    }
    for (std::size_t i = 0; i < 2; ++i) {
        core.windowWidths[i] = Length((*in.windowWidthsM)[i]);
        core.windowHeights[i] = Length((*in.windowHeightsM)[i]);
    }
    for (std::size_t i = 0; i < 4; ++i) {
        core.yokeThicknesses[i] = Length((*in.yokeThicknessesM)[i]);
        core.yokeDepths[i] = Length((*in.yokeDepthsM)[i]);
    }
    core.coreOverallHeight = Length(*in.coreOverallHeightM);
    core.coreOverallWidth = Length(*in.coreOverallWidthM);
    core.laminationConstruction = *in.coreLaminationConstruction;

    constexpr std::array<CoreJointInterface, 8> interfaces{
        CoreJointInterface::TopYokeABToLimbA, CoreJointInterface::TopYokeABToLimbB,
        CoreJointInterface::TopYokeBCToLimbB, CoreJointInterface::TopYokeBCToLimbC,
        CoreJointInterface::BottomYokeABToLimbA, CoreJointInterface::BottomYokeABToLimbB,
        CoreJointInterface::BottomYokeBCToLimbB, CoreJointInterface::BottomYokeBCToLimbC
    };
    for (std::size_t i = 0; i < interfaces.size(); ++i) {
        const auto& type = (*in.coreJointConstructions)[i];
        if (type == "step-lap")
            return geometryFailure("Step-Lap CAD is blocked until packet-by-packet overlap geometry is entered.",
                "core.joints[" + std::to_string(i) + "].stepLapPackets");
        if (type != "butt")
            return geometryFailure("Unsupported or missing core joint construction.", "core.joints[" + std::to_string(i) + "]");
        core.joints.push_back(CoreJointPhysicalGeometry{interfaces[i], CoreJointType::Butt, {}});
    }

    for (const auto& gap : in.airGaps) {
        if (gap.limbIndex >= 3)
            return geometryFailure("Air-gap limb index must be A, B or C.", "core.airGaps");
        const auto segment = static_cast<CoreSegment>(
            static_cast<std::size_t>(CoreSegment::LimbA) + gap.limbIndex);
        core.airGaps.push_back(AirGapPhysicalGeometry{
            segment, Length(gap.positionFromLimbStartM), Length(gap.lengthM)});
    }

    for (std::size_t i = 0; i < 3; ++i) {
        out.bobbins[i] = BobbinPhysicalGeometry{
            Length((*in.bobbinInnerWidthsM)[i]), Length((*in.bobbinInnerHeightsM)[i]),
            Length((*in.bobbinAxialLengthsM)[i]), Length((*in.bobbinWallThicknessesM)[i]),
            Length((*in.bobbinFlangeThicknessesM)[i]), Length((*in.bobbinFlangeOuterWidthsM)[i]),
            Length((*in.bobbinFlangeOuterHeightsM)[i]), Length(0.0), *in.bobbinMaterialId
        };
        out.windingPacks[i] = WindingPackPhysicalGeometry{
            Length((*in.windingRadialBuildsM)[i]), Length((*in.windingAxialHeightsM)[i]),
            Length((*in.windingEndClearancesM)[i]),
            Length(in.layerInsulationBuildM.value_or(0.0)),
            Length(in.mainInsulationBuildM.value_or(0.0)),
            Length(in.conductorInsulationBuildM.value_or(0.0)),
            *in.windingLayers, *in.windingSections, *in.windingTechnology
        };
    }
    const auto validation = domain::validate(out);
    if (!validation) return Result<domain::ReactorPhysicalGeometry>::failure(validation.error());
    return Result<domain::ReactorPhysicalGeometry>::success(std::move(out));
}

} // namespace p135::application

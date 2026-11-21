#include "p135/application/magnetic_operating_point_stage_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace p135::application {
namespace {
Error invalidArtifact(const char* message)
{
    return {ErrorCode::InvalidInput, message, {}};
}

bool positiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool finiteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool singleLine(const std::string& value)
{
    return !value.empty() && value.find_first_of("=\\r\\n") == std::string::npos;
}

bool singleLineOrEmpty(const std::string& value)
{
    return value.find_first_of("=\\r\\n") == std::string::npos;
}
}

Result<std::string> makeMagneticOperatingPointStageArtifact(
    const PhysicalFilterReactorMagneticRequest& request,
    const PhysicalFilterReactorMagneticResult& result)
{
    const auto physicalValidation = domain::validate(request.physicalCore);
    if (!physicalValidation)
        return Result<std::string>::failure(physicalValidation.error());

    const auto& material = request.filterMagnetic.magnetic.material;
    const bool validTurns = std::all_of(request.filterMagnetic.magnetic.turns.begin(),
        request.filterMagnetic.magnetic.turns.end(),
        [](std::uint32_t turns) { return turns > 0; });
    const bool validBhCurve = material.bhCurve.size() >= 2 &&
        std::all_of(material.bhCurve.begin(), material.bhCurve.end(), [](const auto& point) {
            return std::isfinite(point.fieldStrength.si()) && point.fieldStrength.si() >= 0.0 &&
                   std::isfinite(point.fluxDensity.si()) && point.fluxDensity.si() >= 0.0;
        });
    if (!positiveFinite(request.stackingFactor) || request.stackingFactor > 1.0 ||
        request.filterMagnetic.filterBranchId == 0 || !validTurns || !validBhCurve ||
        !singleLine(request.dimensionalSource) || !singleLine(request.dimensionalRevision) ||
        !singleLine(material.id) || !singleLine(material.manufacturer) ||
        !singleLine(material.grade) || !singleLine(material.sourceReference) ||
        !singleLine(material.datasetRevision) || !singleLineOrEmpty(material.lossDataReference) ||
        !singleLine(request.physicalCore.laminationConstruction) ||
        result.magneticAnalysis.filterBranchId != request.filterMagnetic.filterBranchId ||
        material.id.empty() || material.sourceReference.empty() || material.datasetRevision.empty())
        return Result<std::string>::failure(invalidArtifact(
            "Magnetic operating point requires a valid stacking factor, matching filter branch and revision-bound B-H material."));

    const auto& cycle = result.magneticAnalysis.magneticCycle;
    if (!cycle.converged || !cycle.withinBhCurveRange || cycle.samplesUsed < 72 ||
        !std::isfinite(cycle.relativePeakChange) || cycle.relativePeakChange < 0.0)
        return Result<std::string>::failure(invalidArtifact(
            "Magnetic operating point is NOT_VERIFIED: cycle must converge within the measured B-H range."));

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10);
    output << "schema=project135.magnetic-operating-point.v1\n";
    output << "filter_branch_id=" << result.magneticAnalysis.filterBranchId << '\n';
    output << "dimensional_source=" << request.dimensionalSource << '\n';
    output << "dimensional_revision=" << request.dimensionalRevision << '\n';
    output << "stacking_factor=" << request.stackingFactor << '\n';
    output << "core_material_id=" << request.filterMagnetic.magnetic.material.id << '\n';
    output << "bh_source_reference=" << request.filterMagnetic.magnetic.material.sourceReference << '\n';
    output << "bh_dataset_revision=" << request.filterMagnetic.magnetic.material.datasetRevision << '\n';
    output << "samples_used=" << cycle.samplesUsed << '\n';
    output << "relative_peak_change=" << cycle.relativePeakChange << '\n';
    output << "converged=true\nwithin_bh_curve_range=true\n";

    output << "relative_peak_tolerance=" << request.filterMagnetic.relativePeakTolerance << '\n';
    output << "maximum_samples=" << request.filterMagnetic.maximumSamples << '\n';
    output << "lamination_construction=" << request.physicalCore.laminationConstruction << '\n';
    const auto writeLengths = [&output](const char* key, const auto& values) {
        for (std::size_t i = 0; i < values.size(); ++i)
            output << key << '_' << i << "_m=" << values[i].si() << '\n';
    };
    writeLengths("physical_limb_width", request.physicalCore.limbWidths);
    writeLengths("physical_limb_depth", request.physicalCore.limbDepths);
    writeLengths("physical_window_width", request.physicalCore.windowWidths);
    writeLengths("physical_window_height", request.physicalCore.windowHeights);
    writeLengths("physical_yoke_thickness", request.physicalCore.yokeThicknesses);
    writeLengths("physical_yoke_depth", request.physicalCore.yokeDepths);
    writeLengths("physical_limb_center_spacing", request.physicalCore.limbCenterSpacings);
    output << "physical_core_overall_height_m=" << request.physicalCore.coreOverallHeight.si() << '\n';
    output << "physical_core_overall_width_m=" << request.physicalCore.coreOverallWidth.si() << '\n';
    output << "joint_count=" << request.physicalCore.joints.size() << '\n';
    for (std::size_t i = 0; i < request.physicalCore.joints.size(); ++i) {
        const auto& joint = request.physicalCore.joints[i];
        output << "joint." << i << ".interface=" << static_cast<int>(joint.interface) << '\n';
        output << "joint." << i << ".construction=" << static_cast<int>(joint.construction) << '\n';
        output << "joint." << i << ".step_lap_packet_count=" << joint.stepLapPackets.size() << '\n';
        for (std::size_t packet = 0; packet < joint.stepLapPackets.size(); ++packet) {
            const auto& p = joint.stepLapPackets[packet];
            output << "joint." << i << ".packet." << packet << ".stack_offset_m=" << p.stackDepthOffset.si() << '\n';
            output << "joint." << i << ".packet." << packet << ".stack_depth_m=" << p.stackDepth.si() << '\n';
            output << "joint." << i << ".packet." << packet << ".overlap_limb_m=" << p.overlapIntoLimb.si() << '\n';
            output << "joint." << i << ".packet." << packet << ".overlap_yoke_m=" << p.overlapIntoYoke.si() << '\n';
            output << "joint." << i << ".packet." << packet << ".direction=" << static_cast<int>(p.direction) << '\n';
        }
    }
    output << "air_gap_count=" << request.physicalCore.airGaps.size() << '\n';
    for (std::size_t i = 0; i < request.physicalCore.airGaps.size(); ++i) {
        const auto& gap = request.physicalCore.airGaps[i];
        output << "air_gap." << i << ".segment=" << static_cast<int>(gap.segment) << '\n';
        output << "air_gap." << i << ".position_m=" << gap.positionFromSegmentStart.si() << '\n';
        output << "air_gap." << i << ".length_m=" << gap.length.si() << '\n';
    }
    const auto& geometry = result.effectiveMagneticGeometry;
    output << "magnetic_geometry.limb_area_m2=" << geometry.limbEffectiveArea.si() << '\n';
    output << "magnetic_geometry.limb_path_m=" << geometry.limbMeanPath.si() << '\n';
    output << "magnetic_geometry.limb_gap_m=" << geometry.limbAirGap.si() << '\n';
    output << "magnetic_geometry.yoke_area_m2=" << geometry.yokeEffectiveArea.si() << '\n';
    output << "magnetic_geometry.yoke_path_m=" << geometry.yokeSegmentPath.si() << '\n';
    for (std::size_t i = 0; i < geometry.limbEffectiveAreas.size(); ++i) {
        if (geometry.limbEffectiveAreas[i])
            output << "magnetic_geometry.limb_area_" << i << "_m2=" << geometry.limbEffectiveAreas[i]->si() << '\n';
        if (geometry.limbMeanPaths[i])
            output << "magnetic_geometry.limb_path_" << i << "_m=" << geometry.limbMeanPaths[i]->si() << '\n';
        if (geometry.limbAirGaps[i])
            output << "magnetic_geometry.limb_gap_" << i << "_m=" << geometry.limbAirGaps[i]->si() << '\n';
        if (geometry.limbGapEffectiveAreas[i])
            output << "magnetic_geometry.limb_gap_area_" << i << "_m2=" << geometry.limbGapEffectiveAreas[i]->si() << '\n';
    }
    for (std::size_t i = 0; i < geometry.yokeEffectiveAreas.size(); ++i) {
        if (geometry.yokeEffectiveAreas[i])
            output << "magnetic_geometry.yoke_area_" << i << "_m2=" << geometry.yokeEffectiveAreas[i]->si() << '\n';
        if (geometry.yokeSegmentPaths[i])
            output << "magnetic_geometry.yoke_path_" << i << "_m=" << geometry.yokeSegmentPaths[i]->si() << '\n';
    }
    for (std::size_t i = 0; i < request.filterMagnetic.magnetic.turns.size(); ++i)
        output << "turns_phase_" << i << '=' << request.filterMagnetic.magnetic.turns[i] << '\n';
    output << "core_material_manufacturer=" << material.manufacturer << '\n';
    output << "core_material_grade=" << material.grade << '\n';
    output << "core_material_family=" << static_cast<int>(material.family) << '\n';
    output << "core_material_data_source=" << static_cast<int>(material.dataSource) << '\n';
    output << "core_material_density_kg_m3=" << material.density.si() << '\n';
    output << "bh_curve_count=" << material.bhCurve.size() << '\n';
    for (std::size_t i = 0; i < material.bhCurve.size(); ++i) {
        output << "bh_curve." << i << ".H_A_per_m=" << material.bhCurve[i].fieldStrength.si() << '\n';
        output << "bh_curve." << i << ".B_T=" << material.bhCurve[i].fluxDensity.si() << '\n';
    }
    output << "core_loss_data_reference=" << material.lossDataReference << '\n';
    output << "core_loss_point_count=" << material.lossData.size() << '\n';
    for (std::size_t i = 0; i < material.lossData.size(); ++i) {
        const auto& point = material.lossData[i];
        if (!std::isfinite(point.frequency.si()) || point.frequency.si() <= 0.0 ||
            !std::isfinite(point.peakFluxDensity.si()) || point.peakFluxDensity.si() < 0.0 ||
            !std::isfinite(point.temperature.si()) || point.temperature.si() <= 0.0 ||
            !std::isfinite(point.specificLoss.si()) ||
            point.specificLoss.si() < 0.0)
            return Result<std::string>::failure(invalidArtifact(
                "Magnetic operating point contains invalid core-loss material data."));
        output << "core_loss." << i << ".frequency_Hz=" << point.frequency.si() << '\n';
        output << "core_loss." << i << ".peak_flux_density_T=" << point.peakFluxDensity.si() << '\n';
        output << "core_loss." << i << ".temperature_K=" << point.temperature.si() << '\n';
        output << "core_loss." << i << ".specific_loss_W_m3=" << point.specificLoss.si() << '\n';
    }
    output << "excitation_count=" << result.magneticAnalysis.excitations.size() << '\n';
    for (std::size_t i = 0; i < result.magneticAnalysis.excitations.size(); ++i) {
        const auto& excitation = result.magneticAnalysis.excitations[i];
        if (excitation.order == 0)
            return Result<std::string>::failure(invalidArtifact(
                "Magnetic operating point contains a zero-order AC excitation."));
        output << "excitation." << i << ".order=" << excitation.order << '\n';
        for (std::size_t phase = 0; phase < 3; ++phase) {
            const auto value = excitation.phaseRmsPhasors[phase];
            if (!std::isfinite(value.real()) || !std::isfinite(value.imag()))
                return Result<std::string>::failure(invalidArtifact(
                    "Magnetic operating point contains a non-finite harmonic phasor."));
            output << "excitation." << i << ".phase." << phase
                   << ".rms_real_A=" << value.real() << '\n';
            output << "excitation." << i << ".phase." << phase
                   << ".rms_imag_A=" << value.imag() << '\n';
        }
    }
    for (std::size_t i = 0; i < cycle.peakAbsoluteFluxDensity.size(); ++i) {
        const double peak = cycle.peakAbsoluteFluxDensity[i].si();
        if (!finiteNonNegative(peak))
            return Result<std::string>::failure(invalidArtifact(
                "Magnetic operating point contains a negative or non-finite peak flux density."));
        output << "peak_absolute_flux_density_" << i << "_T=" << peak << '\n';
    }
    return Result<std::string>::success(output.str());
}

} // namespace p135::application

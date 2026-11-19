#include "p135/domain/three_limb_magnetic.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace p135::domain {
namespace {
constexpr double vacuumPermeability = 4.0e-7 * 3.14159265358979323846;
constexpr std::size_t nodeCount = 6;
constexpr std::size_t unknownCount = 5;
constexpr std::size_t edgeCount = 7;

struct Edge {
    std::size_t from{};
    std::size_t to{};
    double area{};
    double path{};
    double gap{};
    double mmf{};
};

struct EdgeState {
    double flux{};
    double differentialPermeance{};
    double fluxDensity{};
};

Result<ThreeLimbMagneticResult> fail(ErrorCode code, std::string message,
                                     std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "THREE_LIMB_MAGNETIC_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<ThreeLimbMagneticResult>::failure(std::move(error));
}

std::optional<std::pair<double, double>> bhAtFluxDensity(
    const CoreMaterial& material, double fluxDensity)
{
    const auto& curve = material.bhCurve;
    if (curve.size() < 2 || fluxDensity < 0.0 ||
        fluxDensity > curve.back().fluxDensity.si())
        return std::nullopt;
    for (std::size_t i = 1; i < curve.size(); ++i) {
        const double b0 = curve[i - 1].fluxDensity.si();
        const double b1 = curve[i].fluxDensity.si();
        if (fluxDensity <= b1) {
            const double h0 = curve[i - 1].fieldStrength.si();
            const double h1 = curve[i].fieldStrength.si();
            const double slope = (h1 - h0) / (b1 - b0);
            if (!std::isfinite(slope) || slope <= 0.0) return std::nullopt;
            return std::pair{h0 + (fluxDensity - b0) * slope, slope};
        }
    }
    return std::nullopt;
}

double requiredMmf(const CoreMaterial& material, const Edge& edge,
                   double fluxDensity)
{
    const auto point = bhAtFluxDensity(material, fluxDensity);
    if (!point) return std::numeric_limits<double>::quiet_NaN();
    return point->first * edge.path + fluxDensity * edge.gap / vacuumPermeability;
}

std::optional<EdgeState> evaluateEdge(const CoreMaterial& material,
                                      const Edge& edge, double nodeDrop)
{
    const double drive = nodeDrop + edge.mmf;
    const double magnitude = std::abs(drive);
    const double maximum = requiredMmf(material, edge,
        material.bhCurve.back().fluxDensity.si());
    if (!std::isfinite(maximum) || magnitude > maximum * (1.0 + 1.0e-12))
        return std::nullopt;

    double low = 0.0;
    double high = material.bhCurve.back().fluxDensity.si();
    for (int iteration = 0; iteration < 70; ++iteration) {
        const double middle = 0.5 * (low + high);
        if (requiredMmf(material, edge, middle) < magnitude)
            low = middle;
        else
            high = middle;
    }
    const double b = 0.5 * (low + high);
    const auto point = bhAtFluxDensity(material, b);
    if (!point) return std::nullopt;
    const double differentialReluctance =
        edge.path * point->second + edge.gap / vacuumPermeability;
    if (!std::isfinite(differentialReluctance) || differentialReluctance <= 0.0)
        return std::nullopt;
    const double sign = drive < 0.0 ? -1.0 : 1.0;
    return EdgeState{
        sign * b * edge.area,
        edge.area / differentialReluctance,
        sign * b
    };
}

struct NetworkState {
    std::array<double, edgeCount> flux{};
    std::array<double, edgeCount> permeance{};
    std::array<double, edgeCount> fluxDensity{};
    std::array<double, nodeCount> residual{};
    double norm{};
};

std::optional<NetworkState> evaluateNetwork(
    const CoreMaterial& material, const std::array<Edge, edgeCount>& edges,
    const std::array<double, nodeCount>& potential)
{
    NetworkState state;
    double scale = 0.0;
    for (std::size_t i = 0; i < edgeCount; ++i) {
        const auto& edge = edges[i];
        const auto edgeState = evaluateEdge(material, edge,
            potential[edge.from] - potential[edge.to]);
        if (!edgeState) return std::nullopt;
        state.flux[i] = edgeState->flux;
        state.permeance[i] = edgeState->differentialPermeance;
        state.fluxDensity[i] = edgeState->fluxDensity;
        state.residual[edge.from] += edgeState->flux;
        state.residual[edge.to] -= edgeState->flux;
        scale = std::max(scale, std::abs(edgeState->flux));
    }
    // Node 5 is the magnetic-potential gauge reference and its KCL equation
    // is dependent on the other five node equations.
    for (std::size_t i = 0; i < unknownCount; ++i)
        state.norm = std::max(state.norm, std::abs(state.residual[i]));
    (void)scale;
    return state;
}

bool solveLinearSystem(
    std::array<std::array<double, unknownCount + 1>, unknownCount>& matrix,
    std::array<double, unknownCount>& solution)
{
    for (std::size_t column = 0; column < unknownCount; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < unknownCount; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                pivot = row;
        }
        if (std::abs(matrix[pivot][column]) < 1.0e-24) return false;
        if (pivot != column) std::swap(matrix[pivot], matrix[column]);
        const double diagonal = matrix[column][column];
        for (std::size_t j = column; j <= unknownCount; ++j)
            matrix[column][j] /= diagonal;
        for (std::size_t row = 0; row < unknownCount; ++row) {
            if (row == column) continue;
            const double factor = matrix[row][column];
            for (std::size_t j = column; j <= unknownCount; ++j)
                matrix[row][j] -= factor * matrix[column][j];
        }
    }
    for (std::size_t i = 0; i < unknownCount; ++i)
        solution[i] = matrix[i][unknownCount];
    return true;
}

} // namespace

Result<ThreeLimbMagneticCycleSamplingResult>
sampleThreeLimbMagneticCycle(const ThreeLimbMagneticCycleSamplingRequest& request)
{
    if (!std::isfinite(request.relativePeakTolerance) ||
        request.relativePeakTolerance <= 0.0 ||
        request.relativePeakTolerance >= 1.0 ||
        (request.maximumSamples != 144 && request.maximumSamples != 288)) {
        Error error;
        error.code = ErrorCode::InvalidInput;
        error.message = "Adaptive sampling requires tolerance in (0,1) and maximumSamples of 144 or 288.";
        error.diagnostics.push_back(Diagnostic{
            Severity::Error, "THREE_LIMB_SAMPLING_INVALID", error.message,
            "sampling", std::nullopt, std::nullopt
        });
        return Result<ThreeLimbMagneticCycleSamplingResult>::failure(std::move(error));
    }
    for (const auto& phasor : request.phaseRmsPhasors) {
        if (!std::isfinite(phasor.real()) || !std::isfinite(phasor.imag())) {
            Error error;
            error.code = ErrorCode::InvalidInput;
            error.message = "Three-phase RMS current phasors must be finite.";
            error.diagnostics.push_back(Diagnostic{
                Severity::Error, "THREE_LIMB_SAMPLING_INVALID", error.message,
                "phaseRmsPhasors", std::nullopt, std::nullopt
            });
            return Result<ThreeLimbMagneticCycleSamplingResult>::failure(std::move(error));
        }
    }

    ThreeLimbMagneticCycleSamplingResult result;
    std::array<double, 7> previousPeaks{};
    bool havePrevious = false;
    constexpr double pi = 3.1415926535897932384626433832795;

    for (std::size_t sampleCount = 72; sampleCount <= request.maximumSamples;
         sampleCount *= 2) {
        std::array<double, 7> peaks{};
        for (std::size_t sample = 0; sample < sampleCount; ++sample) {
            const double angle = 2.0 * pi * static_cast<double>(sample) /
                                 static_cast<double>(sampleCount);
            const auto rotation = std::polar(1.0, angle);
            auto magneticRequest = request.magnetic;
            for (std::size_t phase = 0; phase < 3; ++phase) {
                const double instantaneousPeak =
                    std::sqrt(2.0) * std::real(request.phaseRmsPhasors[phase] * rotation);
                magneticRequest.phaseCurrents[phase] = units::Current{instantaneousPeak};
            }
            const auto magneticPoint = analyzeThreeLimbMagneticCircuit(magneticRequest);
            if (!magneticPoint)
                return Result<ThreeLimbMagneticCycleSamplingResult>::failure(
                    magneticPoint.error());

            for (std::size_t phase = 0; phase < 3; ++phase)
                peaks[phase] = std::max(peaks[phase],
                    std::abs(magneticPoint.value().limbFluxDensity[phase].si()));
            for (std::size_t yoke = 0; yoke < 4; ++yoke)
                peaks[yoke + 3] = std::max(peaks[yoke + 3],
                    std::abs(magneticPoint.value().yokeFluxDensity[yoke].si()));
        }

        result.samplesUsed = sampleCount;
        if (havePrevious) {
            double maximumRelativeChange = 0.0;
            for (std::size_t branch = 0; branch < peaks.size(); ++branch) {
                const double scale = std::max({std::abs(peaks[branch]),
                    std::abs(previousPeaks[branch]), 1.0e-12});
                maximumRelativeChange = std::max(maximumRelativeChange,
                    std::abs(peaks[branch] - previousPeaks[branch]) / scale);
            }
            result.relativePeakChange = maximumRelativeChange;
            result.converged = maximumRelativeChange <= request.relativePeakTolerance;
        }
        previousPeaks = peaks;
        havePrevious = true;
        if (result.converged) break;
    }

    for (std::size_t branch = 0; branch < previousPeaks.size(); ++branch)
        result.peakAbsoluteFluxDensity[branch] = units::FluxDensity{previousPeaks[branch]};
    return Result<ThreeLimbMagneticCycleSamplingResult>::success(std::move(result));
}

Result<ThreeLimbMagneticResult>
analyzeThreeLimbMagneticCircuit(const ThreeLimbMagneticRequest& request)
{
    const auto& g = request.geometry;
    std::array<double, 3> limbAreas{};
    std::array<double, 3> limbPaths{};
    std::array<double, 3> limbGaps{};
    std::array<double, 4> yokeAreas{};
    std::array<double, 4> yokePaths{};
    for (std::size_t i = 0; i < limbAreas.size(); ++i) {
        const double area = g.limbEffectiveAreas[i]
            ? g.limbEffectiveAreas[i]->si() : g.limbEffectiveArea.si();
        const double path = g.limbMeanPaths[i]
            ? g.limbMeanPaths[i]->si() : g.limbMeanPath.si();
        const double gap = g.limbAirGaps[i]
            ? g.limbAirGaps[i]->si() : g.limbAirGap.si();
        limbAreas[i] = area;
        limbPaths[i] = path;
        limbGaps[i] = gap;
    }
    for (std::size_t i = 0; i < yokeAreas.size(); ++i) {
        yokeAreas[i] = g.yokeEffectiveAreas[i]
            ? g.yokeEffectiveAreas[i]->si() : g.yokeEffectiveArea.si();
        yokePaths[i] = g.yokeSegmentPaths[i]
            ? g.yokeSegmentPaths[i]->si() : g.yokeSegmentPath.si();
    }
    const bool validLimbs =
        std::all_of(limbAreas.begin(), limbAreas.end(),
            [](double value) { return std::isfinite(value) && value > 0.0; }) &&
        std::all_of(limbPaths.begin(), limbPaths.end(),
            [](double value) { return std::isfinite(value) && value > 0.0; }) &&
        std::all_of(limbGaps.begin(), limbGaps.end(),
            [](double value) { return std::isfinite(value) && value >= 0.0; });
    const bool validYokes =
        std::all_of(yokeAreas.begin(), yokeAreas.end(),
            [](double value) { return std::isfinite(value) && value > 0.0; }) &&
        std::all_of(yokePaths.begin(), yokePaths.end(),
            [](double value) { return std::isfinite(value) && value > 0.0; });
    if (!validLimbs || !validYokes ||
        request.maximumIterations == 0 ||
        !std::isfinite(request.relativeFluxTolerance) ||
        request.relativeFluxTolerance <= 0.0 ||
        request.relativeFluxTolerance >= 1.0)
        return fail(ErrorCode::InvalidInput,
            "Every limb/yoke branch geometry and solver tolerance must be finite and physically valid.",
            "geometry");

    for (const auto turns : request.turns) {
        if (turns == 0)
            return fail(ErrorCode::InvalidInput,
                "Each phase winding must have a positive turn count.", "turns");
    }
    const auto& material = request.material;
    if (material.grade.empty() || material.sourceReference.empty() ||
        material.datasetRevision.empty() ||
        (material.dataSource == MaterialDataSource::ManufacturerDatasheet &&
         material.manufacturer.empty()))
        return fail(ErrorCode::MissingRequiredData,
            "B-H material grade, provenance and dataset revision are required.",
            "material.provenance");
    if (material.bhCurve.size() < 2)
        return fail(ErrorCode::MissingMaterialProperty,
            "At least two measured B-H points are required.", "material.bhCurve");
    if (!std::isfinite(material.bhCurve.front().fieldStrength.si()) ||
        !std::isfinite(material.bhCurve.front().fluxDensity.si()) ||
        std::abs(material.bhCurve.front().fieldStrength.si()) > 1.0e-9 ||
        std::abs(material.bhCurve.front().fluxDensity.si()) > 1.0e-12)
        return fail(ErrorCode::MissingMaterialProperty,
            "The positive B-H curve must start at (H=0, B=0).", "material.bhCurve");
    for (std::size_t i = 1; i < material.bhCurve.size(); ++i) {
        const double h = material.bhCurve[i].fieldStrength.si();
        const double b = material.bhCurve[i].fluxDensity.si();
        if (!std::isfinite(h) || !std::isfinite(b) ||
            h <= material.bhCurve[i - 1].fieldStrength.si() ||
            b <= material.bhCurve[i - 1].fluxDensity.si())
            return fail(ErrorCode::InvalidInput,
                "B-H points must be finite and strictly increasing in H and B.",
                "material.bhCurve");
    }

    std::array<Edge, edgeCount> edges{};
    for (std::size_t phase = 0; phase < 3; ++phase) {
        const double current = request.phaseCurrents[phase].si();
        if (!std::isfinite(current))
            return fail(ErrorCode::InvalidInput,
                "Phase currents must be finite.", "phaseCurrents");
        edges[phase] = Edge{
            3U + phase, phase, limbAreas[phase], limbPaths[phase], limbGaps[phase],
            static_cast<double>(request.turns[phase]) * current
        };
    }
    edges[3] = Edge{0, 1, yokeAreas[0], yokePaths[0], 0.0, 0.0};
    edges[4] = Edge{1, 2, yokeAreas[1], yokePaths[1], 0.0, 0.0};
    edges[5] = Edge{3, 4, yokeAreas[2], yokePaths[2], 0.0, 0.0};
    edges[6] = Edge{4, 5, yokeAreas[3], yokePaths[3], 0.0, 0.0};

    std::array<double, nodeCount> potential{};
    // Seed each top-limb node with its winding MMF. This removes most of the
    // direct limb excitation before Newton balances the shared yoke network.
    for (std::size_t phase = 0; phase < 3; ++phase)
        potential[phase] = edges[phase].mmf;
    auto state = evaluateNetwork(material, edges, potential);
    if (!state)
        return fail(ErrorCode::ModelOutOfDomain,
            "Zero-state initialization is outside the supplied B-H domain.",
            "material.bhCurve");
    const double fluxScale = std::max(1.0e-15,
        std::max({limbArea, yokeArea}) * material.bhCurve.back().fluxDensity.si());
    const double tolerance = request.relativeFluxTolerance * fluxScale;

    std::size_t iterations = 0;
    for (; iterations < request.maximumIterations && state->norm > tolerance; ++iterations) {
        std::array<std::array<double, unknownCount + 1>, unknownCount> matrix{};
        for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
            const auto& edge = edges[edgeIndex];
            const double permeance = state->permeance[edgeIndex];
            const std::size_t a = edge.from;
            const std::size_t b = edge.to;
            if (a < unknownCount) matrix[a][a] += permeance;
            if (b < unknownCount) matrix[b][b] += permeance;
            if (a < unknownCount && b < unknownCount) {
                matrix[a][b] -= permeance;
                matrix[b][a] -= permeance;
            }
        }
        for (std::size_t i = 0; i < unknownCount; ++i)
            matrix[i][unknownCount] = -state->residual[i];
        std::array<double, unknownCount> delta{};
        if (!solveLinearSystem(matrix, delta))
            return fail(ErrorCode::SingularSystem,
                "Three-limb magnetic reluctance network Jacobian is singular.",
                "solver.jacobian");

        bool accepted = false;
        double damping = 1.0;
        for (int lineSearch = 0; lineSearch < 24; ++lineSearch) {
            auto candidatePotential = potential;
            for (std::size_t i = 0; i < unknownCount; ++i)
                candidatePotential[i] += damping * delta[i];
            candidatePotential[5] = 0.0;
            auto candidate = evaluateNetwork(material, edges, candidatePotential);
            if (candidate && candidate->norm < state->norm) {
                potential = candidatePotential;
                state = std::move(candidate);
                accepted = true;
                break;
            }
            damping *= 0.5;
        }
        if (!accepted)
            return fail(ErrorCode::NonConvergence,
                "Three-limb magnetic network Newton iteration could not find an in-domain descent step.",
                "solver.iteration");
    }
    if (state->norm > tolerance)
        return fail(ErrorCode::NonConvergence,
            "Three-limb magnetic network did not converge within the iteration limit.",
            "solver.iteration");

    ThreeLimbMagneticResult result;
    for (std::size_t i = 0; i < 3; ++i) {
        result.limbFluxDensity[i] = units::FluxDensity{state->fluxDensity[i]};
        result.limbFlux[i] = units::MagneticFlux{state->flux[i]};
    }
    for (std::size_t i = 0; i < 4; ++i) {
        result.yokeFluxDensity[i] = units::FluxDensity{state->fluxDensity[i + 3]};
        result.yokeFlux[i] = units::MagneticFlux{state->flux[i + 3]};
    }
    for (std::size_t i = 0; i < nodeCount; ++i)
        result.nodeMagneticPotential[i] = units::MagneticPotential{potential[i]};
    result.iterations = iterations;
    result.maximumFluxResidual_Wb = state->norm;
    result.withinBhCurveRange = true;
    return Result<ThreeLimbMagneticResult>::success(std::move(result));
}

} // namespace p135::domain

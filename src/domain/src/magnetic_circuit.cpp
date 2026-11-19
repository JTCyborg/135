#include "p135/domain/magnetic_circuit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace p135::domain {
namespace {
constexpr double vacuumPermeability = 4.0e-7 * 3.14159265358979323846;

Result<MagneticCircuitResult> fail(ErrorCode code, std::string message,
                                   std::string field)
{
    Error error;
    error.code = code;
    error.message = std::move(message);
    error.diagnostics.push_back(Diagnostic{
        Severity::Error, "MAGNETIC_CIRCUIT_INVALID", error.message,
        std::move(field), std::nullopt, std::nullopt
    });
    return Result<MagneticCircuitResult>::failure(std::move(error));
}

struct BhInterpolated {
    double h{};
    double slopeHPerT{};
};

std::optional<BhInterpolated> interpolateBh(const CoreMaterial& material, double b)
{
    const auto& curve = material.bhCurve;
    if (curve.size() < 2 || b < curve.front().fluxDensity.si() ||
        b > curve.back().fluxDensity.si())
        return std::nullopt;

    for (std::size_t i = 1; i < curve.size(); ++i) {
        const double b0 = curve[i - 1].fluxDensity.si();
        const double b1 = curve[i].fluxDensity.si();
        if (b <= b1) {
            const double h0 = curve[i - 1].fieldStrength.si();
            const double h1 = curve[i].fieldStrength.si();
            const double slope = (h1 - h0) / (b1 - b0);
            if (!std::isfinite(slope) || slope <= 0.0) return std::nullopt;
            return BhInterpolated{h0 + (b - b0) * slope, slope};
        }
    }
    return std::nullopt;
}
} // namespace

Result<MagneticCircuitResult>
analyzeMagneticCircuit(const MagneticCircuitRequest& request)
{
    const auto& core = request.core;
    const double area = core.effectiveArea.si();
    const double path = core.meanMagneticPath.si();
    const double gap = core.airGap.si();
    const double current = std::abs(request.current.si());
    if (request.turns == 0 || !std::isfinite(current) ||
        !std::isfinite(area) || area <= 0.0 ||
        !std::isfinite(path) || path <= 0.0 ||
        !std::isfinite(gap) || gap < 0.0)
        return fail(ErrorCode::InvalidInput,
                    "Turns, current and core geometry must be finite and physically valid.",
                    "request");
    if (!std::isfinite(request.kneeSlopeRatio) ||
        request.kneeSlopeRatio <= 0.0 || request.kneeSlopeRatio >= 1.0)
        return fail(ErrorCode::InvalidInput,
                    "Knee slope ratio must be strictly between zero and one.",
                    "kneeSlopeRatio");
    const auto materialCheck = validate(request.material);
    if (!materialCheck)
        return Result<MagneticCircuitResult>::failure(materialCheck.error());

    const auto& bh = request.material.bhCurve;
    const double bMin = bh.front().fluxDensity.si();
    const double bMax = bh.back().fluxDensity.si();
    if (bMin > 0.0)
        return fail(ErrorCode::MissingMaterialProperty,
                    "B-H curve must include a zero-flux origin for this solver.",
                    "material.bhCurve");

    const double mmf = static_cast<double>(request.turns) * current;
    const auto requiredMmf = [&](double b) -> double {
        const auto point = interpolateBh(request.material, b);
        if (!point) return std::numeric_limits<double>::quiet_NaN();
        return point->h * path + b * gap / vacuumPermeability;
    };
    const double maximumMmf = requiredMmf(bMax);
    if (!std::isfinite(maximumMmf) || mmf > maximumMmf)
        return fail(ErrorCode::ModelOutOfDomain,
                    "Requested current drives flux beyond the measured B-H curve; no extrapolation was performed.",
                    "current");

    double low = bMin;
    double high = bMax;
    for (int iteration = 0; iteration < 100; ++iteration) {
        const double middle = 0.5 * (low + high);
        if (requiredMmf(middle) < mmf) low = middle;
        else high = middle;
    }
    const double b = 0.5 * (low + high);
    const auto operating = interpolateBh(request.material, b);
    if (!operating)
        return fail(ErrorCode::ModelOutOfDomain,
                    "Operating point cannot be interpolated from the supplied B-H curve.",
                    "material.bhCurve");

    MagneticCircuitResult result;
    result.current = units::Current{current};
    result.fluxDensity = units::FluxDensity{b};
    result.coreFieldStrength = units::MagneticFieldStrength{operating->h};
    result.gapFieldStrength = units::MagneticFieldStrength{b / vacuumPermeability};
    result.flux = units::MagneticFlux{b * area};
    result.withinBhCurveRange = true;

    const double reluctanceDifferential =
        path * operating->slopeHPerT / area + gap / (vacuumPermeability * area);
    if (current > 0.0) {
        result.secantInductance = units::Inductance{
            static_cast<double>(request.turns) * b * area / current};
    }
    if (reluctanceDifferential > 0.0 && std::isfinite(reluctanceDifferential)) {
        result.differentialInductance = units::Inductance{
            static_cast<double>(request.turns) * request.turns / reluctanceDifferential};
    }

    const double initialSlope = (bh[1].fieldStrength.si() - bh[0].fieldStrength.si()) /
        (bh[1].fluxDensity.si() - bh[0].fluxDensity.si());
    for (std::size_t i = 1; i < bh.size(); ++i) {
        const double segmentSlope =
            (bh[i].fieldStrength.si() - bh[i - 1].fieldStrength.si()) /
            (bh[i].fluxDensity.si() - bh[i - 1].fluxDensity.si());
        if (segmentSlope <= initialSlope * request.kneeSlopeRatio) {
            const double kneeB = bh[i - 1].fluxDensity.si();
            const double kneeH = bh[i - 1].fieldStrength.si();
            const double kneeMmf = kneeH * path + kneeB * gap / vacuumPermeability;
            result.kneeFluxDensity = units::FluxDensity{kneeB};
            result.kneeCurrent = units::Current{
                kneeMmf / static_cast<double>(request.turns)};
            if (kneeB > 0.0)
                result.kneeUtilization = b / kneeB;
            break;
        }
    }
    return Result<MagneticCircuitResult>::success(std::move(result));
}

} // namespace p135::domain

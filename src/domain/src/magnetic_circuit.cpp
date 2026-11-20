#include "p135/domain/magnetic_circuit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
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
    if (curve.size() < 2 || !std::isfinite(b) ||
        b < curve.front().fluxDensity.si())
        return std::nullopt;
    if (b > curve.back().fluxDensity.si()) {
        const double bLast = curve.back().fluxDensity.si();
        const double hLast = curve.back().fieldStrength.si();
        return BhInterpolated{
            hLast + (b - bLast) / vacuumPermeability,
            1.0 / vacuumPermeability
        };
    }

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
    const double gapArea = request.gapEffectiveArea ? request.gapEffectiveArea->si() : area;
    if (request.turns == 0 || !std::isfinite(current) ||
        !std::isfinite(area) || area <= 0.0 ||
        !std::isfinite(path) || path <= 0.0 ||
        !std::isfinite(gap) || gap < 0.0 ||
        !std::isfinite(gapArea) || gapArea < area)
        return fail(ErrorCode::InvalidInput,
                    "Turns, current and core geometry must be finite and physically valid.",
                    "request");
    if (!std::isfinite(request.kneeSlopeRatio) ||
        request.kneeSlopeRatio <= 0.0 || request.kneeSlopeRatio >= 1.0)
        return fail(ErrorCode::InvalidInput,
                    "Knee slope ratio must be strictly between zero and one.",
                    "kneeSlopeRatio");
    const auto& material = request.material;
    if (material.grade.empty() || material.sourceReference.empty() ||
        material.datasetRevision.empty() ||
        (material.dataSource == MaterialDataSource::ManufacturerDatasheet &&
         material.manufacturer.empty()))
        return fail(ErrorCode::MissingRequiredData,
                    "B-H grade, provenance, dataset revision and datasheet manufacturer (when applicable) are required.",
                    "material.provenance");

    const auto& bh = material.bhCurve;
    if (bh.size() < 2)
        return fail(ErrorCode::MissingMaterialProperty,
                    "At least two B-H points are required.",
                    "material.bhCurve");
    const double bMin = bh.front().fluxDensity.si();
    const double bMax = bh.back().fluxDensity.si();
    if (!std::isfinite(bMin) || !std::isfinite(bh.front().fieldStrength.si()) ||
        std::abs(bMin) > 1.0e-12 ||
        std::abs(bh.front().fieldStrength.si()) > 1.0e-9)
        return fail(ErrorCode::MissingMaterialProperty,
                    "B-H curve must include the (H=0, B=0) origin.",
                    "material.bhCurve");
    for (std::size_t i = 1; i < bh.size(); ++i) {
        if (!std::isfinite(bh[i].fieldStrength.si()) ||
            !std::isfinite(bh[i].fluxDensity.si()) ||
            bh[i].fieldStrength.si() <= bh[i - 1].fieldStrength.si() ||
            bh[i].fluxDensity.si() <= bh[i - 1].fluxDensity.si())
            return fail(ErrorCode::InvalidInput,
                        "B-H points must be strictly increasing in both H and B.",
                        "material.bhCurve");
    }

    const double mmf = static_cast<double>(request.turns) * current;
    const auto requiredMmf = [&](double b) -> double {
        const auto point = interpolateBh(request.material, b);
        if (!point) return std::numeric_limits<double>::quiet_NaN();
        return point->h * path + b * area * gap / (vacuumPermeability * gapArea);
    };
    double b = 0.0;
    if (mmf > 0.0) {
        double low = bMin;
        double high = bMax;
        // Continue beyond the last measured point with dH/dB = 1/μ0.
        // The returned range flag distinguishes this asymptote from measured data.
        for (int expansion = 0; expansion < 64; ++expansion) {
            const double highMmf = requiredMmf(high);
            if (!std::isfinite(highMmf))
                return fail(ErrorCode::ModelOutOfDomain,
                    "Magnetic-circuit MMF bracket became non-finite.", "current");
            if (highMmf >= mmf) break;
            high *= 2.0;
            if (!std::isfinite(high) || high <= 0.0 || expansion == 63)
                return fail(ErrorCode::ModelOutOfDomain,
                    "Unable to bracket the magnetic operating point.", "current");
        }
        for (int iteration = 0; iteration < 120; ++iteration) {
            const double middle = 0.5 * (low + high);
            if (requiredMmf(middle) < mmf) low = middle;
            else high = middle;
        }
        b = 0.5 * (low + high);
    }
    const auto operating = interpolateBh(request.material, b);
    if (!operating)
        return fail(ErrorCode::ModelOutOfDomain,
                    "Operating point cannot be interpolated from the supplied B-H curve.",
                    "material.bhCurve");

    MagneticCircuitResult result;
    result.current = units::Current{current};
    result.fluxDensity = units::FluxDensity{b};
    result.coreFieldStrength = units::MagneticFieldStrength{operating->h};
    result.gapFieldStrength = units::MagneticFieldStrength{b * area / (gapArea * vacuumPermeability)};
    result.flux = units::MagneticFlux{b * area};
    result.coreAreaAtGap = units::Area{area};
    result.effectiveGapArea = units::Area{gapArea};
    result.gapFringingFactor = gapArea / area;
    result.withinBhCurveRange = b <= bMax;

    const double reluctanceDifferential =
        path * operating->slopeHPerT / area + gap / (vacuumPermeability * gapArea);
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
        if (segmentSlope >= initialSlope / request.kneeSlopeRatio) {
            const double kneeB = bh[i - 1].fluxDensity.si();
            const double kneeH = bh[i - 1].fieldStrength.si();
            const double kneeMmf = kneeH * path + kneeB * area * gap / (vacuumPermeability * gapArea);
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

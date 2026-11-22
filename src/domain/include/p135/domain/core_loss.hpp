#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"
#include "p135/domain/magnetic_product.hpp"

#include <array>
#include <string>
#include <vector>

namespace p135::domain {

/// One denominator term c / B^n in a manufacturer core-loss equation.
/// Coefficients use the source document's declared units.
struct CoreLossEquationTerm {
    double coefficient{};
    double fluxDensityExponent{};
};

/// Manufacturer equation of the form:
/// Pv = f / sum(c_i / B^n_i) + d * B^2 * f^2.
/// Axis scaling and output conversion are explicit because datasheets often
/// publish equations in non-SI units (for example gauss and mW/cm^3).
struct CoreLossEquation {
    std::vector<CoreLossEquationTerm> denominatorTerms;
    double eddyCoefficient{};
    double minimumFrequency_Hz{};
    double maximumFrequency_Hz{};
    double minimumFluxDensity_T{};
    double maximumFluxDensity_T{};
    double sourceFluxDensityUnitsPerTesla{};
    double outputWattsPerM3PerSourceUnit{};
    std::string sourceReference;
    std::string datasetRevision;
};

/// Evaluates only inside the published equation domain. No extrapolation.
[[nodiscard]] Result<units::VolumetricCoreLossDensity> evaluateCoreLossEquation(
    const CoreLossEquation& equation,
    units::Frequency frequency,
    units::FluxDensity peakFluxDensity);

/// Trilinearly interpolates manufacturer/measured specific core-loss data
/// (W/kg) on the supplied frequency, peak-flux-density and temperature grid.
/// Every required grid corner must exist; extrapolation is never performed.
[[nodiscard]] Result<units::CoreLossDensity> evaluateCoreLossData(
    const CoreMaterial& material,
    units::Frequency frequency,
    units::FluxDensity peakFluxDensity,
    units::TemperatureKelvin temperature);

/// Converts measured specific core loss (W/kg) to total core loss (W)
/// using an explicitly supplied core mass and its source reference.
struct CoreLossPowerRequest {
    units::Frequency frequency;
    units::FluxDensity peakFluxDensity;
    units::TemperatureKelvin temperature;
    units::Mass coreMass;
    std::string coreMassSourceReference;
};

struct CoreLossPowerResult {
    units::CoreLossDensity specificLoss;
    units::Mass coreMass;
    units::Power totalLoss;
    std::string lossDataReference;
    std::string coreMassSourceReference;
};

[[nodiscard]] Result<CoreLossPowerResult> calculateCoreLossPower(
    const CoreMaterial& material,
    const CoreLossPowerRequest& request);

/// Seven-region order: limb A, limb B, limb C, top yoke AB, top yoke BC,
/// bottom yoke AB, bottom yoke BC. Frequency is common to all regions;
/// peak flux density, temperature and explicit physical mass are region-specific.
struct ThreeLimbCoreLossRequest {
    units::Frequency frequency;
    std::array<units::FluxDensity, 7> peakFluxDensities{};
    std::array<units::TemperatureKelvin, 7> temperatures{};
    std::array<units::Mass, 7> regionMasses{};
    std::array<std::string, 7> regionMassSourceReferences{};
    /// Independent review status for the physical mass calculation. A
    /// provenance string alone never promotes a mass estimate to VERIFIED.
    std::string coreMassVerificationStatus{"NOT_VERIFIED"};
};

struct ThreeLimbCoreLossResult {
    std::array<CoreLossPowerResult, 7> regions{};
    units::Power totalLoss;
};

/// Evaluates every region against the same material loss dataset. The
/// aggregate fails if any region is invalid or lacks mass provenance.
[[nodiscard]] Result<ThreeLimbCoreLossResult> calculateThreeLimbCoreLossPower(
    const CoreMaterial& material,
    const ThreeLimbCoreLossRequest& request);

} // namespace p135::domain

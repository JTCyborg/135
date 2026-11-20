#include "p135/domain/magnetic_product.hpp"
#include "p135/domain/core_loss.hpp"
#include "p135/domain/initial_magnetization.hpp"
#include "p135/domain/magnetic_circuit.hpp"
#include "p135/domain/three_limb_magnetic.hpp"
#include "p135/domain/material_catalog.hpp"

#include <cassert>
#include <complex>
#include <cmath>
#include <limits>

using namespace p135;
using namespace p135::domain;

namespace {
MagneticProduct validProduct()
{
    MagneticProduct product;
    product.name = "Test inductor";
    product.core.effectiveArea = units::Area{4.0e-4};
    product.core.windowArea = units::Area{8.0e-4};
    product.core.meanMagneticPath = units::Length{0.25};
    product.core.airGap = units::Length{1.0e-3};
    product.core.stackingFactor = 0.95;
    product.coreMaterial.id = "MAT-TEST";
    product.coreMaterial.grade = "Test grade";
    product.coreMaterial.manufacturer = "Test manufacturer";
    product.coreMaterial.sourceReference = "test fixture";
    product.coreMaterial.datasetRevision = "1";
    product.coreMaterial.density = units::Density{7650.0};
    product.coreMaterial.bhCurve = {
        {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
        {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.8}},
        {units::MagneticFieldStrength{500.0}, units::FluxDensity{1.2}}
    };
    Winding winding;
    winding.name = "Main";
    winding.turns = 100;
    winding.turnsPerLayer = 25;
    winding.layers = 4;
    winding.conductor.id = "CU-1";
    winding.conductor.bareCrossSection = units::Area{2.5e-6};
    winding.bobbin.material = "PBT";
    winding.bobbin.innerWidth = units::Length{0.04};
    winding.bobbin.innerHeight = units::Length{0.03};
    winding.bobbin.wallThickness = units::Length{0.001};
    product.windings.push_back(winding);
    return product;
}
}

int main()
{
    assert(validate(validProduct()));

    {
        MagneticCircuitRequest request;
        request.core.effectiveArea = units::Area{4.0e-4};
        request.core.meanMagneticPath = units::Length{0.25};
        request.core.airGap = units::Length{1.0e-3};
        request.turns = 100;
        request.current = units::Current{1.0};
        request.material = validProduct().coreMaterial;
        request.material.density = units::Density{}; // Density is not required by the B-H magnetic-circuit equations.
        request.material.bhCurve = {
            {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
            {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.5}},
            {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.0}}
        };
        auto zeroCurrentRequest = request;
        zeroCurrentRequest.current = units::Current{0.0};
        const auto zeroCurrent = analyzeMagneticCircuit(zeroCurrentRequest);
        assert(zeroCurrent);
        assert(std::abs(zeroCurrent.value().fluxDensity.si()) < 1.0e-12);
        assert(std::abs(zeroCurrent.value().flux.si()) < 1.0e-15);
        assert(zeroCurrent.value().kneeCurrent.has_value());

        auto reverseCurrent = request;
        reverseCurrent.current = units::Current{-1.0};
        const auto reverseResult = analyzeMagneticCircuit(reverseCurrent);
        assert(reverseResult);

        const auto operating = analyzeMagneticCircuit(request);
        assert(operating);
        assert(operating.value().withinBhCurveRange);
        assert(std::abs(reverseResult.value().fluxDensity.si() -
                        operating.value().fluxDensity.si()) < 1.0e-12);
        assert(operating.value().fluxDensity.si() > 0.0);
        assert(operating.value().flux.si() > 0.0);
        assert(std::abs(operating.value().gapFringingFactor - 1.0) < 1.0e-12);
        auto withFringing = request;
        withFringing.gapEffectiveArea = units::Area{5.0e-4};
        const auto fringed = analyzeMagneticCircuit(withFringing);
        assert(fringed);
        assert(fringed.value().fluxDensity.si() > operating.value().fluxDensity.si());
        assert(std::abs(fringed.value().gapFringingFactor - 1.25) < 1.0e-12);
        assert(fringed.value().differentialInductance.si() > operating.value().differentialInductance.si());
        auto invalidGapArea = request;
        invalidGapArea.gapEffectiveArea = units::Area{3.0e-4};
        assert(!analyzeMagneticCircuit(invalidGapArea));
        assert(operating.value().secantInductance.si() > 0.0);
        assert(operating.value().differentialInductance.si() > 0.0);
        assert(operating.value().kneeFluxDensity.has_value());
        assert(operating.value().kneeCurrent.has_value());
        assert(operating.value().kneeUtilization.has_value());
        assert(std::abs(operating.value().kneeFluxDensity->si() - 0.5) < 1.0e-12);
        assert(operating.value().kneeCurrent->si() > 4.0);

        auto atKnee = request;
        atKnee.current = *operating.value().kneeCurrent;
        const auto kneeResult = analyzeMagneticCircuit(atKnee);
        assert(kneeResult);
        assert(std::abs(kneeResult.value().fluxDensity.si() - 0.5) < 1.0e-9);

        auto beyondCurve = request;
        beyondCurve.current = units::Current{11.0};
        const auto outOfDomain = analyzeMagneticCircuit(beyondCurve);
        assert(outOfDomain);
        assert(!outOfDomain.value().withinBhCurveRange);
        assert(outOfDomain.value().fluxDensity.si() >
               request.material.bhCurve.back().fluxDensity.si());
        assert(std::isfinite(outOfDomain.value().coreFieldStrength.si()));
        assert(std::isfinite(outOfDomain.value().differentialInductance.si()));

        auto invalidRatio = request;
        invalidRatio.kneeSlopeRatio = 1.0;
        assert(!analyzeMagneticCircuit(invalidRatio));

        auto invalidTurns = request;
        invalidTurns.turns = 0;
        assert(!analyzeMagneticCircuit(invalidTurns));

        auto missingOrigin = request;
        missingOrigin.material.bhCurve.front().fluxDensity = units::FluxDensity{0.01};
        const auto missingOriginResult = analyzeMagneticCircuit(missingOrigin);
        assert(!missingOriginResult);
        assert(missingOriginResult.error().code == ErrorCode::MissingMaterialProperty);
    }


    {
        ThreeLimbMagneticRequest request;
        request.geometry.limbEffectiveArea = units::Area{4.0e-4};
        request.geometry.limbMeanPath = units::Length{0.25};
        request.geometry.limbAirGap = units::Length{1.0e-3};
        request.geometry.yokeEffectiveArea = units::Area{4.0e-4};
        request.geometry.yokeSegmentPath = units::Length{0.15};
        request.turns = {100, 100, 100};
        request.phaseCurrents = {
            units::Current{0.0}, units::Current{0.0}, units::Current{0.0}
        };
        request.material = validProduct().coreMaterial;
        request.material.density = units::Density{};
        request.material.bhCurve = {
            {units::MagneticFieldStrength{0.0}, units::FluxDensity{0.0}},
            {units::MagneticFieldStrength{100.0}, units::FluxDensity{0.5}},
            {units::MagneticFieldStrength{1000.0}, units::FluxDensity{1.0}}
        };
        const auto zero = analyzeThreeLimbMagneticCircuit(request);
        assert(zero);
        assert(zero.value().withinBhCurveRange);
        for (const auto flux : zero.value().limbFlux)
            assert(std::abs(flux.si()) < 1.0e-15);
        for (const auto flux : zero.value().yokeFlux)
            assert(std::abs(flux.si()) < 1.0e-15);

        request.phaseCurrents = {
            units::Current{0.2}, units::Current{-0.1}, units::Current{-0.1}
        };
        const auto balanced = analyzeThreeLimbMagneticCircuit(request);
        assert(balanced);
        assert(balanced.value().withinBhCurveRange);
        auto fringedThreeLimb = request;
        fringedThreeLimb.geometry.limbGapEffectiveAreas[0] = units::Area{5.0e-4};
        const auto fringedThreeLimbResult = analyzeThreeLimbMagneticCircuit(fringedThreeLimb);
        assert(fringedThreeLimbResult);
        assert(std::abs(fringedThreeLimbResult.value().limbFluxDensity[0].si() - balanced.value().limbFluxDensity[0].si()) > 1.0e-8);
        auto invalidThreeLimbGapArea = request;
        invalidThreeLimbGapArea.geometry.limbGapEffectiveAreas[0] = units::Area{3.0e-4};
        assert(!analyzeThreeLimbMagneticCircuit(invalidThreeLimbGapArea));
        assert(balanced.value().iterations > 0);
        assert(balanced.value().maximumFluxResidual_Wb < 1.0e-12);
        double limbFluxSum = 0.0;
        for (const auto flux : balanced.value().limbFlux) {
            assert(std::isfinite(flux.si()));
            limbFluxSum += flux.si();
        }
        assert(std::abs(limbFluxSum) < 1.0e-12);
        for (const auto density : balanced.value().limbFluxDensity)
            assert(std::isfinite(density.si()));
        for (const auto density : balanced.value().yokeFluxDensity)
            assert(std::isfinite(density.si()));

        // Explicitly populated symmetric arrays must reproduce the legacy
        // scalar geometry exactly.
        auto explicitSymmetric = request;
        explicitSymmetric.geometry.limbEffectiveAreas.fill(request.geometry.limbEffectiveArea);
        explicitSymmetric.geometry.limbMeanPaths.fill(request.geometry.limbMeanPath);
        explicitSymmetric.geometry.limbAirGaps.fill(request.geometry.limbAirGap);
        explicitSymmetric.geometry.yokeEffectiveAreas.fill(request.geometry.yokeEffectiveArea);
        explicitSymmetric.geometry.yokeSegmentPaths.fill(request.geometry.yokeSegmentPath);
        const auto symmetricArrays = analyzeThreeLimbMagneticCircuit(explicitSymmetric);
        assert(symmetricArrays);
        for (std::size_t i = 0; i < 3; ++i)
            assert(std::abs(symmetricArrays.value().limbFluxDensity[i].si() -
                            balanced.value().limbFluxDensity[i].si()) < 1.0e-12);
        for (std::size_t i = 0; i < 4; ++i)
            assert(std::abs(symmetricArrays.value().yokeFluxDensity[i].si() -
                            balanced.value().yokeFluxDensity[i].si()) < 1.0e-12);

        // Unequal limb/yoke sections and independent gaps are accepted and
        // alter the solved branch state rather than being silently ignored.
        auto asymmetric = request;
        asymmetric.geometry.limbEffectiveAreas[0] = units::Area{5.0e-4};
        asymmetric.geometry.limbMeanPaths[1] = units::Length{0.28};
        asymmetric.geometry.limbAirGaps[2] = units::Length{1.5e-3};
        asymmetric.geometry.limbAirGaps[0] = units::Length{0.0}; // Explicit ungapped limb.
        asymmetric.geometry.yokeEffectiveAreas[0] = units::Area{5.5e-4};
        asymmetric.geometry.yokeSegmentPaths[3] = units::Length{0.18};
        asymmetric.turns = {100, 105, 95};
        const auto asymmetricResult = analyzeThreeLimbMagneticCircuit(asymmetric);
        assert(asymmetricResult);
        assert(asymmetricResult.value().withinBhCurveRange);
        assert(asymmetricResult.value().maximumFluxResidual_Wb < 1.0e-12);
        bool changedLimbState = false;
        for (std::size_t i = 0; i < 3; ++i) {
            assert(std::isfinite(asymmetricResult.value().limbFluxDensity[i].si()));
            changedLimbState = changedLimbState ||
                std::abs(asymmetricResult.value().limbFluxDensity[i].si() -
                         balanced.value().limbFluxDensity[i].si()) > 1.0e-8;
        }
        assert(changedLimbState);

        ThreeLimbMagneticCycleSamplingRequest sampling;
        sampling.magnetic = request;
        sampling.phaseRmsPhasors = {
            std::complex<double>{0.2, 0.0},
            std::polar(0.12, -1.73),
            std::polar(0.11,  2.31)
        };
        sampling.relativePeakTolerance = 1.0e-3;
        sampling.maximumSamples = 288;
        const auto convergedCycle = sampleThreeLimbMagneticCycle(sampling);
        assert(convergedCycle);
        assert(convergedCycle.value().converged);
        assert(convergedCycle.value().samplesUsed == 144 ||
               convergedCycle.value().samplesUsed == 288);
        assert(convergedCycle.value().relativePeakChange <=
               sampling.relativePeakTolerance);
        for (const auto density : convergedCycle.value().peakAbsoluteFluxDensity)
            assert(std::isfinite(density.si()) && density.si() >= 0.0);

        // The operating-point sampler must superpose harmonics in the
        // instantaneous waveform, not treat each order as an independent sweep.
        auto multiHarmonicSampling = sampling;
        multiHarmonicSampling.harmonicExcitations = {
            {1, sampling.phaseRmsPhasors},
            {5, {
                std::complex<double>{0.08, 0.0},
                std::polar(0.05, 1.2),
                std::polar(0.04, -1.1)
            }}
        };
        const auto multiHarmonicCycle =
            sampleThreeLimbMagneticCycle(multiHarmonicSampling);
        assert(multiHarmonicCycle);
        assert(multiHarmonicCycle.value().converged);
        assert(multiHarmonicCycle.value().peakAbsoluteFluxDensity[0].si() >
               convergedCycle.value().peakAbsoluteFluxDensity[0].si());

        // DC bias is superimposed on each instantaneous AC phase current.
        auto dcBiasedSampling = sampling;
        dcBiasedSampling.phaseDcCurrents[0] = units::Current{0.05};
        const auto dcBiasedCycle = sampleThreeLimbMagneticCycle(dcBiasedSampling);
        assert(dcBiasedCycle);
        assert(std::isfinite(dcBiasedCycle.value().peakAbsoluteFluxDensity[0].si()));
        assert(dcBiasedCycle.value().peakAbsoluteFluxDensity[0].si() >
               convergedCycle.value().peakAbsoluteFluxDensity[0].si());

        auto invalidDcBias = sampling;
        invalidDcBias.phaseDcCurrents[1] =
            units::Current{std::numeric_limits<double>::infinity()};
        assert(!sampleThreeLimbMagneticCycle(invalidDcBias));

        auto strictSampling = sampling;
        strictSampling.relativePeakTolerance = 1.0e-14;
        strictSampling.maximumSamples = 144;
        const auto cappedCycle = sampleThreeLimbMagneticCycle(strictSampling);
        assert(cappedCycle);
        assert(!cappedCycle.value().converged);
        assert(cappedCycle.value().samplesUsed == 144);

        auto extendedSampling = sampling;
        extendedSampling.maximumSamples = 576;
        extendedSampling.relativePeakTolerance = 1.0e-14;
        const auto extendedCappedCycle =
            sampleThreeLimbMagneticCycle(extendedSampling);
        assert(extendedCappedCycle);
        // The 576-sample estimate may legitimately match the 288-sample
        // peak exactly for this waveform. Convergence is defined by the
        // measured peak change, not by reaching the sample cap unconditionally.
        assert(extendedCappedCycle.value().samplesUsed == 576);
        assert(extendedCappedCycle.value().converged ==
               (extendedCappedCycle.value().relativePeakChange <=
                extendedSampling.relativePeakTolerance));

        auto invalidSampling = sampling;
        invalidSampling.maximumSamples = 200;
        assert(!sampleThreeLimbMagneticCycle(invalidSampling));

        auto invalidLimbArea = asymmetric;
        invalidLimbArea.geometry.limbEffectiveAreas[1] = units::Area{-1.0};
        const auto invalidLimbAreaResult =
            analyzeThreeLimbMagneticCircuit(invalidLimbArea);
        assert(!invalidLimbAreaResult);
        assert(invalidLimbAreaResult.error().code == ErrorCode::InvalidInput);

        auto invalidTurns = request;
        invalidTurns.turns[1] = 0;
        assert(!analyzeThreeLimbMagneticCircuit(invalidTurns));

        auto invalidYoke = request;
        invalidYoke.geometry.yokeEffectiveArea = units::Area{-1.0};
        assert(!analyzeThreeLimbMagneticCircuit(invalidYoke));

        auto outsideCurve = request;
        outsideCurve.phaseCurrents = {
            units::Current{1000.0}, units::Current{-500.0}, units::Current{-500.0}
        };
        const auto outside = analyzeThreeLimbMagneticCircuit(outsideCurve);
        assert(outside);
        assert(!outside.value().withinBhCurveRange);
        for (const auto density : outside.value().limbFluxDensity)
            assert(std::isfinite(density.si()));
        for (const auto density : outside.value().yokeFluxDensity)
            assert(std::isfinite(density.si()));
    }


    {
        // Micrometals Mix-2 manufacturer equation, source units: gauss and mW/cm3.
        CoreLossEquation equation;
        equation.denominatorTerms = {
            {4.00e9, 3.0},
            {3.00e8, 2.3},
            {2.70e6, 1.65}
        };
        equation.eddyCoefficient = 9.60e-16;
        equation.minimumFrequency_Hz = 60.0;
        equation.maximumFrequency_Hz = 500000.0;
        equation.minimumFluxDensity_T = 0.001; // 10 G
        equation.maximumFluxDensity_T = 1.0;   // 10,000 G
        equation.sourceFluxDensityUnitsPerTesla = 10000.0;
        equation.outputWattsPerM3PerSourceUnit = 1000.0; // mW/cm3 -> W/m3
        equation.sourceReference =
            "https://s3.amazonaws.com/micrometals-production/filer_public/20/4e/204efa63-7335-47a8-96f8-09fa750a0990/mix-02-datasheet.pdf";
        equation.datasetRevision = "2023-12-15";

        const auto nominalPoint = evaluateCoreLossEquation(
            equation, units::Frequency{100000.0}, units::FluxDensity{0.014});
        assert(nominalPoint);
        // Datasheet nominal point: 18 mW/cm3 at 100 kHz and 140 gauss.
        assert(nominalPoint.value().si() > 17600.0);
        assert(nominalPoint.value().si() < 17800.0);

        const auto outsideFrequency = evaluateCoreLossEquation(
            equation, units::Frequency{600000.0}, units::FluxDensity{0.014});
        assert(!outsideFrequency);
        assert(outsideFrequency.error().code == ErrorCode::ModelOutOfDomain);
        const auto outsideFlux = evaluateCoreLossEquation(
            equation, units::Frequency{100000.0}, units::FluxDensity{1.01});
        assert(!outsideFlux);
        assert(outsideFlux.error().code == ErrorCode::ModelOutOfDomain);
        const auto invalidOperatingPoint = evaluateCoreLossEquation(
            equation, units::Frequency{0.0}, units::FluxDensity{0.014});
        assert(!invalidOperatingPoint);

        auto missingSource = equation;
        missingSource.sourceReference.clear();
        const auto missingProvenance = evaluateCoreLossEquation(
            missingSource, units::Frequency{100000.0}, units::FluxDensity{0.014});
        assert(!missingProvenance);
        assert(missingProvenance.error().code == ErrorCode::MissingRequiredData);

        auto invalidTerm = equation;
        invalidTerm.denominatorTerms.front().coefficient = -1.0;
        assert(!evaluateCoreLossEquation(
            invalidTerm, units::Frequency{100000.0}, units::FluxDensity{0.014}));
    }


    {
        // Micrometals Mix-2 initial magnetization equation: Oe -> A/m, gauss -> tesla.
        InitialMagnetizationEquation equation;
        equation.initialPermeability = 10.0;
        equation.coefficientA = 1.50e-3;
        equation.exponentB = 1.96;
        equation.coefficientC = 1.97e4;
        equation.exponentD = 9.18e-4;
        equation.coefficientE = 1.48e3;
        equation.minimumField_Oe = 0.0;
        equation.maximumField_Oe = 4000.0;
        equation.sourceReference =
            "https://datasheets.micrometals.com/D101-1002-DataSheet.pdf";
        equation.datasetRevision = "2023-12-15";

        const auto atZero = evaluateInitialMagnetization(
            equation, units::MagneticFieldStrength{0.0});
        assert(atZero && std::abs(atZero.value().si()) < 1.0e-15);
        const auto at100Oe = evaluateInitialMagnetization(
            equation, units::MagneticFieldStrength{100.0 * 1000.0 /
                (4.0 * 3.14159265358979323846)});
        assert(at100Oe);
        assert(at100Oe.value().si() > 0.103 && at100Oe.value().si() < 0.105);

        const auto curve = sampleInitialMagnetizationCurve(equation, 129);
        assert(curve);
        assert(curve.value().size() == 129);
        assert(std::abs(curve.value().front().fluxDensity.si()) < 1.0e-15);
        assert(curve.value().back().fluxDensity.si() > 1.29);
        assert(curve.value().back().fluxDensity.si() < 1.30);
        for (std::size_t i = 1; i < curve.value().size(); ++i) {
            assert(curve.value()[i].fieldStrength.si() >
                   curve.value()[i - 1].fieldStrength.si());
            assert(curve.value()[i].fluxDensity.si() >
                   curve.value()[i - 1].fluxDensity.si());
        }

        // Feed the equation-derived curve into the existing nonlinear solver.
        auto equationMaterial = validProduct().coreMaterial;
        equationMaterial.bhCurve = curve.value();
        MagneticCircuitRequest magneticRequest;
        magneticRequest.core = validProduct().core;
        magneticRequest.material = equationMaterial;
        magneticRequest.turns = 100;
        magneticRequest.current = units::Current{1.0};
        const auto solvedFromEquation = analyzeMagneticCircuit(magneticRequest);
        assert(solvedFromEquation);
        assert(solvedFromEquation.value().withinBhCurveRange);
        assert(solvedFromEquation.value().fluxDensity.si() > 0.0);

        const auto outside = evaluateInitialMagnetization(
            equation, units::MagneticFieldStrength{4000.1 * 1000.0 /
                (4.0 * 3.14159265358979323846)});
        assert(!outside);
        assert(outside.error().code == ErrorCode::ModelOutOfDomain);
        assert(!sampleInitialMagnetizationCurve(equation, 1));
        auto noSource = equation;
        noSource.sourceReference.clear();
        assert(!evaluateInitialMagnetization(
            noSource, units::MagneticFieldStrength{100.0}));
    }


    {
        CoreMaterial measured;
        measured.id = "CORE-LOSS-GRID";
        measured.sourceReference = "manufacturer-datasheet";
        measured.datasetRevision = "R2";
        measured.lossDataReference = "manufacturer-loss-table";
        for (const double f : {50.0, 100.0}) {
            for (const double b : {0.5, 1.0}) {
                for (const double temp : {293.15, 353.15}) {
                    const double loss = 2.0 + 0.01 * f + 10.0 * b +
                                        0.02 * (temp - 293.15);
                    measured.lossData.push_back({
                        units::Frequency{f}, units::FluxDensity{b},
                        units::TemperatureKelvin{temp}, units::CoreLossDensity{loss}});
                }
            }
        }
        const auto interpolated = evaluateCoreLossData(
            measured, units::Frequency{75.0}, units::FluxDensity{0.75},
            units::TemperatureKelvin{323.15});
        assert(interpolated);
        const double expected = 2.0 + 0.01 * 75.0 + 10.0 * 0.75 + 0.02 * 30.0;
        assert(std::abs(interpolated.value().si() - expected) < 1.0e-12);

        const auto outside = evaluateCoreLossData(
            measured, units::Frequency{101.0}, units::FluxDensity{0.75},
            units::TemperatureKelvin{323.15});
        assert(!outside && outside.error().code == ErrorCode::ModelOutOfDomain);

        auto incomplete = measured;
        incomplete.lossData.pop_back();
        const auto missingCorner = evaluateCoreLossData(
            incomplete, units::Frequency{75.0}, units::FluxDensity{0.75},
            units::TemperatureKelvin{323.15});
        assert(!missingCorner &&
               missingCorner.error().code == ErrorCode::MissingMaterialProperty);

        auto missingSource = measured;
        missingSource.lossDataReference.clear();
        const auto noProvenance = evaluateCoreLossData(
            missingSource, units::Frequency{75.0}, units::FluxDensity{0.75},
            units::TemperatureKelvin{323.15});
        assert(!noProvenance &&
               noProvenance.error().code == ErrorCode::MissingRequiredData);
    }

    CoreMaterialCatalog catalog;
    auto material = validProduct().coreMaterial;
    material.lossDataReference = "measured loss fixture";
    material.lossData = {{units::Frequency{50.0}, units::FluxDensity{1.0}, units::TemperatureKelvin{293.15}, units::CoreLossDensity{1.2}}};
    assert(catalog.add(material));
    assert(catalog.size() == 1);
    assert(catalog.find("MAT-TEST") != nullptr);
    assert(!catalog.add(material));
    auto invalidLoss = material;
    invalidLoss.id = "MAT-BAD";
    invalidLoss.lossData.front().specificLoss = units::CoreLossDensity{-1.0};
    assert(!catalog.add(invalidLoss));
    auto missingProvenance = material;
    missingProvenance.id = "MAT-NOPROV";
    missingProvenance.sourceReference.clear();
    assert(!validate(missingProvenance));

    ConductorMaterialCatalog conductorCatalog;
    ConductorMaterial copper;
    copper.id = "COND-CU-TEST";
    copper.grade = "Test copper";
    copper.sourceReference = "test fixture";
    copper.datasetRevision = "1";
    copper.resistivityAt20C = units::Resistivity{1.72e-8};
    copper.temperatureCoefficient = units::TemperatureCoefficient{0.0039};
    copper.density = units::Density{8960.0};
    assert(conductorCatalog.add(copper));
    auto negativeTemperatureCoefficient = copper;
    negativeTemperatureCoefficient.id = "COND-NTC-TEST";
    negativeTemperatureCoefficient.temperatureCoefficient = units::TemperatureCoefficient{-0.002};
    assert(validate(negativeTemperatureCoefficient));
    assert(conductorCatalog.size() == 1);
    assert(conductorCatalog.find("COND-CU-TEST") != nullptr);
    assert(!conductorCatalog.add(copper));
    auto invalidConductor = copper;
    invalidConductor.id = "COND-BAD";
    invalidConductor.resistivityAt20C = units::Resistivity{-1.0};
    assert(!validate(invalidConductor));

    InsulationMaterialCatalog insulationCatalog;
    InsulationMaterial insulation;
    insulation.id = "INS-TEST";
    insulation.grade = "Test film";
    insulation.sourceReference = "test fixture";
    insulation.datasetRevision = "1";
    insulation.thermalClass = InsulationThermalClass::F;
    insulation.maximumTemperature = units::TemperatureKelvin{428.15};
    insulation.dielectricStrength = units::ElectricFieldStrength{1.0e7};
    insulation.relativePermittivity = 3.2;
    assert(insulationCatalog.add(insulation));
    auto unspecifiedOptionalProperties = insulation;
    unspecifiedOptionalProperties.id = "INS-OPTIONAL-ABSENT";
    unspecifiedOptionalProperties.relativePermittivity.reset();
    unspecifiedOptionalProperties.density.reset();
    assert(validate(unspecifiedOptionalProperties));
    auto invalidPermittivity = insulation;
    invalidPermittivity.id = "INS-BAD-PERMITTIVITY";
    invalidPermittivity.relativePermittivity = 0.0;
    assert(!validate(invalidPermittivity));
    assert(insulationCatalog.size() == 1);
    assert(insulationCatalog.find("INS-TEST") != nullptr);
    assert(!insulationCatalog.add(insulation));
    auto missingInsulationData = insulation;
    missingInsulationData.id = "INS-MISSING";
    missingInsulationData.dielectricStrength = units::ElectricFieldStrength{};
    assert(!validate(missingInsulationData));

    auto missingBh = validProduct();
    missingBh.coreMaterial.bhCurve.clear();
    const auto missingBhResult = validate(missingBh);
    assert(!missingBhResult);
    assert(missingBhResult.error().code == ErrorCode::MissingMaterialProperty);

    auto insufficientWindow = validProduct();
    insufficientWindow.windings.front().turns = 101;
    insufficientWindow.windings.front().turnsPerLayer = 25;
    insufficientWindow.windings.front().layers = 4;
    assert(!validate(insufficientWindow));

    auto invalidStack = validProduct();
    invalidStack.core.stackingFactor = 1.1;
    assert(!validate(invalidStack));

    auto missingBobbin = validProduct();
    missingBobbin.windings.front().bobbin.material.clear();
    assert(!validate(missingBobbin));

    return 0;
}

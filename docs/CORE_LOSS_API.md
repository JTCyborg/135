# Manufacturer core-loss equations

## Purpose

`CoreLossEquation` represents a manufacturer-published volumetric core-loss equation whose source may use non-SI axes and output units. `evaluateCoreLossEquation()` performs unit conversions explicitly and evaluates only inside the declared frequency and peak-flux-density domain.

## Current imported model

Micrometals Mix-2 (datasheet revision 2023-12-15) publishes:

```text
Pv [mW/cm3] = f / (a/B^3 + b/B^2.3 + c/B^1.65) + d*B^2*f^2
B in gauss; f in Hz
a = 4.00e9
b = 3.00e8
c = 2.70e6
d = 9.60e-16
```

The imported chart domain is 60 Hz to 500 kHz and 10 to 10,000 gauss. Conversion is 10,000 gauss/T and 1,000 W/m3 per mW/cm3. At 140 gauss and 100 kHz, the equation evaluates to approximately 17.70 mW/cm3; the datasheet nominal reference is 18 mW/cm3 (maximum 20 mW/cm3).

The equation returns `VolumetricCoreLossDensity` in W/m3. This is intentionally distinct from `CoreLossDensity` in W/kg used by the existing mass-specific `CoreLossPoint` dataset. Conversion between the two requires a validated material density and must not be implicit.

## Validity and limitations

- Missing provenance, invalid coefficients, non-positive operating points and out-of-domain requests are rejected.
- The evaluator never extrapolates beyond the declared datasheet chart domain.
- The Mix-2 source does not publish a temperature axis for this equation; temperature dependence is therefore not inferred.
- This equation is a material-level loss model. Core geometry, fringing, waveform harmonics, DC bias, manufacturing variation and thermal feedback remain separate modelling concerns.
- The imported DC-bias permeability and initial-magnetization equations are retained in the material catalog as manufacturer data; they are not yet coupled into the nonlinear magnetic-circuit solver.
- No design-release claim is made from this import alone.


## Initial magnetization and nonlinear solver input

The Mix-2 datasheet also publishes an initial-magnetization equation:

```text
Bpk [gauss] = mu_i /
  (1/(H + a*H^b) + 1/(c*H^d) + 1/e)

H in Oe; mu_i=10; a=1.50e-3; b=1.96;
c=1.97e4; d=9.18e-4; e=1.48e3
```

`evaluateInitialMagnetization()` accepts SI magnetic field strength and returns tesla. It rejects field strengths outside 0–4000 Oe. `sampleInitialMagnetizationCurve()` creates an explicitly model-generated positive B-H branch in SI units. That vector can be assigned to `CoreMaterial::bhCurve` and passed to the existing `analyzeMagneticCircuit()` or `analyzeThreeLimbMagneticCircuit()` APIs. These solvers therefore remain the single nonlinear solving path; no parallel magnetic-circuit solver is introduced.

The generated curve is a fit/equation sample, not a set of measured points. Its 0–4000 Oe range is the declared equation domain, not a promise of accuracy outside the datasheet's characterization conditions. The caller must preserve manufacturer provenance and keep the operating point inside the curve. The helper does not silently set material density or other catalog properties.

## Source

Micrometals, Mix-02 Powder Core Solutions datasheet, revision 2023-12-15:
https://s3.amazonaws.com/micrometals-production/filer_public/20/4e/204efa63-7335-47a8-96f8-09fa750a0990/mix-02-datasheet.pdf
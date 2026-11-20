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

## Measured core-loss data and total core power

CoreMaterial::lossData stores mass-specific loss in W/kg on a frequency, peak-flux-density and temperature grid. evaluateCoreLossData() performs trilinear interpolation in those three coordinates. It requires all corners needed for the requested interpolation cell, rejects duplicate coordinates and incomplete cells, and never extrapolates beyond the measured axes. Interpolation is linear in the supplied physical coordinates; it is not a logarithmic fit.

calculateCoreLossPower() converts the interpolated W/kg value to total core loss in watts:

```text
P_core [W] = p_specific [W/kg] * m_core [kg]
```

The core mass must be supplied explicitly with a non-empty source reference (for example, a weighing record or a controlled manufacturing drawing/BOM). It is deliberately not inferred from Ae, le, magnetic path length or a partially specified CAD model. The result preserves both the loss-data reference and the core-mass source reference for downstream thermal accounting.

This API calculates the material loss at one supplied operating point. It does not yet derive a cycle-equivalent loss from the seven branch peak flux densities, sum spatially varying limb/yoke losses, or automatically populate a reactor thermal network. Those require local flux waveforms/material data and an explicit mapping of core regions and masses; a single peak-B value must not be treated as a complete loss calculation for a nonsinusoidal waveform.


## Baosteel AC reference datasets (not yet solver-ready)

Two immutable `core-loss-reference` records are seeded separately from `CoreMaterial::lossData`:

| Baosteel grade | Thickness | Density | P10/50 | P15/50 | P10/60 | P15/60 | Product P15/50 maximum |
|---|---:|---:|---:|---:|---:|---:|---:|
| B35A300 | 0.35 mm | 7650 kg/m³ | 1.14 W/kg | 2.53 W/kg | 1.42 W/kg | 3.13 W/kg | 3.00 W/kg |
| B50A800 | 0.50 mm | 7800 kg/m³ | 2.67 W/kg | 5.70 W/kg | 3.34 W/kg | 7.11 W/kg | 8.00 W/kg |

The four AC reference points per grade are transcribed from Baosteel's official "Typical electromagnetic performance" Series A table. The separate P15/50 maximum is the manufacturer's product-specification limit, not the typical table value. The official page identifies GB/T 3655-2008 as the magnetic test reference and states that half the test samples are horizontal and half vertical. Q/BQB 480-2023 defines specific total loss using a sinusoidal magnetic-polarization waveform.

The test temperature is not stated. For this reason these values are intentionally loaded into `CoreLossReferenceDataset`, not into `CoreMaterial::lossData`. They are not eligible for trilinear interpolation or the revision-bound reactor thermal network, whose solver requires a frequency/flux/temperature grid. The dataset loader preserves temperature as absent rather than assigning room temperature. No interpolation, extrapolation, temperature correction, or conversion to a different manufacturer's nominally similar grade is performed.

The current Baosteel HTML table is the numeric source of record. Its values differ slightly from the typical values printed in Q/BQB 480-2023 Appendix D; the import deliberately records the current official HTML values and does not blend the two tables.

Baosteel also publishes a B35A300 0.35 mm PDF with plotted 50 Hz and 60 Hz loss-versus-induction curves. This is a graphical reference, not a tabulated dataset. It is not digitized into solver inputs: any point extraction would be approximate and would need to carry a separate digitization uncertainty/status. The PDF does not provide the missing temperature axis. No corresponding verified temperature grid for the exact B35A300/B50A800 grades was found in the manufacturer's published material reviewed for this import.

The immutable dataset loader now rejects duplicate frequency/flux coordinates, non-positive explicitly supplied measurement temperatures, invalid specification limits, missing source/dataset revision identifiers, and incomplete quality/test provenance. This prevents ambiguous anchors from entering later calculations while preserving the exact source values.

`findExactCoreLossReferencePoint()` exposes a safe read-only path for inspecting one of these anchors: both frequency and peak flux density must match an imported coordinate exactly. A nearby point returns no result. This helper is not a loss model and does not authorize use in the thermal solver; the caller must still honor the dataset's quality status, excitation waveform and unknown measurement temperature.

Sources:
- Baosteel official product performance table: https://esales.baosteel.com/baosteel_products/eleSteel/en/products/products_04_2.jsp
- Baosteel Q/BQB 480-2023 (test definitions and sampling): https://pdt.static.globalsources.com/IMAGES/PDT/SPEC/829/K1229503829.pdf

## Three-limb region-resolved loss accounting

`ThreeLimbCoreLossRequest` and `calculateThreeLimbCoreLossPower()` account for seven independently specified regions in the magnetic solver's fixed order: limb A, limb B, limb C, top yoke AB, top yoke BC, bottom yoke AB, bottom yoke BC. Each region receives its own peak flux density, temperature, physical mass and mass-source reference. The function evaluates each region against the same exact material loss dataset and returns seven auditable power results plus their sum. If any region is missing mass provenance or lies outside the measured loss-data domain, the aggregate calculation fails rather than silently omitting that region.

This is region-resolved accounting, not yet a waveform-loss solver. The current measured-data schema represents loss against frequency, peak flux density and temperature; it does not encode arbitrary flux waveforms, minor loops, DC bias or rotational flux. Therefore the caller must only use this model where the material data's excitation conditions are applicable. A sampled nonsinusoidal magnetic cycle does not by itself make sinusoidal material loss data valid for that waveform.

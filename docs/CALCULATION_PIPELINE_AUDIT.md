# Calculation Pipeline Integration Audit

Audit baseline: `main` at `550572743f9730874219ea52b8c2500596692671`.

This document records the current desktop integration after the Debug CTest suite passed 38/38. It distinguishes calculations that are already executed by the desktop UI from the separate dependency-graph execution API. Passing unit tests does not establish that the full desktop workflow is integrated or physically verified.

## 1. Existing desktop calculation path

`app/desktop/src/main_window.cpp`, `MainWindow::calculateFilter()`, already performs a substantial direct calculation workflow:

- Single-tuned shunt filter synthesis using the filter synthesis model.
- AC network solve and frequency sweep; the phase-current plot is populated from sweep branch currents.
- Bus impedance scan; the bus-impedance plot is populated only when the impedance result exists.
- Harmonic analysis when a usable harmonic spectrum is supplied.
- Magnetic analysis when the user supplies compatible core geometry, turns and measured B-H data. The three-limb path includes phase DC-bias inputs and adaptive cycle sampling for a multi-harmonic operating point.
- A three-limb magnetic frequency sweep can populate the core-flux plot. The plot remains empty when a valid magnetic model/sweep is unavailable.
- Separate capacitor and damping-resistor thermal assessments can run when harmonic results, supported component revisions and required thermal assignments are available.
- Immutable calculation persistence includes input/result JSON, engine-version metadata, a canonical hash and exact material revision links.

These are real model calls in the existing desktop workflow; they must not be duplicated by a new orchestration layer. Their current scope and limitations must remain visible in the UI and snapshots.

## 2. Separate execution API is not integrated

The application layer already provides:

- `planReactorRecalculation(changedInputIds)` in `reactor_design_graph.cpp`;
- `executeReactorDesignPlan(plan, state, executor)` in `reactor_design_execution.cpp`;
- transactional candidate execution: a failed stage does not replace the previously accepted revision;
- stable dependency IDs for filter excitation, core material/geometry/magnetic properties, winding geometry/resistance/losses, thermal steady state, assembly geometry and design verification.

Repository audit found no call to `executeReactorDesignPlan()` in `app/desktop/src/main_window.cpp`. The desktop currently executes the electrical, magnetic and component thermal calculations directly; the dependency graph is therefore descriptive and its transactional revision state is not the source of truth for the UI.

The executor accepts opaque serialized stage artifacts. That is not sufficient by itself to claim a stage is physically valid. The desktop needs typed stage adapters that consume the actual model inputs/results and return explicit failures when required data or model support is missing.

## 3. Current result boundaries

| Area | Current implementation | Correct status boundary |
|---|---|---|
| Filter synthesis / AC network | Directly executed in desktop | Calculated for supported shunt-filter topology |
| Frequency/current plot | Built from AC frequency-sweep results | Diagnostic sweep; not a multi-harmonic operating waveform |
| Bus impedance plot | Built from impedance scan when available | Unavailable / NOT_VERIFIED when source/network inputs do not support the scan |
| Harmonic response | Conditional on supplied harmonic spectrum | NOT_VERIFIED without a valid spectrum |
| Core magnetic model | Conditional on explicit geometry, turns and B-H data | NOT_VERIFIED when inputs are absent, sampling fails or results extrapolate beyond measured B-H data |
| Three-limb flux plot | Built from three-limb magnetic sweep | Must remain empty/labelled unavailable without a valid magnetic sweep |
| Ideal area-product sizing | Optional calculation from five user-entered criteria | IDEAL LOWER BOUND / IDEALIZED; not a selected core or winding design |
| Capacitor/resistor thermal checks | Separate component assessments | Do not imply reactor-core or winding thermal verification |
| Coupled reactor thermal model | Thermal modules exist, but no complete desktop stage integration is established by this audit | NOT_CONNECTED / NOT_VERIFIED until actual inputs and result adapters are connected |
| 3D CAD | Preliminary geometry path exists from explicit Construction dimensions | PRELIMINARY / NOT_VERIFIED; not production geometry or engineering certification |

## 4. Integration work sequence

### A. Typed stage context and adapters
1. Define an immutable calculation context carrying the accepted filter snapshot ID/branch, typed `ReactorDesignInputs`, exact material revision IDs, and references to the already-computed filter, harmonic, magnetic and thermal artifacts.
2. Implement stage adapters around existing model results. Do not recalculate the same solver independently in a second code path.
3. Each adapter must distinguish: valid result, missing required input, unsupported model/topology, numerical failure and extrapolated/unverified result.
4. Do not convert an unavailable/placeholder result into a `Valid` execution artifact.

### B. Transactional dependency execution
1. Derive changed input IDs from the accepted input fingerprint, not from a hard-coded full rebuild.
2. Execute affected stages in graph order using the typed adapters.
3. Preserve the last accepted immutable revision on any stage failure.
4. Invalidate downstream stage artifacts whenever an upstream input or material revision changes.
5. Commit a new revision only when the selected commit policy is satisfied; keep partial diagnostic results separate from an accepted engineering design revision.

### C. Result UI and plots
1. Add per-stage status, missing-input reason and artifact revision/fingerprint to Calculation & design.
2. Keep current electrical diagnostic plots, but label their exact source and status.
3. Keep magnetic plots tied only to magnetic solver outputs; never fill them with idealized or synthetic data.
4. Add winding/core loss and thermal plots only after the corresponding physical models and adapters produce real data.
5. Include units, RMS/peak distinction, source revision and model version in result metadata.

### D. CAD consistency
1. Bind CAD preview to the same accepted Construction input fingerprint and material/geometry lineage as the calculation revision.
2. Invalidate CAD on any relevant geometry/material change.
3. Preserve `PRELIMINARY / NOT_VERIFIED` for simplified envelopes.
4. Continue to block Step-Lap and cooling-duct CAD where the OCCT representation is not implemented.

## 5. Immediate implementation target

The next code increment should establish a typed desktop-to-application stage boundary and integrate the existing filter-excitation result first. It must not mark magnetic, winding, loss, thermal or design-verification stages valid merely because the electrical calculation completed. Subsequent increments should adapt the already-existing magnetic and component thermal results, then connect the reactor winding/core thermal network where its required physical inputs are present.

## 6. Verification requirements

- Add application tests for stage adapters with complete, missing, unsupported and failed inputs.
- Add tests proving that changing filter, geometry or material revisions invalidates the correct downstream artifacts.
- Add a desktop regression test for the displayed stage states and unavailable-result reasons.
- Run Debug and Release builds/tests on Windows/MSVC.
- Verify the UI with TEST-01 and with deliberately incomplete Construction data.
- Do not report desktop build, UI, or STEP export success unless actually run in the Windows/MSVC environment.

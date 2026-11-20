# Reactor thermal network assembly

`analyzeReactorThermalNetwork()` combines the existing seven-region core-loss result and the three-phase winding copper-loss result into one steady-state thermal conductance network.

## Node mapping

The generated node order is fixed:

1. Core limb A
2. Core limb B
3. Core limb C
4. Top yoke AB
5. Top yoke BC
6. Bottom yoke AB
7. Bottom yoke BC
8. Winding phase A
9. Winding phase B
10. Winding phase C

Each core node uses its own region loss and preserves both the loss-dataset reference and the physical-mass source reference. Each winding node sums the corresponding phase loss across all harmonic entries; the three phase sums are checked against the supplied three-phase copper-loss total.

## Explicit inputs

The caller must supply:
- core and winding maximum-temperature limits and their source references;
- the winding copper-loss study reference;
- every inter-node thermal conductance and its source reference;
- every node-to-ambient conductance and its source reference;
- core and conductor material design-release statuses.

No conductance, cooling coefficient, temperature limit, or material qualification state is inferred.

## Release gating

A numerical network solution is not by itself a design release. If either material release status is not exactly `VERIFIED`, nodes that would otherwise be `PASS` are downgraded to `NOT_VERIFIED`. A calculated temperature exceedance remains `FAIL`.

## Model limits

This assembly delegates to the existing linear steady-state thermal network. It does not model transient thermal capacity, nonlinear convection or radiation, spatial hot spots inside a region, frequency-dependent thermal properties, skin/proximity winding losses, or waveform-dependent core-loss corrections. The supplied core-loss model must already be valid for the actual material excitation conditions.

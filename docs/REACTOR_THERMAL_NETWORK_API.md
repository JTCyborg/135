# Revision-bound reactor thermal network

`analyzeRevisionBoundReactorThermalNetwork(...)` is the end-to-end entry point for a three-limb reactor thermal assessment.

It accepts exact immutable core and conductor revision IDs, a seven-region core-loss operating request, a revision-bound winding copper-loss request, and an explicit thermal-network configuration.

## Execution sequence

1. Load the exact core-material revision from SQLite.
2. Verify its canonical JSON and SHA-256 content hash.
3. Calculate losses for all seven core regions using that revision's loss dataset and the supplied region masses, flux densities and temperatures.
4. Load the exact conductor-material revision and calculate the selected filter branch's three-phase harmonic copper losses.
5. Build seven core nodes and three winding-phase nodes.
6. Solve the linear coupled thermal conductance network using only explicitly supplied conductance links and ambient links.

The result returns both material revision snapshots, including their immutable revision IDs and content hashes, and the thermal result. Each node's loss provenance is bound to the corresponding revisionId#contentHash.

## Inputs that remain explicit

The caller must supply:
- frequency, regional peak flux density, temperature, mass and mass provenance for all seven core regions;
- filter branch, winding turns, parallel conductors, mean turn length, bare conductor area and winding temperature;
- ambient temperature;
- seven core and three winding temperature limits with source references;
- all node-to-node and node-to-ambient thermal conductances with source references.

The API does not infer core mass, winding geometry, thermal conductance, cooling performance, temperature limits or material qualification.

## Status semantics

A solved temperature is not automatically a verified design result. Starter material records are marked NOT_VERIFIED; consequently, their nodes remain NOT_VERIFIED even when the preliminary numerical temperature is below the supplied limit. If material design-release status is not VERIFIED, neither a preliminary PASS nor a preliminary FAIL is promoted to a qualified assessment.

The underlying network is a linear steady-state conductance model. It does not include transient thermal storage, nonlinear convection/radiation, local hot spots, anisotropic thermal properties or frequency-dependent AC winding losses.

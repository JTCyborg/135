# Project135

Windows desktop engineering platform for calculation, design, optimization and CAD documentation of wound magnetic products and passive power-electronics components.

## Project documentation

- [Полный сценарий проекта](docs/PROJECT_SCENARIO.md)
- [Core API и контракты расчётных модулей](docs/CORE_API.md)
- [P135.002 — Circuit Engine / MNA](docs/P135.002_CIRCUIT_ENGINE.md)

## Current development status

### P135.001 — Core foundation
- C++20 core target independent from Qt and OCCT.
- Strongly typed SI quantity wrappers.
- Result/error and diagnostic primitives.
- Cooperative cancellation token.
- Visual Studio 2026 x64 Debug/Release CMake presets.
- Initial smoke tests.

### P135.002 — Circuit Engine (initial linear MNA)
- DC operating point and single-frequency AC steady-state.
- R, L, C and ideal voltage/current sources.
- Complex RMS phasors and branch currents.
- Power balance and linear-system residual diagnostics.
- Initial analytical regression tests.

**Important:** the repository implementation has not yet been built and tested in the target Windows/MSVC environment. See the Circuit Engine document for supported scope and limitations.

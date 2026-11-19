# Project135

Windows desktop engineering platform for calculation, design, optimization and CAD documentation of wound magnetic products and passive power-electronics components.

## Project documentation

- [Полный сценарий проекта](docs/PROJECT_SCENARIO.md)
- [Core API и контракты расчётных модулей](docs/CORE_API.md)
- [P135.001 — Desktop UI shell](docs/P135.001_DESKTOP_UI.md)
- [P135.002 — Circuit Engine / MNA](docs/P135.002_CIRCUIT_ENGINE.md)

## Current development status

### P135.001 — Core foundation and desktop shell
- C++20 core target independent from Qt and OCCT.
- Strongly typed SI quantity wrappers, Result/error diagnostics and cooperative cancellation.
- Initial Qt 6 Widgets executable target: `Project135.exe`.
- Initial pages: Project, Filter definition, Calculation and 3D CAD placeholder.
- Draft shunt-filter input controls; inputs are not yet connected to product synthesis.
- Visual Studio 2026 x64 Debug/Release presets, including desktop-enabled variants.

### P135.002 — Circuit Engine (initial linear MNA)
- DC operating point and single-frequency AC steady-state.
- R, L, C and ideal voltage/current sources.
- Complex RMS phasors, branch currents, power balance and residual diagnostics.
- Initial analytical regression tests and frequency sweep API.

**Verification status:** the Qt desktop target and engineering tests have not yet been built in the target Windows/MSVC + Qt environment. The desktop shell is an initial UI prototype, not a completed filter-design workflow. See [desktop UI scope](docs/P135.001_DESKTOP_UI.md) and [Circuit Engine scope](docs/P135.002_CIRCUIT_ENGINE.md).

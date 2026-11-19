# Project135

Windows desktop engineering platform for calculation, design, optimization and CAD documentation of wound magnetic products and passive power-electronics components.

## Project documentation

- [Полный сценарий проекта](docs/PROJECT_SCENARIO.md)
- [Core API и контракты расчётных модулей](docs/CORE_API.md)
- [P135.001 — Desktop UI shell](docs/P135.001_DESKTOP_UI.md)
- [P135.002 — Circuit Engine / MNA](docs/P135.002_CIRCUIT_ENGINE.md)
- [P135.003 — Filter synthesis](docs/P135.003_FILTER_SYNTHESIS.md)

## Current development status

### P135.001 — Core foundation and desktop shell
- C++20 core target independent from Qt and OCCT.
- Strongly typed SI quantity wrappers, Result/error diagnostics and cooperative cancellation.
- Initial Qt 6 Widgets executable target: `Project135.exe`.
- Initial pages: Project, Filter definition, Calculation and 3D CAD placeholder.
- Connected three-phase shunt-filter form and first analytical LC-branch synthesis workflow.
- Visual Studio 2026 x64 Debug/Release presets, including desktop-enabled variants.

### P135.002 — Circuit Engine (initial linear MNA)
- DC operating point and single-frequency AC steady-state.
- R, L, C and ideal voltage/current sources.
- Complex RMS phasors, branch currents, power balance and residual diagnostics.
- Initial analytical regression tests and frequency sweep API.

### P135.003 — Shunt Passive Filter synthesis (first slice)
- New Qt-independent `Project135Filter` library.
- Three-phase Wye/Delta network definition and parallel single-tuned series-RLC branches.
- Analytical C/L/damping-R sizing from fundamental-frequency reactive power, tuning order and quality factor.
- Input validation with structured diagnostics and filter regression tests.
- Qt UI calls the synthesis API and displays C/L/R results; circuit-network verification and harmonic/resonance analysis remain pending.

**Verification status:** the user confirmed the existing desktop Debug and Release builds launch successfully before this milestone. The new filter module and tests have been committed but still require a fresh local Debug/Release build and CTest run. See [filter synthesis scope](docs/P135.003_FILTER_SYNTHESIS.md).

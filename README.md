# Project135

Windows desktop engineering platform for calculation, design, optimization and CAD documentation of wound magnetic products and passive power-electronics components.

## Project documentation

- [Полный сценарий проекта](docs/PROJECT_SCENARIO.md)
- [Core API и контракты расчётных модулей](docs/CORE_API.md)

## Current development stage

**P135.001 — Core foundation**

Implemented:
- C++20 core interface target, independent from Qt and OCCT.
- Strongly typed SI quantity wrappers.
- Result/error and diagnostic primitives.
- Cooperative cancellation token.
- Visual Studio 2026 x64 Debug/Release CMake presets.
- Initial smoke tests for core primitives.

The desktop application is intentionally disabled in the initial configure presets until the core foundation and test target are established.

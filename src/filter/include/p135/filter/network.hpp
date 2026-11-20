#pragma once

#include "p135/circuit/solver.hpp"
#include "p135/core/result.hpp"
#include "p135/filter/synthesis.hpp"

namespace p135::filter {

/// Builds a balanced three-phase Thevenin source and the synthesized shunt RLC bank.
/// Source resistance and inductance are per phase; zero values preserve an ideal source.
/// The returned circuit uses RMS phasors and SI component values.
[[nodiscard]] Result<circuit::Circuit> buildCircuit(
    const FilterDesign& design,
    units::Resistance sourceResistance = units::Resistance{0.0},
    units::Inductance sourceInductance = units::Inductance{0.0});

} // namespace p135::filter

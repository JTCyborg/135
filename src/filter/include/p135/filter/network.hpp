#pragma once

#include "p135/circuit/solver.hpp"
#include "p135/core/result.hpp"
#include "p135/filter/synthesis.hpp"

namespace p135::filter {

/// Builds a balanced, ideal three-phase AC source and the synthesized shunt RLC bank.
/// The returned circuit uses RMS phasors and SI component values.
[[nodiscard]] Result<circuit::Circuit> buildCircuit(const FilterDesign& design);

} // namespace p135::filter

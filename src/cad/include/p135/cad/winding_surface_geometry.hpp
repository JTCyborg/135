#pragma once

#include "p135/core/result.hpp"

#include <TopoDS_Shape.hxx>

#include <array>
#include <cstddef>
#include <vector>

namespace p135::cad {

// Topological role within the known hollow rectangular winding envelope.
// These roles do not establish air exposure or convection suitability.
enum class WindingEnvelopeFaceRole {
    OuterLateral,
    InnerCavity,
    AxialEnd,
    Other
};

// Geometric descriptor only. It does not assert that a face is air-exposed,
// externally visible, or suitable for a convection boundary condition.
struct WindingEnvelopeFaceGeometry {
    std::size_t faceIndex{0};
    double areaMm2{0.0};
    std::array<double, 3> centroidMm{};
    std::array<double, 3> unitNormal{};
    std::array<double, 6> boundsMm{};
    bool planar{false};
    WindingEnvelopeFaceRole role{WindingEnvelopeFaceRole::Other};
};

[[nodiscard]] Result<std::vector<WindingEnvelopeFaceGeometry>>
extractWindingEnvelopeFaceGeometry(const TopoDS_Shape& windingEnvelope);

} // namespace p135::cad

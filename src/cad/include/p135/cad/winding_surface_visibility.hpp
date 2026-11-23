#pragma once

#include "p135/core/result.hpp"

#include <TopoDS_Shape.hxx>

#include <cstddef>
#include <vector>

namespace p135::cad {

enum class WindingFaceVisibility {
    FullyVisible,
    PartiallyOccluded,
    FullyOccluded,
    Unsupported
};

struct WindingFaceVisibilityResult {
    std::size_t faceIndex{0};
    WindingFaceVisibility visibility{WindingFaceVisibility::Unsupported};
    std::size_t sampleCount{0};
    std::size_t visibleSampleCount{0};
    double visibleFraction{0.0};
    // Area multiplied by the sampled visible fraction; this is an estimate,
    // not an exact trimmed-face area or a convection-effective area.
    double estimatedVisibleAreaMm2{0.0};
};

struct WindingFaceVisibilityOptions {
    std::size_t samplesPerDirection{9};
    double rayStartOffsetMm{0.01};
    double geometricToleranceMm{1.0e-6};
};

// Occluders must contain only other physical components (core, bobbin,
// neighbouring phases, enclosure, etc.). The source winding envelope is
// deliberately separate and must not be included in occluders.
[[nodiscard]] Result<std::vector<WindingFaceVisibilityResult>>
analyzeWindingEnvelopeFaceVisibility(
    const TopoDS_Shape& windingEnvelope,
    const std::vector<TopoDS_Shape>& occluders,
    const WindingFaceVisibilityOptions& options = {});

} // namespace p135::cad

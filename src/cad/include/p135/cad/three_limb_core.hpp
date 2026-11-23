#pragma once

#include "p135/core/result.hpp"
#include "p135/domain/mechanical_geometry.hpp"

#include <TopoDS_Shape.hxx>

namespace p135::cad {

// Builds one hollow bobbin solid with two explicitly dimensioned end flanges.
// The local datum is centred in X/Z; the axial interval is Y=[0, axialLength].
[[nodiscard]] Result<TopoDS_Shape> buildBobbinShape(
    const domain::BobbinPhysicalGeometry& geometry);

// Builds a hollow rectangular winding-pack envelope around the bobbin tube.
// This is a spatial envelope only, not a conductor/layer manufacturing model.
[[nodiscard]] Result<TopoDS_Shape> buildWindingPackEnvelopeShape(
    const domain::BobbinPhysicalGeometry& bobbin,
    const domain::WindingPackPhysicalGeometry& winding);

// Builds one independent OCCT solid for each explicitly dimensioned rectangular
// or foil conductor path. Returns a compound of separate turn/conductor solids;
// round conductors remain on the envelope path until a swept circular profile
// implementation is available.
[[nodiscard]] Result<TopoDS_Shape> buildDetailedWindingTurnsShape(
    const domain::BobbinPhysicalGeometry& bobbin,
    const domain::WindingPackPhysicalGeometry& winding);

// Builds a reactor assembly compound containing the seven core components
// and three positioned bobbin solids. Bobbins are centred on their limbs in X/Z
// and centred within the corresponding clear window along Y.
[[nodiscard]] Result<TopoDS_Shape> buildThreeLimbReactorAssemblyShape(
    const domain::ReactorPhysicalGeometry& geometry);

// Builds the core, three bobbins and three positioned winding representations.
// Explicit single-section rectangular/foil data produces independent conductor
// solids; other constructions retain the clearly identified envelope fallback.
// The existing core-and-bobbin assembly API remains unchanged.
[[nodiscard]] Result<TopoDS_Shape> buildThreeLimbReactorAssemblyWithWindingEnvelopesShape(
    const domain::ReactorPhysicalGeometry& geometry);

// Builds a seven-component B-Rep compound (three limbs and four yoke segments)
// from explicit physical dimensions. The returned shape is not a fused single
// solid; assembly identity is retained for the later BOM/assembly stage.
[[nodiscard]] Result<TopoDS_Shape> buildThreeLimbCoreShape(
    const domain::ReactorPhysicalGeometry& geometry);

} // namespace p135::cad

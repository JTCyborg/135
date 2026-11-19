#pragma once

#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace p135::domain {

enum class CoreGeometry { ThreeLimb, ThreeSinglePhase, EI, UI, Toroidal, StripWound, Custom };
enum class CoreMaterialFamily { CRGO, ElectricalSteel, Amorphous, Nanocrystalline, Ferrite, Powder };
enum class ConductorShape { Round, Rectangular, Foil, Litz, Busbar };
enum class WindingTechnology { Layer, Sectional, Disc, Foil, Multilayer, Litz };
enum class WindingDirection { Clockwise, CounterClockwise };
enum class InsulationPosition { Turn, Layer, Section, Main, Reinforced };
enum class CoolingMethod { NaturalAir, ForcedAir, Liquid };

struct CoreGeometry {
    CoreGeometry type{CoreGeometry::Custom};
    units::Area effectiveArea;       // Ae, m^2
    units::Area windowArea;          // Aw, m^2
    units::Length meanMagneticPath;  // le, m
    units::Length airGap;            // total physical gap, m
    double stackingFactor{1.0};
};

struct BHPoint {
    units::MagneticFieldStrength fieldStrength;
    units::FluxDensity fluxDensity;
};

struct CoreMaterial {
    std::string id;
    std::string manufacturer;
    std::string grade;
    CoreMaterialFamily family{CoreMaterialFamily::ElectricalSteel};
    units::Density density;
    std::vector<BHPoint> bhCurve;
    // Loss data must be supplied as measured/manufacturer data before core-loss
    // calculations can be considered verified.
    std::string lossDataReference;
};

struct Conductor {
    std::string id;
    std::string material{"Copper"};
    ConductorShape shape{ConductorShape::Round};
    units::Area bareCrossSection;
    units::Length bareWidth;
    units::Length bareThickness;
    units::Length insulationBuild;
    double resistivityAt20C{1.724e-8}; // ohm*m, copper default is a nominal reference only
    double temperatureCoefficient{0.00393}; // 1/K
};

struct InsulationLayer {
    InsulationPosition position{InsulationPosition::Turn};
    std::string material;
    units::Length thickness;
    std::optional<double> dielectricStrength; // V/m; absent means NOT_VERIFIED
    std::optional<double> maximumTemperature; // K absolute
};

struct Bobbin {
    bool used{true};
    std::string material;
    units::Length innerWidth;
    units::Length innerHeight;
    units::Length wallThickness;
    units::Length flangeThickness;
};

struct Winding {
    std::string name;
    WindingTechnology technology{WindingTechnology::Layer};
    WindingDirection direction{WindingDirection::Clockwise};
    std::uint32_t turns{};
    std::uint32_t turnsPerLayer{};
    std::uint32_t layers{};
    std::uint32_t parallelConductors{1};
    Conductor conductor;
    std::vector<InsulationLayer> insulation;
    Bobbin bobbin;
};

struct CoolingConfiguration {
    CoolingMethod method{CoolingMethod::NaturalAir};
    units::TemperatureKelvin ambientTemperature{293.15};
    units::TemperatureKelvin maximumWindingTemperature{393.15};
};

struct MagneticProduct {
    std::string name;
    CoreGeometry core;
    CoreMaterial coreMaterial;
    std::vector<Winding> windings;
    CoolingConfiguration cooling;
};

/// Structural validation only. It does not certify magnetic, dielectric,
/// thermal, manufacturing or standards compliance.
[[nodiscard]] Result<void> validate(const MagneticProduct& product);

} // namespace p135::domain

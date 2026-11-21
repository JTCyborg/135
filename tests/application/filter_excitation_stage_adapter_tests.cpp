#include "p135/application/filter_excitation_stage_adapter.hpp"
#include "p135/filter/synthesis.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <string>

using namespace p135;

int main()
{
    filter::FilterSpecification specification;
    specification.network.lineToLineVoltage = units::Voltage{400.0};
    specification.network.fundamentalFrequency = units::Frequency{50.0};
    specification.network.connection = filter::Connection::Wye;
    specification.branches.push_back({
        1, "TEST-01", 5.0, units::Power{100000.0}, 50.0
    });

    const auto design = filter::synthesize(specification);
    assert(design);
    const auto artifact = application::makeFilterExcitationStageArtifact(design.value());
    assert(artifact);
    assert(artifact.value().find("schema=project135.filter-excitation.v1") != std::string::npos);
    assert(artifact.value().find("line_to_line_voltage_V=400") != std::string::npos);
    assert(artifact.value().find("fundamental_frequency_Hz=50") != std::string::npos);
    assert(artifact.value().find("branch_count=1") != std::string::npos);
    assert(artifact.value().find("branch.0.inductance_H=") != std::string::npos);

    auto missingBranch = design.value();
    missingBranch.branches.clear();
    assert(!application::makeFilterExcitationStageArtifact(missingBranch));

    auto invalidBranch = design.value();
    invalidBranch.branches.front().inductance = units::Inductance{
        std::numeric_limits<double>::quiet_NaN()
    };
    assert(!application::makeFilterExcitationStageArtifact(invalidBranch));

    auto invalidNetwork = design.value();
    invalidNetwork.network.fundamentalFrequency = units::Frequency{0.0};
    assert(!application::makeFilterExcitationStageArtifact(invalidNetwork));
    return 0;
}

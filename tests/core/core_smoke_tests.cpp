#include "p135/core/cancellation.hpp"
#include "p135/core/result.hpp"
#include "p135/core/units.hpp"

#include <cassert>
#include <type_traits>

int main() {
    using namespace p135;

    static_assert(!std::is_convertible_v<units::Voltage, units::Current>);
    static_assert(!std::is_convertible_v<units::TemperatureKelvin,
                                         units::TemperatureDifference>);

    const auto success = Result<int>::success(42);
    assert(success.hasValue());
    assert(success.value() == 42);

    const auto failure = Result<int>::failure(
        Error{ErrorCode::InvalidInput, "test error", {}});
    assert(!failure.hasValue());
    assert(failure.error().code == ErrorCode::InvalidInput);

    CancellationSource source;
    const auto token = source.token();
    assert(!token.isCancellationRequested());
    source.requestCancellation();
    assert(token.isCancellationRequested());

    return 0;
}

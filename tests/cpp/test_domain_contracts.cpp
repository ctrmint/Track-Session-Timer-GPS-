#include "track_timer/domain/contracts.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>

int main()
{
    using namespace track_timer::domain;

    const GnssFix fix{
        1'000'000'000,
        1'000'100,
        52.0,
        -1.0,
        100.0F,
        30.0F,
        90.0F,
        0.8F,
        0.1F,
        1.0F,
        42,
        0x01,
        18,
        FixType::fix_3d,
        FixRejectReason::none,
        true,
    };

    assert(fix.measurement_time_ns == 1'000'000'000);
    assert(fix.arrival_monotonic_us == 1'000'100);
    assert(fix.accepted_for_timing);
    assert(queue_capacity::gnss_fixes >= 25 * 2);
    assert(queue_capacity::ui_snapshots == 2);
    assert(std::is_trivially_copyable_v<LogRecord>);

    const LapEvent event{1, 2'000'000'000, 60'000'000'000, 0.5, 41, 42, 0};
    assert(event.intersection_fraction == 0.5);
    assert(event.segment_sequence_1 == fix.sequence_number);

    std::cout << "Domain contracts passed\n";
    return 0;
}

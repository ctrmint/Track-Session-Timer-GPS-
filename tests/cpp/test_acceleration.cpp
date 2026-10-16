#include "track_timer/ui/acceleration.hpp"
#include "track_timer/ui/foundation.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

constexpr std::int64_t kMs = 1'000'000;          // one millisecond in nanoseconds
constexpr std::int64_t kEpoch = 40 * kMs;        // one fix at 25 Hz

ui::AccelerationSnapshot sample(const float speed_mps, const std::int64_t measurement_ns,
                                const std::int64_t arrival_us = 0)
{
    ui::AccelerationSnapshot snapshot{};
    snapshot.target_mph = 60;
    snapshot.now_monotonic_us = arrival_us;
    snapshot.gnss.receiver_present = true;
    snapshot.gnss.fix_valid = true;
    snapshot.gnss.fix.fix_type = domain::FixType::fix_3d;
    snapshot.gnss.fix.num_satellites = 14;
    snapshot.gnss.fix.speed_mps = speed_mps;
    snapshot.gnss.fix.speed_accuracy_mps = 0.05F;
    snapshot.gnss.fix.measurement_time_ns = measurement_ns;
    snapshot.gnss.fix.arrival_monotonic_us = arrival_us;
    return snapshot;
}

// Extrapolating back to zero is the half of the measurement with no second opinion: a
// finish can be checked against a stopwatch, a start cannot.
void the_start_is_extrapolated_back_to_zero_speed()
{
    // 2 m/s at t=1000 ms, 6 m/s at t=1040 ms. 4 m/s gained in 40 ms, so zero was 20 ms
    // before the first sample.
    const auto zero = ui::extrapolated_zero_ns(1000 * kMs, 2.0F, 1040 * kMs, 6.0F);
    assert(zero == 980 * kMs);

    // Already moving with little gain extrapolates a long way back - correct arithmetic,
    // and the reason the first sample has to be near rest for the result to mean anything.
    //
    // Allowed a microsecond of slack rather than asserted exactly: 1.1F - 1.0F is not
    // 0.1 in binary floating point, and a test that demanded the exact nanosecond would be
    // asserting the representation rather than the behaviour.
    const auto shallow = ui::extrapolated_zero_ns(1000 * kMs, 1.0F, 1040 * kMs, 1.1F);
    assert(std::llabs(shallow - 600 * kMs) < 1000);

    // No acceleration between the pair: nothing to extrapolate along, so the first sample
    // stands rather than dividing by zero.
    assert(ui::extrapolated_zero_ns(1000 * kMs, 3.0F, 1040 * kMs, 3.0F) == 1000 * kMs);
    assert(ui::extrapolated_zero_ns(1000 * kMs, 3.0F, 1040 * kMs, 2.0F) == 1000 * kMs);
}

void the_finish_is_interpolated_between_the_straddling_fixes()
{
    // 25 m/s at t=5000 ms, 29 m/s at t=5040 ms, target 26 m/s - a quarter of the way.
    const auto crossing =
        ui::interpolated_crossing_ns(5000 * kMs, 25.0F, 5040 * kMs, 29.0F, 26.0F);
    assert(crossing == 5010 * kMs);

    // Landing exactly on the later sample is the boundary case.
    assert(ui::interpolated_crossing_ns(5000 * kMs, 25.0F, 5040 * kMs, 29.0F, 29.0F) ==
           5040 * kMs);
    assert(ui::interpolated_crossing_ns(5000 * kMs, 25.0F, 5040 * kMs, 29.0F, 25.0F) ==
           5000 * kMs);
}

void an_absent_receiver_shows_no_time()
{
    ui::AccelerationRun run;
    run.update({});
    assert(run.state() == ui::AccelerationState::no_receiver);
    const auto view = run.view_model({});
    assert(!view.time_valid);
    assert(std::strcmp(view.time.data(), "--.--") == 0);
    assert(std::strcmp(view.status.data(), "NO RECEIVER") == 0);
}

// A fix can be perfect while the speed it carries is not worth measuring against.
void a_receiver_unsure_of_its_speed_will_not_time_a_run()
{
    ui::AccelerationRun run;
    auto snapshot = sample(0.0F, 0);
    snapshot.gnss.fix.speed_accuracy_mps = 2.0F;
    run.update(snapshot);
    assert(run.state() == ui::AccelerationState::no_speed);
    const auto view = run.view_model(snapshot);
    // Its own words: the fix is fine, and "NO FIX" would send somebody to the wrong
    // problem entirely.
    assert(std::strcmp(view.status.data(), "NO SPEED") == 0);
    assert(!view.time_valid);
}

void standing_still_is_armed_and_reads_zero()
{
    ui::AccelerationRun run;
    const auto snapshot = sample(0.1F, 1000 * kMs);
    run.update(snapshot);
    assert(run.state() == ui::AccelerationState::armed);
    const auto view = run.view_model(snapshot);
    // A literal 0.00, which is true: the car has not moved. This is the one place a zero
    // is the right answer rather than a placeholder.
    assert(view.time_valid);
    assert(std::strcmp(view.time.data(), "0.00") == 0);
    assert(std::strcmp(view.status.data(), "READY") == 0);
    assert(std::strcmp(view.target.data(), "0-60 MPH") == 0);
}

// The whole run, at 25 Hz, accelerating steadily to past 60 mph.
void a_run_times_from_rest_to_the_target_and_stops()
{
    ui::AccelerationRun run;
    std::int64_t t = 1000 * kMs;
    std::int64_t arrival = 1'000'000;
    run.update(sample(0.0F, t, arrival));
    assert(run.state() == ui::AccelerationState::armed);

    // 4 m/s^2, so 60 mph (26.8224 m/s) arrives about 6.7 s after a true standing start.
    constexpr float kAccel = 4.0F;
    float speed = 0.0F;
    for (int step = 1; step <= 250; ++step) {
        t += kEpoch;
        arrival += 40'000;
        speed = kAccel * static_cast<float>(step) * 0.04F;
        run.update(sample(speed, t, arrival));
        if (run.state() == ui::AccelerationState::complete) {
            break;
        }
        // Armed until the car is genuinely moving, then running. At 4 m/s^2 that is three
        // epochs below the rest threshold, which is the threshold doing its job rather
        // than a late start: the extrapolation recovers the true zero from the pair that
        // straddles it.
        assert(run.state() == ui::AccelerationState::armed ||
               run.state() == ui::AccelerationState::running);
    }
    assert(run.state() == ui::AccelerationState::complete);

    // 26.8224 / 4 = 6.7056 s. Interpolated at both ends, so this lands far closer than the
    // 40 ms between fixes - which is the entire reason both ends are interpolated.
    const auto elapsed = run.elapsed_seconds();
    assert(std::fabs(elapsed - 6.7056F) < 0.01F);

    const auto view = run.view_model(sample(speed, t, arrival));
    assert(view.state == ui::AccelerationState::complete);
    assert(std::strcmp(view.status.data(), "DONE") == 0);
    // Green is the signal that it finished, so it must belong to this state alone.
    assert(view.time_rgb == ui::color::positive_bright);
    assert(std::strcmp(view.time.data(), "6.71") == 0);
}

void the_time_holds_once_the_target_is_passed()
{
    ui::AccelerationRun run;
    std::int64_t t = 0;
    std::int64_t arrival = 0;
    run.update(sample(0.0F, t, arrival));
    for (int step = 1; step <= 300; ++step) {
        t += kEpoch;
        arrival += 40'000;
        run.update(sample(4.0F * static_cast<float>(step) * 0.04F, t, arrival));
        if (run.state() == ui::AccelerationState::complete) {
            break;
        }
    }
    const auto frozen = run.elapsed_seconds();
    // Keep accelerating well past the target; the number must not move.
    for (int step = 0; step < 50; ++step) {
        t += kEpoch;
        arrival += 40'000;
        run.update(sample(60.0F, t, arrival));
    }
    assert(run.state() == ui::AccelerationState::complete);
    assert(std::fabs(run.elapsed_seconds() - frozen) < 0.0001F);
}

// Coming back to rest re-arms without being told, which is how this gets used between runs.
void returning_to_rest_re_arms()
{
    ui::AccelerationRun run;
    std::int64_t t = 0;
    std::int64_t arrival = 0;
    run.update(sample(0.0F, t, arrival));
    for (int step = 1; step <= 300; ++step) {
        t += kEpoch;
        arrival += 40'000;
        run.update(sample(4.0F * static_cast<float>(step) * 0.04F, t, arrival));
        if (run.state() == ui::AccelerationState::complete) {
            break;
        }
    }
    assert(run.state() == ui::AccelerationState::complete);

    t += kEpoch;
    arrival += 40'000;
    run.update(sample(0.0F, t, arrival));
    assert(run.state() == ui::AccelerationState::armed);
    assert(run.elapsed_seconds() == 0.0F);
}

void a_tap_resets_at_any_time()
{
    ui::AccelerationRun run;
    run.update(sample(0.0F, 0, 0));
    run.update(sample(8.0F, kEpoch, 40'000));
    assert(run.state() == ui::AccelerationState::running);
    run.reset();
    assert(run.state() == ui::AccelerationState::armed);
    assert(run.elapsed_seconds() == 0.0F);
}

// The failure this mode is built around: a run that stopped being measured looks exactly
// like a run that finished, because in both cases the number stops moving.
void a_run_that_loses_its_fixes_is_never_mistaken_for_a_finished_one()
{
    ui::AccelerationRun run;
    run.update(sample(0.0F, 0, 0));
    run.update(sample(8.0F, kEpoch, 40'000));
    assert(run.state() == ui::AccelerationState::running);

    // Same fix again, long after it arrived: the receiver has gone quiet.
    auto stale = sample(8.0F, kEpoch, 40'000);
    stale.now_monotonic_us = 40'000 + ui::kAccelerationStaleUs + 1;
    run.update(stale);
    assert(run.state() == ui::AccelerationState::abandoned);

    const auto view = run.view_model(stale);
    assert(std::strcmp(view.status.data(), "LOST FIX") == 0);
    // Never green. Green means finished, and this did not.
    assert(view.time_rgb != ui::color::positive_bright);
}

void a_slower_target_finishes_sooner()
{
    const auto run_to = [](const std::uint8_t mph) {
        ui::AccelerationRun run;
        std::int64_t t = 0;
        std::int64_t arrival = 0;
        auto first = sample(0.0F, t, arrival);
        first.target_mph = mph;
        run.update(first);
        for (int step = 1; step <= 400; ++step) {
            t += kEpoch;
            arrival += 40'000;
            auto next = sample(4.0F * static_cast<float>(step) * 0.04F, t, arrival);
            next.target_mph = mph;
            run.update(next);
            if (run.state() == ui::AccelerationState::complete) {
                break;
            }
        }
        assert(run.state() == ui::AccelerationState::complete);
        return run.elapsed_seconds();
    };
    // 30 mph is 13.4112 m/s, so at 4 m/s^2 it arrives at 3.3528 s - half the 0-60.
    assert(std::fabs(run_to(30) - 3.3528F) < 0.01F);
    assert(run_to(30) < run_to(60));
    assert(run_to(60) < run_to(100));
}

void every_state_has_a_name_and_untruncated_text()
{
    const ui::AccelerationState states[] = {
        ui::AccelerationState::no_receiver, ui::AccelerationState::no_fix,
        ui::AccelerationState::no_speed,    ui::AccelerationState::armed,
        ui::AccelerationState::running,     ui::AccelerationState::complete,
        ui::AccelerationState::abandoned,
    };
    for (const auto state : states) {
        assert(std::strlen(ui::acceleration_state_name(state)) > 0);
    }
    ui::AccelerationRun run;
    const auto snapshot = sample(0.0F, 0);
    run.update(snapshot);
    const auto view = run.view_model(snapshot);
    assert(std::strlen(view.status.data()) < ui::kAccelerationStatusCapacity - 1);
    assert(std::strlen(view.target.data()) < ui::kAccelerationTargetCapacity - 1);
    assert(std::strlen(view.time.data()) < ui::kAccelerationTimeCapacity - 1);
}

}  // namespace

int main()
{
    the_start_is_extrapolated_back_to_zero_speed();
    the_finish_is_interpolated_between_the_straddling_fixes();
    an_absent_receiver_shows_no_time();
    a_receiver_unsure_of_its_speed_will_not_time_a_run();
    standing_still_is_armed_and_reads_zero();
    a_run_times_from_rest_to_the_target_and_stops();
    the_time_holds_once_the_target_is_passed();
    returning_to_rest_re_arms();
    a_tap_resets_at_any_time();
    a_run_that_loses_its_fixes_is_never_mistaken_for_a_finished_one();
    a_slower_target_finishes_sooner();
    every_state_has_a_name_and_untruncated_text();

    std::cout << "0-X acceleration: both ends of the run interpolated rather than "
                 "quantised to the fix rate, and a run that lost its fixes never renders "
                 "as a finished one\n";
    return 0;
}

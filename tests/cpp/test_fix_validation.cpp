#include "track_timer/gnss/fix_validation.hpp"
#include "track_timer/timing/crossing_validation.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

namespace {

using namespace track_timer;

constexpr std::int64_t kEpochNs = 400'000'000;   // 400 s into the GNSS week
constexpr std::int64_t kIntervalNs = 40'000'000; // 25 Hz
constexpr std::int64_t kIntervalUs = 40'000;

// A car at 62.6 m/s, 140 mph: the same sample the UBX parser and GPS Only tests use, so
// all three agree on what a fast lap looks like.
domain::GnssFix decoded_sample()
{
    domain::GnssFix fix{};
    fix.measurement_time_ns = kEpochNs;
    fix.latitude_deg = 52.4305520;
    fix.longitude_deg = -1.4777130;
    fix.height_m = 118.0F;
    fix.speed_mps = 62.6F;
    fix.heading_deg = 274.0F;
    fix.horizontal_accuracy_m = 0.8F;
    fix.speed_accuracy_mps = 0.12F;
    fix.heading_accuracy_deg = 0.5F;
    fix.num_satellites = 14;
    fix.fix_type = domain::FixType::fix_3d;
    fix.valid_flags = gnss::kGnssFixOk | gnss::kValidDate | gnss::kValidTime;
    return fix;
}

domain::GnssFix at_epoch(const int index)
{
    auto fix = decoded_sample();
    fix.measurement_time_ns = kEpochNs + kIntervalNs * index;
    return fix;
}

std::int64_t arrival(const int index) { return 1'000'000 + kIntervalUs * index; }

// Accepts one fix so the ordering and motion checks have a reference to work from.
void prime(gnss::FixValidator& validator)
{
    const auto first = validator.evaluate(at_epoch(0), arrival(0));
    assert(first.accepted_for_timing);
}

void a_clean_fix_is_accepted_and_completed()
{
    gnss::FixValidator validator{};
    const auto judged = validator.evaluate(decoded_sample(), 1'234'567);

    assert(judged.accepted_for_timing);
    assert(judged.reject_reason == domain::FixRejectReason::none);
    // The four fields the decoder deliberately leaves unset are what this stage adds.
    assert(judged.arrival_monotonic_us == 1'234'567);
    assert(judged.sequence_number == 1);
    assert(validator.counters().accepted == 1);
    assert(validator.counters().evaluated == 1);
}

// The design point of this gate, asserted against the real crossing thresholds rather
// than restated as a number: a fix too imprecise to time a lap against is still a usable
// observation here. If this gate were as strict, every such fix would be refused before
// the crossing validator ever saw it, and the reason a lap was missed would arrive there
// as source_fix_rejected, which says nothing.
void the_receiver_gate_is_looser_than_the_lap_timing_gate()
{
    auto imprecise = decoded_sample();
    imprecise.horizontal_accuracy_m = timing::kDefaultMaximumHorizontalAccuracyM + 3.0F;
    assert(imprecise.horizontal_accuracy_m <
           gnss::kDefaultMaximumHorizontalAccuracyM);

    gnss::FixValidator validator{};
    const auto judged = validator.evaluate(imprecise, arrival(0));
    assert(judged.accepted_for_timing);
}

void an_unusable_status_is_refused()
{
    // gnssFixOK clear. The receiver is telling us not to trust it.
    {
        auto fix = decoded_sample();
        fix.valid_flags &= ~gnss::kGnssFixOk;
        gnss::FixValidator validator{};
        assert(validator.evaluate(fix, arrival(0)).reject_reason ==
               domain::FixRejectReason::invalid_status);
    }
    // No position at all.
    for (const auto type : {domain::FixType::no_fix, domain::FixType::time_only}) {
        auto fix = decoded_sample();
        fix.fix_type = type;
        gnss::FixValidator validator{};
        assert(validator.evaluate(fix, arrival(0)).reject_reason ==
               domain::FixRejectReason::invalid_status);
    }
    // Garbage that would otherwise propagate into the projection as a NaN.
    {
        auto fix = decoded_sample();
        fix.latitude_deg = std::nan("");
        gnss::FixValidator validator{};
        assert(validator.evaluate(fix, arrival(0)).reject_reason ==
               domain::FixRejectReason::invalid_status);
    }
    // A 2D fix is usable here by default, because height is not used for lap timing - but
    // a policy may insist, and then it is not.
    {
        auto fix = decoded_sample();
        fix.fix_type = domain::FixType::fix_2d;
        gnss::FixValidator permissive{};
        assert(permissive.evaluate(fix, arrival(0)).accepted_for_timing);

        gnss::FixQualityPolicy strict{};
        strict.require_three_dimensional = true;
        gnss::FixValidator demanding{strict};
        assert(demanding.evaluate(fix, arrival(0)).reject_reason ==
               domain::FixRejectReason::invalid_status);
    }
}

void arrival_order_and_measurement_order_are_judged_separately()
{
    // The MCU clock going backwards: the caller handed observations over out of order.
    {
        gnss::FixValidator validator{};
        prime(validator);
        const auto judged = validator.evaluate(at_epoch(1), arrival(0) - 1);
        assert(judged.reject_reason == domain::FixRejectReason::non_monotonic_sequence);
    }
    // The receiver's own clock going backwards, with arrival perfectly in order.
    {
        gnss::FixValidator validator{};
        prime(validator);
        const auto judged = validator.evaluate(at_epoch(-1), arrival(1));
        assert(judged.reject_reason == domain::FixRejectReason::non_monotonic_time);
    }
}

// The receiver repeating an epoch it has already sent. The observation is not new,
// whatever else is true of it.
void a_repeated_epoch_is_stale()
{
    gnss::FixValidator validator{};
    prime(validator);
    const auto judged = validator.evaluate(at_epoch(0), arrival(1));
    assert(judged.reject_reason == domain::FixRejectReason::stale);
}

void an_accuracy_beyond_usable_is_refused()
{
    gnss::FixValidator validator{};
    prime(validator);
    auto lost = at_epoch(1);
    lost.horizontal_accuracy_m = gnss::kDefaultMaximumHorizontalAccuracyM + 10.0F;
    assert(validator.evaluate(lost, arrival(1)).reject_reason ==
           domain::FixRejectReason::excessive_accuracy);
}

// Three different faults, all implausible motion, because each is a way a receiver lies
// after losing lock.
void motion_a_car_cannot_perform_is_refused()
{
    // A velocity no car reaches.
    {
        gnss::FixValidator validator{};
        prime(validator);
        auto fast = at_epoch(1);
        fast.speed_mps = 200.0F;
        assert(validator.evaluate(fast, arrival(1)).reject_reason ==
               domain::FixRejectReason::implausible_motion);
    }
    // An acceleration no car reaches: 20 m/s gained in 40 ms is about 50 g.
    {
        gnss::FixValidator validator{};
        prime(validator);
        auto surge = at_epoch(1);
        surge.speed_mps = 82.6F;
        assert(validator.evaluate(surge, arrival(1)).reject_reason ==
               domain::FixRejectReason::implausible_motion);
    }
    // A position jump while the reported speed stays calm. A receiver recovering from lost
    // lock does exactly this, so the position check catches what the speed check cannot.
    {
        gnss::FixValidator validator{};
        prime(validator);
        auto teleport = at_epoch(1);
        teleport.latitude_deg += 0.001;  // about 111 m, in 40 ms
        assert(validator.evaluate(teleport, arrival(1)).reject_reason ==
               domain::FixRejectReason::implausible_motion);
    }
}

// Comparing against the last accepted fix rather than the last seen one. Otherwise one
// corrupt observation becomes the reference that condemns every good fix after it.
void a_corrupt_fix_does_not_become_the_reference()
{
    gnss::FixValidator validator{};
    prime(validator);

    auto teleport = at_epoch(1);
    teleport.latitude_deg += 0.001;
    assert(!validator.evaluate(teleport, arrival(1)).accepted_for_timing);

    // The next ordinary fix, continuous with the last good one, is accepted.
    assert(validator.evaluate(at_epoch(2), arrival(2)).accepted_for_timing);
}

// "Quality policy can evolve without losing raw evidence": a refusal must not cost a
// single reported value, or a replay could not put a different policy against it.
void a_rejected_fix_keeps_every_raw_value()
{
    gnss::FixValidator validator{};
    auto refused = decoded_sample();
    refused.valid_flags &= ~gnss::kGnssFixOk;

    const auto judged = validator.evaluate(refused, arrival(0));
    assert(!judged.accepted_for_timing);
    assert(judged.latitude_deg == refused.latitude_deg);
    assert(judged.longitude_deg == refused.longitude_deg);
    assert(judged.speed_mps == refused.speed_mps);
    assert(judged.heading_deg == refused.heading_deg);
    assert(judged.horizontal_accuracy_m == refused.horizontal_accuracy_m);
    assert(judged.num_satellites == refused.num_satellites);
    assert(judged.measurement_time_ns == refused.measurement_time_ns);
    assert(judged.valid_flags == refused.valid_flags);
    assert(judged.fix_type == refused.fix_type);
}

// A log that only numbered the fixes it liked would show no gap where a rejection
// happened, and the record would read as though nothing had arrived.
void every_observation_is_numbered_whether_or_not_it_survives()
{
    gnss::FixValidator validator{};
    assert(validator.evaluate(at_epoch(0), arrival(0)).sequence_number == 1);

    auto refused = at_epoch(1);
    refused.valid_flags &= ~gnss::kGnssFixOk;
    const auto rejected = validator.evaluate(refused, arrival(1));
    assert(rejected.sequence_number == 2);
    assert(!rejected.accepted_for_timing);

    assert(validator.evaluate(at_epoch(2), arrival(2)).sequence_number == 3);
    assert(validator.counters().evaluated == 3);
    assert(validator.counters().accepted == 2);
    assert(validator.counters().rejected == 1);
    assert(validator.counters().by_reason[static_cast<std::size_t>(
               domain::FixRejectReason::invalid_status)] == 1);
}

void the_same_history_always_yields_the_same_reason()
{
    const auto run = []() {
        gnss::FixValidator validator{};
        prime(validator);
        auto broken = at_epoch(1);
        broken.horizontal_accuracy_m = 80.0F;
        broken.speed_mps = 300.0F;  // both faults at once
        return validator.evaluate(broken, arrival(1)).reject_reason;
    };
    // Accuracy is checked before motion, so the reason is settled by the order of the
    // checks and not by which fault is "worse".
    assert(run() == domain::FixRejectReason::excessive_accuracy);
    assert(run() == run());
}

// The distinction GPS Only mode exists to show, and the one the ready dashboard needs:
// nothing connected is not the same as connected and still searching.
void health_separates_nothing_connected_from_not_yet_fixed()
{
    gnss::ReceiverStatus status{};
    assert(status.health(0) == domain::GnssHealth::unavailable);

    status.note_traffic(1'000'000);
    assert(status.health(1'000'000) == domain::GnssHealth::searching);
}

void health_degrades_and_recovers_with_the_stream()
{
    gnss::FixValidator validator{};
    gnss::ReceiverStatus status{};

    const auto good = validator.evaluate(at_epoch(0), arrival(0));
    status.observe(good);
    assert(status.health(arrival(0)) == domain::GnssHealth::good);

    // A refusal makes the receiver poor rather than dead; it is still talking.
    auto refused = at_epoch(1);
    refused.valid_flags &= ~gnss::kGnssFixOk;
    status.observe(validator.evaluate(refused, arrival(1)));
    assert(status.health(arrival(1)) == domain::GnssHealth::poor);

    // A good fix restores it.
    status.observe(validator.evaluate(at_epoch(2), arrival(2)));
    assert(status.health(arrival(2)) == domain::GnssHealth::good);

    // Silence past the threshold is stale, because a fix was seen once.
    assert(status.health(arrival(2) + gnss::kDefaultStaleAfterUs + 1) ==
           domain::GnssHealth::stale);
}

// Usable but imprecise is reported as poor, so the health degrades before lap timing does
// rather than at the same moment.
void a_usable_but_imprecise_fix_reports_poor()
{
    gnss::FixValidator validator{};
    gnss::ReceiverStatus status{};

    auto imprecise = at_epoch(0);
    imprecise.horizontal_accuracy_m = gnss::kDefaultGoodHorizontalAccuracyM + 5.0F;
    const auto judged = validator.evaluate(imprecise, arrival(0));
    assert(judged.accepted_for_timing);

    status.observe(judged);
    assert(status.health(arrival(0)) == domain::GnssHealth::poor);
}

void names_are_stable_for_logging_and_replay()
{
    assert(std::strcmp(gnss::fix_reject_reason_name(domain::FixRejectReason::none),
                       "accepted") == 0);
    for (const auto reason :
         {domain::FixRejectReason::invalid_status, domain::FixRejectReason::stale,
          domain::FixRejectReason::non_monotonic_sequence,
          domain::FixRejectReason::non_monotonic_time,
          domain::FixRejectReason::excessive_accuracy,
          domain::FixRejectReason::implausible_motion}) {
        assert(std::strcmp(gnss::fix_reject_reason_name(reason), "unknown") != 0);
        assert(std::strlen(gnss::fix_reject_reason_name(reason)) > 0);
    }
    for (const auto health :
         {domain::GnssHealth::unavailable, domain::GnssHealth::searching,
          domain::GnssHealth::poor, domain::GnssHealth::good, domain::GnssHealth::stale}) {
        assert(std::strcmp(gnss::gnss_health_name(health), "unknown") != 0);
    }
}

void a_reset_forgets_the_history_but_not_the_policy()
{
    gnss::FixQualityPolicy policy{};
    policy.require_three_dimensional = true;
    gnss::FixValidator validator{policy};
    prime(validator);
    validator.reset();

    assert(validator.counters().evaluated == 0);
    assert(validator.policy().require_three_dimensional);
    // With no history, an epoch earlier than the one already seen is accepted again.
    assert(validator.evaluate(at_epoch(-5), arrival(0)).accepted_for_timing);
}

}  // namespace

int main()
{
    a_clean_fix_is_accepted_and_completed();
    the_receiver_gate_is_looser_than_the_lap_timing_gate();
    an_unusable_status_is_refused();
    arrival_order_and_measurement_order_are_judged_separately();
    a_repeated_epoch_is_stale();
    an_accuracy_beyond_usable_is_refused();
    motion_a_car_cannot_perform_is_refused();
    a_corrupt_fix_does_not_become_the_reference();
    a_rejected_fix_keeps_every_raw_value();
    every_observation_is_numbered_whether_or_not_it_survives();
    the_same_history_always_yields_the_same_reason();
    health_separates_nothing_connected_from_not_yet_fixed();
    health_degrades_and_recovers_with_the_stream();
    a_usable_but_imprecise_fix_reports_poor();
    names_are_stable_for_logging_and_replay();
    a_reset_forgets_the_history_but_not_the_policy();

    std::cout << "Fix quality: every rejection has one deterministic reason, raw evidence "
                 "survives refusal, and receiver health separates absent from searching\n";
    return 0;
}

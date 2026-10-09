#include "track_timer/gnss/fix_validation.hpp"

#include <cmath>

namespace track_timer::gnss {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMetresPerDegreeLatitude = 111'320.0;
constexpr double kNanosecondsPerSecond = 1.0e9;

[[nodiscard]] bool values_are_sane(const domain::GnssFix& fix) noexcept
{
    return std::isfinite(fix.latitude_deg) && std::isfinite(fix.longitude_deg) &&
           std::isfinite(fix.height_m) && std::isfinite(fix.speed_mps) &&
           std::isfinite(fix.heading_deg) && std::isfinite(fix.horizontal_accuracy_m) &&
           std::isfinite(fix.speed_accuracy_mps) &&
           std::isfinite(fix.heading_accuracy_deg) && fix.latitude_deg >= -90.0 &&
           fix.latitude_deg <= 90.0 && fix.longitude_deg >= -180.0 &&
           fix.longitude_deg <= 180.0 && fix.speed_mps >= 0.0F &&
           fix.horizontal_accuracy_m >= 0.0F && fix.speed_accuracy_mps >= 0.0F;
}

[[nodiscard]] bool has_a_position(const domain::FixType type) noexcept
{
    return type != domain::FixType::no_fix && type != domain::FixType::time_only;
}

[[nodiscard]] bool is_three_dimensional(const domain::FixType type) noexcept
{
    return type == domain::FixType::fix_3d || type == domain::FixType::gnss_dead_reckoning;
}

// A deliberately rough equirectangular distance. This is a plausibility bound, not a
// measurement: the projection lap timing uses is circuit-local and lives in the track
// component, and reaching for it here would tie the receiver gate to a loaded track - so a
// fix could not be judged before a circuit was chosen. Over the tens of metres a fraction
// of a second of driving covers, the error is orders of magnitude below the threshold
// being tested.
[[nodiscard]] double rough_distance_m(const domain::GnssFix& from,
                                      const domain::GnssFix& to) noexcept
{
    const auto mean_latitude_rad =
        (from.latitude_deg + to.latitude_deg) * 0.5 * kPi / 180.0;
    const auto north_m = (to.latitude_deg - from.latitude_deg) * kMetresPerDegreeLatitude;
    const auto east_m = (to.longitude_deg - from.longitude_deg) *
                        kMetresPerDegreeLatitude * std::cos(mean_latitude_rad);
    return std::sqrt(north_m * north_m + east_m * east_m);
}

}  // namespace

FixValidator::FixValidator(const FixQualityPolicy& policy) noexcept : policy_(policy) {}

// The order of these checks is the contract: the first failure wins, so a given
// observation against a given history always yields the same reason. Reordering them
// would silently change what a log says about the past.
domain::FixRejectReason FixValidator::judge(
    const domain::GnssFix& candidate, const std::int64_t arrival_monotonic_us) const noexcept
{
    if (!values_are_sane(candidate) || !has_a_position(candidate.fix_type) ||
        (candidate.valid_flags & kGnssFixOk) == 0U ||
        (policy_.require_three_dimensional && !is_three_dimensional(candidate.fix_type))) {
        return domain::FixRejectReason::invalid_status;
    }

    // Arrival order, on the MCU clock. A monotonic clock that goes backwards means the
    // caller handed observations over out of order, which would corrupt every age this
    // system computes.
    if (last_arrival_us_ != domain::kUnavailableTime &&
        arrival_monotonic_us < last_arrival_us_) {
        return domain::FixRejectReason::non_monotonic_sequence;
    }

    if (!has_accepted_) {
        return domain::FixRejectReason::none;
    }

    // Measurement order, on the receiver's clock. Compared against the last *accepted*
    // fix rather than the last seen one, so a single corrupt observation cannot become the
    // reference that condemns everything after it.
    if (candidate.measurement_time_ns < last_accepted_.measurement_time_ns) {
        return domain::FixRejectReason::non_monotonic_time;
    }
    // The same epoch twice. The observation is not new, whatever else is true of it.
    if (candidate.measurement_time_ns == last_accepted_.measurement_time_ns) {
        return domain::FixRejectReason::stale;
    }

    if (candidate.horizontal_accuracy_m > policy_.maximum_horizontal_accuracy_m ||
        candidate.speed_accuracy_mps > policy_.maximum_speed_accuracy_mps) {
        return domain::FixRejectReason::excessive_accuracy;
    }

    if (candidate.speed_mps > policy_.maximum_speed_mps) {
        return domain::FixRejectReason::implausible_motion;
    }

    const auto interval_s =
        static_cast<double>(candidate.measurement_time_ns -
                            last_accepted_.measurement_time_ns) / kNanosecondsPerSecond;
    if (interval_s > 0.0) {
        const auto speed_change =
            std::fabs(static_cast<double>(candidate.speed_mps - last_accepted_.speed_mps));
        if (speed_change / interval_s > static_cast<double>(policy_.maximum_acceleration_mps2)) {
            return domain::FixRejectReason::implausible_motion;
        }
        // A position that moved further than the speed limit allows, whatever the reported
        // speed says. A receiver recovering from lost lock jumps position while still
        // reporting a calm velocity, so the two checks catch different faults.
        if (rough_distance_m(last_accepted_, candidate) / interval_s >
            static_cast<double>(policy_.maximum_speed_mps)) {
            return domain::FixRejectReason::implausible_motion;
        }
    }

    return domain::FixRejectReason::none;
}

domain::GnssFix FixValidator::evaluate(const domain::GnssFix& decoded,
                                       const std::int64_t arrival_monotonic_us) noexcept
{
    auto fix = decoded;
    fix.arrival_monotonic_us = arrival_monotonic_us;
    // Every observation gets an ordinal, accepted or not, so a log reads as a complete
    // record of what arrived rather than of what survived.
    fix.sequence_number = counters_.evaluated + 1;

    const auto reason = judge(fix, arrival_monotonic_us);
    fix.reject_reason = reason;
    fix.accepted_for_timing = reason == domain::FixRejectReason::none;

    ++counters_.evaluated;
    if (fix.accepted_for_timing) {
        ++counters_.accepted;
        last_accepted_ = fix;
        has_accepted_ = true;
    }
    else {
        ++counters_.rejected;
    }
    ++counters_.by_reason[static_cast<std::size_t>(reason)];

    // Arrival advances on every observation, accepted or not: it records when the transport
    // last handed something over, which is a fact about the link rather than about quality.
    if (last_arrival_us_ == domain::kUnavailableTime ||
        arrival_monotonic_us >= last_arrival_us_) {
        last_arrival_us_ = arrival_monotonic_us;
    }
    return fix;
}

void FixValidator::reset() noexcept
{
    counters_ = {};
    last_accepted_ = {};
    last_arrival_us_ = domain::kUnavailableTime;
    has_accepted_ = false;
}

const FixValidatorCounters& FixValidator::counters() const noexcept { return counters_; }

const FixQualityPolicy& FixValidator::policy() const noexcept { return policy_; }

const domain::GnssFix& FixValidator::last_accepted() const noexcept { return last_accepted_; }

bool FixValidator::has_accepted() const noexcept { return has_accepted_; }

ReceiverStatus::ReceiverStatus(const FixQualityPolicy& policy) noexcept : policy_(policy) {}

void ReceiverStatus::note_traffic(const std::int64_t arrival_monotonic_us) noexcept
{
    last_traffic_us_ = arrival_monotonic_us;
}

void ReceiverStatus::observe(const domain::GnssFix& judged) noexcept
{
    last_traffic_us_ = judged.arrival_monotonic_us;
    if (judged.accepted_for_timing) {
        last_accepted_us_ = judged.arrival_monotonic_us;
        last_accepted_accuracy_m = judged.horizontal_accuracy_m;
        consecutive_rejections_ = 0;
        has_accepted_ = true;
        return;
    }
    ++consecutive_rejections_;
}

domain::GnssHealth ReceiverStatus::health(const std::int64_t now_monotonic_us) const noexcept
{
    // Nothing has ever been heard from the receiver. This is the state the device is in
    // with no transport at all, and it must be distinguishable from a receiver that is
    // present and has not fixed yet.
    if (last_traffic_us_ == domain::kUnavailableTime) {
        return domain::GnssHealth::unavailable;
    }
    if (now_monotonic_us - last_traffic_us_ > policy_.stale_after_us) {
        // Traffic stopped. If it never produced a fix, the receiver was never really there
        // as far as timing is concerned.
        return has_accepted_ ? domain::GnssHealth::stale : domain::GnssHealth::unavailable;
    }
    if (!has_accepted_) {
        return domain::GnssHealth::searching;
    }
    if (now_monotonic_us - last_accepted_us_ > policy_.stale_after_us) {
        return domain::GnssHealth::stale;
    }
    if (consecutive_rejections_ > 0 ||
        last_accepted_accuracy_m > policy_.good_horizontal_accuracy_m) {
        return domain::GnssHealth::poor;
    }
    return domain::GnssHealth::good;
}

void ReceiverStatus::reset() noexcept
{
    last_traffic_us_ = domain::kUnavailableTime;
    last_accepted_us_ = domain::kUnavailableTime;
    last_accepted_accuracy_m = 0.0F;
    consecutive_rejections_ = 0;
    has_accepted_ = false;
}

const char* fix_reject_reason_name(const domain::FixRejectReason reason) noexcept
{
    switch (reason) {
    case domain::FixRejectReason::none:
        return "accepted";
    case domain::FixRejectReason::invalid_status:
        return "invalid-status";
    case domain::FixRejectReason::stale:
        return "stale";
    case domain::FixRejectReason::non_monotonic_sequence:
        return "non-monotonic-sequence";
    case domain::FixRejectReason::non_monotonic_time:
        return "non-monotonic-time";
    case domain::FixRejectReason::excessive_accuracy:
        return "excessive-accuracy";
    case domain::FixRejectReason::implausible_motion:
        return "implausible-motion";
    }
    return "unknown";
}

const char* gnss_health_name(const domain::GnssHealth health) noexcept
{
    switch (health) {
    case domain::GnssHealth::unavailable:
        return "unavailable";
    case domain::GnssHealth::searching:
        return "searching";
    case domain::GnssHealth::poor:
        return "poor";
    case domain::GnssHealth::good:
        return "good";
    case domain::GnssHealth::stale:
        return "stale";
    }
    return "unknown";
}

}  // namespace track_timer::gnss

#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::gnss {

// Judging a decoded observation, and reporting what the receiver is doing.
//
// The NAV-PVT decoder reports what the message said and nothing more. Four fields of
// domain::GnssFix are deliberately left unset there because they are not in the message -
// they are what this system knows about the observation rather than what the receiver
// claimed: when it arrived, where it sits in the sequence we have seen, whether it is fit
// to time against, and if not, why not. Filling those in is this file's whole job.
//
// Two clock domains are kept apart and never compared with each other:
//
//   measurement_time_ns   the receiver's own time of week, in nanoseconds. Durations
//                         between fixes are measured here, because this is the clock the
//                         position was actually sampled against.
//   arrival_monotonic_us  the MCU's monotonic clock, in microseconds. Only ever compared
//                         with other arrivals, to ask how long the receiver has been quiet.
//
// Comparing one against the other would be meaningless: they share no epoch, and the
// offset between them includes the whole transport.

// Bit positions inside domain::GnssFix::valid_flags. The decoder packs NAV-PVT's own
// `valid` byte into bits 0-7 and its `flags` byte into bits 8-15, keeping both whole so a
// policy can change its mind later without having lost the evidence it would need.
inline constexpr std::uint32_t kValidDate = 1U << 0U;
inline constexpr std::uint32_t kValidTime = 1U << 1U;
inline constexpr std::uint32_t kFullyResolved = 1U << 2U;
inline constexpr std::uint32_t kGnssFixOk = 1U << 8U;
inline constexpr std::uint32_t kDifferentialSolution = 1U << 9U;

// These thresholds are deliberately loose, and that is the most important thing about
// them. This gate asks whether an observation is usable at all - not whether it is good
// enough to time a lap against. timing::validate_crossing already applies the strict
// thresholds (5 m, 2 m/s, 25 degrees, six satellites) to the two fixes either side of the
// line. If this gate were equally strict, a merely imprecise fix would never reach the
// crossing validator, every refusal would surface there as source_fix_rejected, and the
// reason a lap was missed would be lost on the way.

// Fifty metres is "the receiver does not know where it is", not "the receiver is
// imprecise". The crossing gate does the precision work.
inline constexpr float kDefaultMaximumHorizontalAccuracyM = 50.0F;
inline constexpr float kDefaultMaximumSpeedAccuracyMps = 10.0F;

// Below this a fix counts as good rather than merely usable. It sits between this gate's
// 50 m and the crossing gate's 5 m, so the health report degrades before lap timing does.
inline constexpr float kDefaultGoodHorizontalAccuracyM = 10.0F;

// 134 m/s is 300 mph: above anything this device will be fitted to, and below the
// velocities a receiver invents when it loses lock.
inline constexpr float kDefaultMaximumSpeedMps = 134.0F;

// About 5 g. A car on slicks brakes at roughly 1.5 g, so this refuses nonsense rather
// than hard driving.
inline constexpr float kDefaultMaximumAccelerationMps2 = 50.0F;

// How long the receiver may go without an accepted fix before it is called stale rather
// than merely quiet. Four seconds is a hundred missed epochs at 25 Hz.
inline constexpr std::int64_t kDefaultStaleAfterUs = 4'000'000;

struct FixQualityPolicy {
    float maximum_horizontal_accuracy_m{kDefaultMaximumHorizontalAccuracyM};
    float maximum_speed_accuracy_mps{kDefaultMaximumSpeedAccuracyMps};
    float good_horizontal_accuracy_m{kDefaultGoodHorizontalAccuracyM};
    float maximum_speed_mps{kDefaultMaximumSpeedMps};
    float maximum_acceleration_mps2{kDefaultMaximumAccelerationMps2};
    std::int64_t stale_after_us{kDefaultStaleAfterUs};
    // A 2D fix has no height, and height is not used for lap timing, so a 2D fix is still
    // usable here. The crossing gate is where three dimensions are required.
    bool require_three_dimensional{false};
};

inline constexpr std::size_t kFixRejectReasonCount = 7;

struct FixValidatorCounters {
    std::uint32_t evaluated{0};
    std::uint32_t accepted{0};
    std::uint32_t rejected{0};
    std::array<std::uint32_t, kFixRejectReasonCount> by_reason{};
};

class FixValidator {
  public:
    FixValidator() noexcept = default;
    explicit FixValidator(const FixQualityPolicy& policy) noexcept;

    // Completes a decoded observation and judges it.
    //
    // The returned fix is whole whether it was accepted or not: a rejected one keeps every
    // raw value and both flag bytes, so a replay can put a different policy against the
    // same evidence and get a different answer. Nothing is dropped here - dropping is the
    // caller's decision, made with the reason in hand.
    [[nodiscard]] domain::GnssFix evaluate(const domain::GnssFix& decoded,
                                           std::int64_t arrival_monotonic_us) noexcept;

    void reset() noexcept;

    [[nodiscard]] const FixValidatorCounters& counters() const noexcept;
    [[nodiscard]] const FixQualityPolicy& policy() const noexcept;

    // The most recent fix that passed. Readable without draining the queue, because a
    // display is not a consumer: showing the latest reading must not take it away from
    // the timing engine.
    [[nodiscard]] const domain::GnssFix& last_accepted() const noexcept;
    [[nodiscard]] bool has_accepted() const noexcept;

  private:
    [[nodiscard]] domain::FixRejectReason judge(const domain::GnssFix& candidate,
                                                std::int64_t arrival_monotonic_us) const noexcept;

    FixQualityPolicy policy_{};
    FixValidatorCounters counters_{};
    domain::GnssFix last_accepted_{};
    std::int64_t last_arrival_us_{domain::kUnavailableTime};
    bool has_accepted_{false};
};

// What the receiver is doing, as opposed to what any single observation says.
//
// Separate from the validator because the questions are different: the validator judges
// one fix against the one before it, while this watches the stream and answers "is the
// receiver working". A receiver can emit a run of individually valid fixes and still be in
// trouble, and it can be perfectly healthy while searching.
class ReceiverStatus {
  public:
    ReceiverStatus() noexcept = default;
    explicit ReceiverStatus(const FixQualityPolicy& policy) noexcept;

    // Called when the transport sees traffic, whether or not it parsed into anything. This
    // is what separates "nothing is connected" from "something is connected and quiet".
    void note_traffic(std::int64_t arrival_monotonic_us) noexcept;

    void observe(const domain::GnssFix& judged) noexcept;

    [[nodiscard]] domain::GnssHealth health(std::int64_t now_monotonic_us) const noexcept;

    void reset() noexcept;

  private:
    FixQualityPolicy policy_{};
    std::int64_t last_traffic_us_{domain::kUnavailableTime};
    std::int64_t last_accepted_us_{domain::kUnavailableTime};
    float last_accepted_accuracy_m{0.0F};
    std::uint32_t consecutive_rejections_{0};
    bool has_accepted_{false};
};

[[nodiscard]] const char* fix_reject_reason_name(domain::FixRejectReason reason) noexcept;
[[nodiscard]] const char* gnss_health_name(domain::GnssHealth health) noexcept;

static_assert(std::is_trivially_copyable_v<FixQualityPolicy>);
static_assert(std::is_trivially_copyable_v<FixValidatorCounters>);

}  // namespace track_timer::gnss

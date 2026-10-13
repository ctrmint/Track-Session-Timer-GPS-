#pragma once

#include "track_timer/domain/contracts.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// The view model behind Clock mode: a 24-hour GMT wall clock, read from the receiver.
//
// A clock is the one readout where being wrong is indistinguishable from being right.
// GPS Only must not render an absent receiver as 0 mph because a stationary car looks the
// same; here the equivalent mistake is worse, because 00:00:00 is a believable time and
// midnight GMT actually happens. So every state that does not have a trustworthy time
// shows dashes, and the one state that is running without the receiver says so.
//
// Formatting is separated from drawing for that reason: the rules worth being sure about -
// which state is which, where the second boundary falls, what happens at midnight - are
// all testable on the host without a panel.

// "HH:MM", ":SS" and "YYYY-MM-DD", each with room for its terminator.
inline constexpr std::size_t kClockTimeCapacity = 6;
inline constexpr std::size_t kClockSecondsCapacity = 3;
inline constexpr std::size_t kClockDateCapacity = 11;
inline constexpr std::size_t kClockStatusCapacity = 24;

enum class ClockState : std::uint8_t {
    // No bytes have ever arrived. A wiring or power problem, not a satellite one.
    no_receiver,
    // The receiver is talking but has not resolved the time. Normal for the first seconds
    // of a cold start, and the opposite diagnosis from the above.
    waiting,
    // A trustworthy time, recently read.
    live,
    // The receiver has gone quiet, and the clock is free-running on the MCU's own crystal
    // from the last time it did answer.
    //
    // This state exists because the alternative is worse. A clock that freezes on its last
    // good second looks completely correct and is wrong by however long it has been
    // frozen - nothing on the screen contradicts it. Free-running and saying so is honest;
    // a silently stale clock is not.
    holding,
    // Held for longer than this firmware is willing to vouch for. Back to dashes: at that
    // point "the time is X" has become a claim about a crystal rather than about GNSS, and
    // the mode exists to show GNSS time.
    lost,
};

// What the screen is given. Not a GnssFix: the last *valid* time has to outlive the fix
// that carried it, or the holding state could not exist.
struct ClockSnapshot {
    // The most recent trustworthy reading, retained. Invalid means there has never been
    // one, which is what separates no_receiver and waiting from everything else.
    domain::UtcTime utc{};
    bool receiver_present{false};  // bytes have arrived from a receiver
    // When `utc` was read, and what the monotonic clock says now, both in microseconds.
    // Only ever subtracted from one another - they share no epoch with `utc`, which is a
    // calendar time. Unavailable on either means the age is unknown and no advance is
    // applied.
    std::int64_t utc_monotonic_us{domain::kUnavailableTime};
    std::int64_t now_monotonic_us{domain::kUnavailableTime};
};

struct ClockViewModel {
    std::array<char, kClockTimeCapacity> time{};        // "14:32", or "--:--"
    std::array<char, kClockSecondsCapacity> seconds{};  // "07", or "--"
    std::array<char, kClockDateCapacity> date{};        // "2026-10-13", or empty
    std::array<char, kClockStatusCapacity> status{};    // "GMT", "HOLD 2m", "NO RECEIVER"
    std::uint32_t status_rgb{0};
    // False in every state that has no time to show. The screen uses this rather than
    // comparing the strings, so "show nothing" is a decision made once, here.
    bool digits_valid{false};
    ClockState state{ClockState::no_receiver};
};

// Longest a reading may be before the clock stops calling itself live. Two seconds is
// loose on purpose: fixes arrive at 20 Hz, so this only trips when the receiver or the
// task feeding it has actually stopped, not when a poll was late.
inline constexpr std::int64_t kClockFreshnessUs = 2'000'000;

// How long the clock will free-run before giving up on the digits.
//
// Not a drift limit. An ESP32 crystal at +-20 ppm loses about 1.7 s a day, so an hour of
// holding costs well under a tenth of a second - the arithmetic is not what runs out.
// What runs out is the claim: after an hour with nothing from the receiver, "this is GNSS
// time" is no longer true, and this mode has no other source to offer.
inline constexpr std::int64_t kClockHoldLimitUs = 3'600'000'000LL;

[[nodiscard]] ClockState clock_state(const ClockSnapshot& snapshot) noexcept;
[[nodiscard]] ClockViewModel clock_view(const ClockSnapshot& snapshot) noexcept;
[[nodiscard]] const char* clock_state_name(ClockState state) noexcept;

// `utc` moved by a whole number of seconds, forwards or backwards, with the calendar
// handled properly: minute, hour, month-length and leap-year rollovers all included.
//
// Exposed because it is the part most likely to be wrong in a way nothing else notices.
// Both callers of it are off-by-one risks - the sub-second borrow below, and the holding
// advance - and a clock that is a second out, or that turns 2026-02-28 into 2026-02-29,
// is a defect that only shows up on the one day it matters.
//
// One documented limit: a positive leap second is reported by the receiver as second 60,
// and the civil-time arithmetic here has no representation for it. A leap second is
// therefore shown verbatim when no shift is needed, and collapses into the adjacent
// second when one is. Nothing in this project needs better, and nothing should silently
// pretend otherwise.
[[nodiscard]] domain::UtcTime shift_utc(const domain::UtcTime& utc,
                                        std::int64_t seconds) noexcept;

// Where the clock actually is, given the reading's own sub-second part and how long ago it
// arrived. This is the whole-second offset to apply to `utc`.
//
// The sub-second part is the reason this is not simply "show what the receiver said". The
// receiver reports the calendar fields *rounded* to the nearest second with `nano` as the
// signed correction, so a negative `nano` means the true instant is still inside the
// previous second. Showing the rounded field regardless makes the clock up to half a
// second fast - and at 20 Hz roughly half of all readings are the negative case, so it
// would be fast far more often than not.
[[nodiscard]] std::int64_t clock_offset_seconds(const ClockSnapshot& snapshot) noexcept;

static_assert(std::is_trivially_copyable_v<ClockSnapshot>);
static_assert(std::is_trivially_copyable_v<ClockViewModel>);

}  // namespace track_timer::ui

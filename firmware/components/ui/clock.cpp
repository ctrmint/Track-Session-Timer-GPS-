#include "track_timer/ui/clock.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr std::int64_t kSecondsPerDay = 86'400;
constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000;

// Civil date to a day number and back, after Howard Hinnant's chrono-compatible
// algorithms (http://howardhinnant.github.io/date_algorithms.html), which are exact for
// every proleptic Gregorian date and need no tables or branches per month.
//
// Used rather than hand-rolled rollover arithmetic because there are two directions to get
// right here - the sub-second borrow goes backwards, the holding advance forwards - and one
// exact conversion in both directions is far less likely to be wrong than two sets of
// carry rules, each with its own month-length and leap-year special cases.
[[nodiscard]] std::int64_t days_from_civil(std::int32_t year, std::uint32_t month,
                                           std::uint32_t day) noexcept
{
    year -= month <= 2 ? 1 : 0;
    const auto era = (year >= 0 ? year : year - 399) / 400;
    const auto year_of_era = static_cast<std::uint32_t>(year - era * 400);
    const auto day_of_year =
        (153U * (month + (month > 2 ? -3U : 9U)) + 2U) / 5U + day - 1U;
    const auto day_of_era =
        year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    return static_cast<std::int64_t>(era) * 146'097 + static_cast<std::int64_t>(day_of_era) -
           719'468;
}

struct CivilDate {
    std::int32_t year{0};
    std::uint32_t month{0};
    std::uint32_t day{0};
};

[[nodiscard]] CivilDate civil_from_days(std::int64_t days) noexcept
{
    days += 719'468;
    const auto era = (days >= 0 ? days : days - 146'096) / 146'097;
    // At most 146,096, so 32-bit throughout from here. The widths matter: mixing a 64-bit
    // value with the algorithm's unsigned wrap-around steps silently produces a different
    // answer, because the wrap it relies on is a 32-bit one.
    const auto day_of_era = static_cast<std::uint32_t>(days - era * 146'097);
    const std::uint32_t year_of_era =
        (day_of_era - day_of_era / 1460U + day_of_era / 36524U - day_of_era / 146096U) / 365U;
    const auto year = static_cast<std::int64_t>(year_of_era) + era * 400;
    const std::uint32_t day_of_year =
        day_of_era - (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
    const std::uint32_t shifted_month = (5U * day_of_year + 2U) / 153U;
    const std::uint32_t day = day_of_year - (153U * shifted_month + 2U) / 5U + 1U;
    const std::uint32_t month =
        shifted_month < 10U ? shifted_month + 3U : shifted_month - 9U;
    return {static_cast<std::int32_t>(year + (month <= 2U ? 1 : 0)), month, day};
}

// Rounds towards negative infinity, which is what a clock needs and what C++ integer
// division does not do. A reading 400 ms into the previous second must land on that second,
// not be truncated towards zero into this one.
[[nodiscard]] std::int64_t floor_divide(const std::int64_t numerator,
                                        const std::int64_t denominator) noexcept
{
    const auto quotient = numerator / denominator;
    const auto remainder = numerator % denominator;
    return (remainder != 0 && ((remainder < 0) != (denominator < 0))) ? quotient - 1
                                                                      : quotient;
}

[[nodiscard]] std::int64_t floor_modulo(const std::int64_t numerator,
                                        const std::int64_t denominator) noexcept
{
    return numerator - floor_divide(numerator, denominator) * denominator;
}

void write_text(char* const destination, const std::size_t capacity,
                const char* const text) noexcept
{
    std::snprintf(destination, capacity, "%s", text);
}

// How long the clock has been free-running, in words a driver can act on. Minutes are
// enough: the point of the number is to say whether this is a hiccup or an outage.
void write_hold_text(char* const destination, const std::size_t capacity,
                      const std::int64_t age_us) noexcept
{
    const auto seconds = age_us / 1'000'000;
    if (seconds < 60) {
        std::snprintf(destination, capacity, "HOLD %llds",
                      static_cast<long long>(seconds));
        return;
    }
    std::snprintf(destination, capacity, "HOLD %lldm",
                  static_cast<long long>(seconds / 60));
}

// Unknown rather than zero when either timestamp is missing, so a caller that has no
// monotonic clock - a host test, or the simulator - gets the receiver's reading shown as
// it stands instead of an advance computed from a sentinel.
[[nodiscard]] bool reading_age_us(const ClockSnapshot& snapshot, std::int64_t& age) noexcept
{
    if (snapshot.utc_monotonic_us == domain::kUnavailableTime ||
        snapshot.now_monotonic_us == domain::kUnavailableTime) {
        return false;
    }
    const auto elapsed = snapshot.now_monotonic_us - snapshot.utc_monotonic_us;
    if (elapsed < 0) {
        // The monotonic clock should make this impossible. If it happens, advancing by a
        // negative age would move the clock backwards, so it is treated as unknown.
        return false;
    }
    age = elapsed;
    return true;
}

}  // namespace

domain::UtcTime shift_utc(const domain::UtcTime& utc, const std::int64_t seconds) noexcept
{
    if (seconds == 0) {
        return utc;
    }
    const auto day = days_from_civil(static_cast<std::int32_t>(utc.year), utc.month, utc.day);
    const auto second_of_day = static_cast<std::int64_t>(utc.hour) * 3600 +
                               static_cast<std::int64_t>(utc.minute) * 60 +
                               static_cast<std::int64_t>(utc.second);
    const auto total = day * kSecondsPerDay + second_of_day + seconds;

    const auto shifted_second_of_day = floor_modulo(total, kSecondsPerDay);
    const auto date = civil_from_days(floor_divide(total, kSecondsPerDay));

    domain::UtcTime shifted{};
    shifted.year = static_cast<std::uint16_t>(date.year);
    shifted.month = static_cast<std::uint8_t>(date.month);
    shifted.day = static_cast<std::uint8_t>(date.day);
    shifted.hour = static_cast<std::uint8_t>(shifted_second_of_day / 3600);
    shifted.minute = static_cast<std::uint8_t>(shifted_second_of_day % 3600 / 60);
    shifted.second = static_cast<std::uint8_t>(shifted_second_of_day % 60);
    // The shift does not make an invalid reading valid, and does not invalidate a valid
    // one: it moves a time the receiver already vouched for.
    shifted.valid = utc.valid;
    // Deliberately dropped. The fraction was consumed by computing this shift, so keeping
    // it would invite a second consumer adding it again - which is exactly the mistake
    // #193 was.
    shifted.nanosecond = 0;
    return shifted;
}

std::int64_t clock_offset_seconds(const ClockSnapshot& snapshot) noexcept
{
    std::int64_t age = 0;
    const auto have_age = reading_age_us(snapshot, age);
    const auto elapsed_ns = have_age ? age * 1'000 : 0;
    return floor_divide(static_cast<std::int64_t>(snapshot.utc.nanosecond) + elapsed_ns,
                        kNanosecondsPerSecond);
}

ClockState clock_state(const ClockSnapshot& snapshot) noexcept
{
    if (!snapshot.utc.valid) {
        // Never had a time. Which of the two reasons it is depends only on whether the
        // receiver has ever said anything, and they have opposite fixes: one is a cable,
        // the other is patience.
        return snapshot.receiver_present ? ClockState::waiting : ClockState::no_receiver;
    }
    std::int64_t age = 0;
    if (!reading_age_us(snapshot, age)) {
        // No usable age, so there is no evidence the reading is stale. Shown as it stands.
        return ClockState::live;
    }
    if (age <= kClockFreshnessUs) {
        return ClockState::live;
    }
    return age <= kClockHoldLimitUs ? ClockState::holding : ClockState::lost;
}

ClockViewModel clock_view(const ClockSnapshot& snapshot) noexcept
{
    ClockViewModel model{};
    model.state = clock_state(snapshot);

    switch (model.state) {
    case ClockState::no_receiver:
        write_text(model.time.data(), model.time.size(), "--:--");
        write_text(model.seconds.data(), model.seconds.size(), "--");
        write_text(model.status.data(), model.status.size(), "NO RECEIVER");
        model.status_rgb = color::critical_bright;
        return model;
    case ClockState::waiting:
        write_text(model.time.data(), model.time.size(), "--:--");
        write_text(model.seconds.data(), model.seconds.size(), "--");
        write_text(model.status.data(), model.status.size(), "WAITING FOR TIME");
        model.status_rgb = color::caution_bright;
        return model;
    case ClockState::lost:
        write_text(model.time.data(), model.time.size(), "--:--");
        write_text(model.seconds.data(), model.seconds.size(), "--");
        write_text(model.status.data(), model.status.size(), "TIME LOST");
        model.status_rgb = color::critical_bright;
        return model;
    case ClockState::live:
    case ClockState::holding:
        break;
    }

    const auto shown = shift_utc(snapshot.utc, clock_offset_seconds(snapshot));
    model.digits_valid = true;
    // The modulos bound each field to the width its buffer holds. shift_utc already
    // produces in-range values, but snprintf truncates silently, so a field that somehow
    // arrived out of range would otherwise cut the string short and shift the whole face
    // rather than showing one wrong number. These make the width a property of the format
    // instead of a property of the caller - and let the compiler verify it.
    std::snprintf(model.time.data(), model.time.size(), "%02u:%02u",
                  static_cast<unsigned>(shown.hour % 100U),
                  static_cast<unsigned>(shown.minute % 100U));
    std::snprintf(model.seconds.data(), model.seconds.size(), "%02u",
                  static_cast<unsigned>(shown.second % 100U));
    std::snprintf(model.date.data(), model.date.size(), "%04u-%02u-%02u",
                  static_cast<unsigned>(shown.year % 10'000U),
                  static_cast<unsigned>(shown.month % 100U),
                  static_cast<unsigned>(shown.day % 100U));

    if (model.state == ClockState::live) {
        // Named, not decorative. GMT is not British Summer Time, so for half the year this
        // clock reads an hour behind every other clock in the country - correct, and
        // guaranteed to look broken. The label is what stops that being read as a fault.
        write_text(model.status.data(), model.status.size(), "GMT");
        model.status_rgb = color::text_secondary;
        return model;
    }

    std::int64_t age = 0;
    (void)reading_age_us(snapshot, age);
    write_hold_text(model.status.data(), model.status.size(), age);
    model.status_rgb = color::caution_bright;
    return model;
}

const char* clock_state_name(const ClockState state) noexcept
{
    switch (state) {
    case ClockState::no_receiver:
        return "no-receiver";
    case ClockState::waiting:
        return "waiting";
    case ClockState::live:
        return "live";
    case ClockState::holding:
        return "holding";
    case ClockState::lost:
        return "lost";
    }
    return "unknown";
}

}  // namespace track_timer::ui

#include "track_timer/ui/time_roller.hpp"

#include <cmath>
#include <cstdio>

namespace track_timer::ui {
namespace {

inline constexpr std::uint32_t kMaximumDurationSeconds = 24U * 60U * 60U;
inline constexpr std::uint32_t kMinimumSessionSeconds = 60U;

[[nodiscard]] std::int32_t wrapped(const std::int32_t value, const std::int32_t limit) noexcept
{
    const auto span = limit + 1;
    auto result = value % span;
    if (result < 0) {
        result += span;
    }
    return result;
}

}  // namespace

bool is_time_field(const SettingsField field) noexcept
{
    return field == SettingsField::session_duration ||
           field == SettingsField::rest_duration || field == SettingsField::average_lap;
}

TimeFieldSpec time_field_spec(const SettingsField field) noexcept
{
    switch (field) {
    case SettingsField::session_duration:
        return {1439, kMinimumSessionSeconds, kMaximumDurationSeconds};
    case SettingsField::rest_duration:
        return {1439, 0, kMaximumDurationSeconds};
    case SettingsField::average_lap:
        // A lap over an hour is not a thing, and zero means "not set", which is what the
        // laps-remaining estimate tests for.
        return {59, 0, 59 * 60 + 59};
    default:
        return {};
    }
}

std::uint32_t time_field_seconds(const SettingsField field,
                                 const settings::DeviceSettings& settings) noexcept
{
    switch (field) {
    case SettingsField::session_duration:
        return settings.session_duration_seconds;
    case SettingsField::rest_duration:
        return settings.rest_duration_seconds;
    case SettingsField::average_lap:
        return settings.average_lap_seconds;
    default:
        return 0;
    }
}

bool apply_time_field(const SettingsField field, const std::uint32_t seconds,
                      settings::DeviceSettings& draft) noexcept
{
    switch (field) {
    case SettingsField::session_duration:
        draft.session_duration_seconds = seconds;
        return true;
    case SettingsField::rest_duration:
        draft.rest_duration_seconds = seconds;
        return true;
    case SettingsField::average_lap:
        draft.average_lap_seconds = static_cast<std::uint16_t>(seconds);
        if (seconds == 0) {
            // Laps remaining cannot be computed without an average, so clearing the
            // average has to take the lower display back to elapsed with it.
            draft.lower_display = settings::LowerDisplayMode::elapsed;
        }
        return true;
    default:
        return false;
    }
}

void format_duration_value(char* const output, const std::size_t size,
                           const std::uint32_t seconds) noexcept
{
    if (output == nullptr || size == 0) {
        return;
    }
    const auto minutes = seconds / 60U;
    const auto remainder = seconds % 60U;
    if (remainder == 0U) {
        std::snprintf(output, size, "%u MIN", static_cast<unsigned>(minutes));
        return;
    }
    std::snprintf(output, size, "%u:%02u", static_cast<unsigned>(minutes),
                  static_cast<unsigned>(remainder));
}

void HoldToSave::begin(const std::uint32_t now_ms) noexcept
{
    started_ms_ = now_ms;
    travel_ = 0;
    active_ = true;
}

void HoldToSave::travel(const std::int32_t delta_pixels) noexcept
{
    if (!active_) {
        return;
    }
    travel_ += delta_pixels < 0 ? -delta_pixels : delta_pixels;
    if (travel_ > kRollerHoldTravelLimitPx) {
        active_ = false;
    }
}

void HoldToSave::end() noexcept
{
    active_ = false;
    travel_ = 0;
}

bool HoldToSave::active() const noexcept { return active_; }

float HoldToSave::progress(const std::uint32_t now_ms) const noexcept
{
    if (!active_ || now_ms <= started_ms_) {
        return 0.0F;
    }
    const auto held = now_ms - started_ms_;
    if (held >= kRollerHoldToSaveMs) {
        return 1.0F;
    }
    return static_cast<float>(held) / static_cast<float>(kRollerHoldToSaveMs);
}

bool HoldToSave::complete(const std::uint32_t now_ms) const noexcept
{
    return active_ && now_ms >= started_ms_ + kRollerHoldToSaveMs;
}

void TimeRoller::reset(const std::uint32_t total_seconds, const TimeFieldSpec spec) noexcept
{
    spec_ = spec;
    const auto clamped = total_seconds > spec.maximum_seconds ? spec.maximum_seconds
                                                              : total_seconds;
    minutes_ = static_cast<std::int32_t>(clamped / 60U);
    seconds_ = static_cast<std::int32_t>(clamped % 60U);
    if (minutes_ > static_cast<std::int32_t>(spec_.maximum_minutes)) {
        minutes_ = spec_.maximum_minutes;
    }
    active_ = RollerColumn::minutes;
    residue_ = 0.0F;
    velocity_ = 0.0F;
    coast_residue_ = 0.0F;
}

void TimeRoller::begin(const RollerColumn column) noexcept
{
    active_ = column;
    // Touching a spinning column stops it, rather than fighting the coast.
    velocity_ = 0.0F;
    coast_residue_ = 0.0F;
    residue_ = 0.0F;
}

void TimeRoller::drag(const float delta_pixels, const std::uint32_t elapsed_ms) noexcept
{
    // Down is positive and decreases the value: the column follows the finger.
    const auto steps = -delta_pixels / kRollerPixelsPerStep;
    residue_ += steps;
    const auto whole = static_cast<std::int32_t>(residue_);
    if (whole != 0) {
        apply_steps(active_, whole);
        residue_ -= static_cast<float>(whole);
    }
    if (elapsed_ms > 0) {
        const auto instant = steps * 1000.0F / static_cast<float>(elapsed_ms);
        // Smoothed, so one jittery sample cannot launch a flick on its own.
        velocity_ = velocity_ * 0.6F + instant * 0.4F;
    }
}

void TimeRoller::release() noexcept
{
    if (std::fabs(velocity_) < kRollerFlickMinimumStepsPerSecond) {
        velocity_ = 0.0F;
        coast_residue_ = 0.0F;
        return;
    }
    if (velocity_ > kRollerFlickCeilingStepsPerSecond) {
        velocity_ = kRollerFlickCeilingStepsPerSecond;
    }
    else if (velocity_ < -kRollerFlickCeilingStepsPerSecond) {
        velocity_ = -kRollerFlickCeilingStepsPerSecond;
    }
}

bool TimeRoller::advance(const std::uint32_t elapsed_ms) noexcept
{
    if (velocity_ == 0.0F || elapsed_ms == 0) {
        return velocity_ != 0.0F;
    }
    const auto elapsed_s = static_cast<float>(elapsed_ms) / 1000.0F;
    coast_residue_ += velocity_ * elapsed_s;
    const auto whole = static_cast<std::int32_t>(coast_residue_);
    if (whole != 0) {
        apply_steps(active_, whole);
        coast_residue_ -= static_cast<float>(whole);
    }
    velocity_ *= std::exp(-kRollerFlickDecayPerSecond * elapsed_s);
    if (std::fabs(velocity_) < 1.0F) {
        velocity_ = 0.0F;
        coast_residue_ = 0.0F;
        return false;
    }
    return true;
}

void TimeRoller::step(const RollerColumn column, const std::int32_t delta) noexcept
{
    active_ = column;
    velocity_ = 0.0F;
    coast_residue_ = 0.0F;
    apply_steps(column, delta);
}

void TimeRoller::apply_steps(const RollerColumn column, const std::int32_t steps) noexcept
{
    if (column == RollerColumn::minutes) {
        minutes_ = wrapped(minutes_ + steps, static_cast<std::int32_t>(spec_.maximum_minutes));
        return;
    }
    seconds_ = wrapped(seconds_ + steps, 59);
}

std::uint32_t TimeRoller::total_seconds() const noexcept
{
    return static_cast<std::uint32_t>(minutes_) * 60U + static_cast<std::uint32_t>(seconds_);
}

std::uint32_t TimeRoller::committed_seconds() const noexcept
{
    const auto total = total_seconds();
    if (total < spec_.minimum_seconds) {
        return spec_.minimum_seconds;
    }
    if (total > spec_.maximum_seconds) {
        return spec_.maximum_seconds;
    }
    return total;
}

std::uint16_t TimeRoller::minutes() const noexcept
{
    return static_cast<std::uint16_t>(minutes_);
}

std::uint8_t TimeRoller::seconds() const noexcept
{
    return static_cast<std::uint8_t>(seconds_);
}

RollerColumn TimeRoller::active() const noexcept { return active_; }

bool TimeRoller::coasting() const noexcept { return velocity_ != 0.0F; }

}  // namespace track_timer::ui

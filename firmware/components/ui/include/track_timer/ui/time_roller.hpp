#pragma once

#include "track_timer/settings/settings.hpp"
#include "track_timer/ui/settings_editor.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

enum class RollerColumn : std::uint8_t {
    minutes,
    seconds,
};

// What one field allows. Minutes are not capped at 59 for the durations: "minutes and
// seconds" describes the units, not a two-digit limit, and a 90 minute session reads
// naturally as 90:00. Capping at 59 would cut the validated 24 hour range to 59:59.
struct TimeFieldSpec {
    std::uint16_t maximum_minutes{59};
    std::uint32_t minimum_seconds{0};
    std::uint32_t maximum_seconds{59 * 60 + 59};
};

[[nodiscard]] bool is_time_field(SettingsField field) noexcept;
[[nodiscard]] TimeFieldSpec time_field_spec(SettingsField field) noexcept;
[[nodiscard]] std::uint32_t time_field_seconds(SettingsField field,
                                               const settings::DeviceSettings& settings) noexcept;
// Writes a committed value into `draft`. False if the field is not a time field.
[[nodiscard]] bool apply_time_field(SettingsField field, std::uint32_t seconds,
                                    settings::DeviceSettings& draft) noexcept;

// "20 MIN" for a whole number of minutes, "20:30" otherwise. Whole minutes stay in the
// wording the device has always used, because that is what most values still are.
void format_duration_value(char* output, std::size_t size, std::uint32_t seconds) noexcept;

// How far the finger travels to advance one step, and how a flick decays. Tuned against
// the 450 px panel: a full-height drag is about thirteen steps.
inline constexpr float kRollerPixelsPerStep = 34.0F;
// Below this a release is a stop, not a flick, so a slow drag ends where it is let go.
inline constexpr float kRollerFlickMinimumStepsPerSecond = 3.0F;
inline constexpr float kRollerFlickDecayPerSecond = 2.6F;
inline constexpr float kRollerFlickCeilingStepsPerSecond = 90.0F;

// LVGL reports a long press at 400 ms, which is far too eager for a save: a driver
// pausing mid-roll with a finger still down committed the half-rolled value. Saving is
// the one irreversible thing this screen does, so it asks for a deliberate hold.
inline constexpr std::uint32_t kRollerHoldToSaveMs = 1200;
// A hold that wanders is a drag that paused, not a hold. Generous enough for a gloved
// finger resting on a moving car.
inline constexpr std::int32_t kRollerHoldTravelLimitPx = 14;

// Tracks whether a touch has become a deliberate hold. Separate from TimeRoller because
// it is about the finger, not the value, and separate from the screen so it can be tested
// without LVGL.
class HoldToSave {
  public:
    void begin(std::uint32_t now_ms) noexcept;
    // Cumulative travel cancels the hold once the finger has clearly moved.
    void travel(std::int32_t delta_pixels) noexcept;
    void end() noexcept;

    [[nodiscard]] bool active() const noexcept;
    // 0 to 1, for showing the driver that the hold is being counted.
    [[nodiscard]] float progress(std::uint32_t now_ms) const noexcept;
    [[nodiscard]] bool complete(std::uint32_t now_ms) const noexcept;

  private:
    std::uint32_t started_ms_{0};
    std::int32_t travel_{0};
    bool active_{false};
};

// The interaction model behind the two-column selector, with no LVGL in it.
//
// Columns wrap within themselves and never carry: rolling seconds past 59 returns to 00
// and leaves minutes alone. A carry is unpredictable when the two columns are targeted
// independently, and a flick that rolls seconds several times round would otherwise drag
// minutes with it.
class TimeRoller {
  public:
    void reset(std::uint32_t total_seconds, TimeFieldSpec spec) noexcept;

    // A touch begins on a column. Any coast in progress stops, so a finger down catches
    // a spinning column rather than fighting it.
    void begin(RollerColumn column) noexcept;
    // Finger moved. Positive pixels are downward, which decreases the value: the column
    // follows the finger like a physical wheel, so pulling down brings lower values up.
    void drag(float delta_pixels, std::uint32_t elapsed_ms) noexcept;
    void release() noexcept;
    // Advances a flick. Returns true while still coasting, so the caller knows to redraw.
    bool advance(std::uint32_t elapsed_ms) noexcept;
    // One discrete step, for a swipe rather than a drag.
    void step(RollerColumn column, std::int32_t delta) noexcept;

    [[nodiscard]] std::uint32_t total_seconds() const noexcept;
    // Clamped into the field's validated range. A roller can sit below a field's floor -
    // session duration has a one minute minimum - so commit clamps rather than refusing.
    [[nodiscard]] std::uint32_t committed_seconds() const noexcept;
    [[nodiscard]] std::uint16_t minutes() const noexcept;
    [[nodiscard]] std::uint8_t seconds() const noexcept;
    [[nodiscard]] RollerColumn active() const noexcept;
    [[nodiscard]] bool coasting() const noexcept;

  private:
    void apply_steps(RollerColumn column, std::int32_t steps) noexcept;

    TimeFieldSpec spec_{};
    std::int32_t minutes_{0};
    std::int32_t seconds_{0};
    RollerColumn active_{RollerColumn::minutes};
    float residue_{0.0F};
    float velocity_{0.0F};
    float coast_residue_{0.0F};
};

static_assert(std::is_trivially_copyable_v<TimeFieldSpec>);

}  // namespace track_timer::ui

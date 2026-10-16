#include "track_timer/ui/compass.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cmath>
#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr const char* kNoValue = "---";
constexpr float kDegreesPerTurn = 360.0F;
constexpr std::size_t kCardinalCount = 16;
constexpr float kDegreesPerCardinal = kDegreesPerTurn / static_cast<float>(kCardinalCount);

constexpr const char* kCardinals[kCardinalCount] = {
    "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
    "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",
};

void write_text(char* const destination, const std::size_t capacity,
                const char* const text) noexcept
{
    std::snprintf(destination, capacity, "%s", text);
}

// Wrapped into 0..360 before anything else looks at it. A receiver should not report a
// bearing outside that, but a heading that arrives at -1 or 361 must become 359 or 1
// rather than a negative array index or a fourth digit in a three-digit field.
[[nodiscard]] float normalised(const float heading_deg) noexcept
{
    if (!std::isfinite(heading_deg)) {
        return 0.0F;
    }
    auto wrapped = std::fmod(heading_deg, kDegreesPerTurn);
    if (wrapped < 0.0F) {
        wrapped += kDegreesPerTurn;
    }
    return wrapped;
}

}  // namespace

const char* compass_cardinal(const float heading_deg) noexcept
{
    const auto wrapped = normalised(heading_deg);
    // Rounded, not truncated: each point is centred on its own bearing, so east runs from
    // 78.75 to 101.25 rather than from 90 to 112.5. Truncating would shift every label by
    // half a sector, which reads as correct right up until it is checked.
    const auto sector =
        static_cast<std::size_t>(std::lround(wrapped / kDegreesPerCardinal)) % kCardinalCount;
    return kCardinals[sector];
}

CompassState compass_state(const GpsOnlySnapshot& snapshot) noexcept
{
    const auto receiver = gps_only_state(snapshot);
    if (receiver == GpsOnlyState::no_receiver) {
        return CompassState::no_receiver;
    }
    if (receiver == GpsOnlyState::no_fix) {
        return CompassState::no_fix;
    }
    // Both gates, and the accuracy one first. headAcc is the receiver saying how well it
    // knows the course, which is the question being asked; the speed floor only catches a
    // receiver that stays optimistic while standing still.
    if (!std::isfinite(snapshot.fix.heading_accuracy_deg) ||
        snapshot.fix.heading_accuracy_deg > kMaximumHeadingAccuracyDeg ||
        snapshot.fix.heading_accuracy_deg <= 0.0F) {
        return CompassState::no_heading;
    }
    if (!(snapshot.fix.speed_mps >= kMinimumHeadingSpeedMps)) {
        return CompassState::no_heading;
    }
    return CompassState::live;
}

CompassViewModel compass_view(const GpsOnlySnapshot& snapshot) noexcept
{
    CompassViewModel model{};
    model.state = compass_state(snapshot);

    switch (model.state) {
    case CompassState::no_receiver:
        write_text(model.degrees.data(), model.degrees.size(), kNoValue);
        write_text(model.status.data(), model.status.size(), "NO RECEIVER");
        model.status_rgb = color::critical_bright;
        return model;
    case CompassState::no_fix:
        write_text(model.degrees.data(), model.degrees.size(), kNoValue);
        write_text(model.status.data(), model.status.size(), "NO FIX");
        model.status_rgb = color::caution_bright;
        return model;
    case CompassState::no_heading:
        write_text(model.degrees.data(), model.degrees.size(), kNoValue);
        // Named for the cause rather than the symptom. "NO FIX" would be wrong - the fix
        // is fine and the position is good - and leaving it blank would invite reading
        // the absent needle as a fault.
        write_text(model.status.data(), model.status.size(), "NO HEADING");
        model.status_rgb = color::caution_bright;
        return model;
    case CompassState::live:
        break;
    }

    model.heading_deg = normalised(snapshot.fix.heading_deg);
    model.heading_valid = true;
    // Zero-padded to three, always. A heading is the one readout here whose width never
    // changes, which is what spares this mode the repositioning Speed Only needs.
    std::snprintf(model.degrees.data(), model.degrees.size(), "%03u",
                  static_cast<unsigned>(std::lround(model.heading_deg)) % 360U);
    write_text(model.cardinal.data(), model.cardinal.size(),
               compass_cardinal(model.heading_deg));
    write_text(model.status.data(), model.status.size(), "TRUE");
    model.status_rgb = color::text_secondary;
    return model;
}

const char* compass_state_name(const CompassState state) noexcept
{
    switch (state) {
    case CompassState::no_receiver:
        return "no-receiver";
    case CompassState::no_fix:
        return "no-fix";
    case CompassState::no_heading:
        return "no-heading";
    case CompassState::live:
        return "live";
    }
    return "unknown";
}

}  // namespace track_timer::ui

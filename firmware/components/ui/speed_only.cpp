#include "track_timer/ui/speed_only.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr const char* kNoValue = "---";

// Three digits, because 100+ mph is not hypothetical on a straight and a lap timer that
// cannot render it is wrong exactly where it matters. Anything beyond that is a receiver
// fault rather than a speed, and is clamped rather than allowed to overrun the buffer -
// snprintf would truncate silently and leave a plausible two-digit number behind.
constexpr float kMaximumDisplayedMph = 999.0F;

void write_text(char* const destination, const std::size_t capacity,
                const char* const text) noexcept
{
    std::snprintf(destination, capacity, "%s", text);
}

}  // namespace

SpeedOnlyViewModel speed_only_view(const GpsOnlySnapshot& snapshot) noexcept
{
    SpeedOnlyViewModel model{};
    model.state = gps_only_state(snapshot);

    switch (model.state) {
    case GpsOnlyState::no_receiver:
        write_text(model.speed.data(), model.speed.size(), kNoValue);
        write_text(model.status.data(), model.status.size(), "NO RECEIVER");
        model.status_rgb = color::critical_bright;
        return model;
    case GpsOnlyState::no_fix:
        write_text(model.speed.data(), model.speed.size(), kNoValue);
        // A different word for a different problem: the one above is a cable, this one is
        // patience. Saying "no signal" for both would merge the two diagnoses.
        write_text(model.status.data(), model.status.size(), "NO FIX");
        model.status_rgb = color::caution_bright;
        return model;
    case GpsOnlyState::fixed:
        break;
    }

    auto mph = snapshot.fix.speed_mps * kMetresPerSecondToMph;
    if (!(mph >= 0.0F)) {
        // Also catches NaN, which compares false against everything and would otherwise
        // reach snprintf and print "nan" into a field sized for three digits.
        mph = 0.0F;
    }
    if (mph > kMaximumDisplayedMph) {
        mph = kMaximumDisplayedMph;
    }
    model.speed_valid = true;
    std::snprintf(model.speed.data(), model.speed.size(), "%.0f",
                  static_cast<double>(mph));
    write_text(model.status.data(), model.status.size(), "MPH");
    model.status_rgb = color::text_secondary;
    return model;
}

}  // namespace track_timer::ui

#include "track_timer/ui/gps_only.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr const char* kNoValue = "---";

template <std::size_t N>
void write(std::array<char, N>& target, const char* const text) noexcept
{
    std::snprintf(target.data(), target.size(), "%s", text);
}

template <std::size_t N, typename... Args>
void format(std::array<char, N>& target, const char* const pattern,
            Args... arguments) noexcept
{
    std::snprintf(target.data(), target.size(), pattern, arguments...);
}

}  // namespace

const char* fix_type_label(const domain::FixType type) noexcept
{
    switch (type) {
    case domain::FixType::no_fix:
        return "NO FIX";
    case domain::FixType::dead_reckoning:
        return "DR";
    case domain::FixType::fix_2d:
        return "2D";
    case domain::FixType::fix_3d:
        return "3D";
    case domain::FixType::gnss_dead_reckoning:
        return "3D + DR";
    case domain::FixType::time_only:
        return "TIME";
    }
    return "?";
}

const char* gps_only_state_name(const GpsOnlyState state) noexcept
{
    switch (state) {
    case GpsOnlyState::no_receiver:
        return "no-receiver";
    case GpsOnlyState::no_fix:
        return "no-fix";
    case GpsOnlyState::fixed:
        return "fixed";
    }
    return "unknown";
}

GpsOnlyState gps_only_state(const GpsOnlySnapshot& snapshot) noexcept
{
    if (!snapshot.receiver_present) {
        return GpsOnlyState::no_receiver;
    }
    return snapshot.fix_valid ? GpsOnlyState::fixed : GpsOnlyState::no_fix;
}

GpsOnlyViewModel gps_only_view(const GpsOnlySnapshot& snapshot) noexcept
{
    GpsOnlyViewModel view{};
    view.state = gps_only_state(snapshot);

    const bool present = view.state != GpsOnlyState::no_receiver;
    const bool fixed = view.state == GpsOnlyState::fixed;

    switch (view.state) {
    case GpsOnlyState::no_receiver:
        write(view.status, "NO RECEIVER");
        view.status_rgb = color::critical_bright;
        break;
    case GpsOnlyState::no_fix:
        // Not an error. A cold start outdoors can take minutes, and a screen that called
        // it a fault would send someone looking for a wiring problem that is not there.
        write(view.status, "SEARCHING");
        view.status_rgb = color::caution_bright;
        break;
    case GpsOnlyState::fixed:
        write(view.status, "FIX OK");
        view.status_rgb = color::positive_bright;
        break;
    }

    if (fixed) {
        format(view.speed, "%.0f",
               static_cast<double>(snapshot.fix.speed_mps * kMetresPerSecondToMph));
        format(view.speed_other, "%.1f m/s", static_cast<double>(snapshot.fix.speed_mps));
    }
    else {
        write(view.speed, kNoValue);
        write(view.speed_other, "--- m/s");
    }

    std::size_t index = 0;
    const auto row = [&view, &index](const char* const label) -> GpsOnlyRow& {
        auto& entry = view.rows[index];
        ++index;
        write(entry.label, label);
        write(entry.value, kNoValue);
        return entry;
    };

    // Fix type, satellite count, frame rate and dropped frames are all reported before a
    // fix exists, and watching the satellite count climb is how a cold start is told apart
    // from a receiver that is not talking at all. So these appear as soon as bytes arrive.
    auto& fix_type = row("FIX");
    if (present) {
        write(fix_type.value, fix_type_label(snapshot.fix.fix_type));
    }
    auto& satellites = row("SATS");
    if (present) {
        format(satellites.value, "%u", static_cast<unsigned>(snapshot.fix.num_satellites));
    }

    // Position, speed and accuracy mean nothing without a fix.
    auto& latitude = row("LAT");
    if (fixed) {
        format(latitude.value, "%.6f", snapshot.fix.latitude_deg);
    }
    auto& longitude = row("LON");
    if (fixed) {
        format(longitude.value, "%.6f", snapshot.fix.longitude_deg);
    }
    auto& altitude = row("ALT");
    if (fixed) {
        format(altitude.value, "%.0f m", static_cast<double>(snapshot.fix.height_m));
    }
    auto& heading = row("HDG");
    if (fixed) {
        format(heading.value, "%.0f", static_cast<double>(snapshot.fix.heading_deg));
    }
    auto& horizontal = row("H.ACC");
    if (fixed) {
        format(horizontal.value, "%.1f m",
               static_cast<double>(snapshot.fix.horizontal_accuracy_m));
    }
    auto& speed_accuracy = row("S.ACC");
    if (fixed) {
        format(speed_accuracy.value, "%.2f m/s",
               static_cast<double>(snapshot.fix.speed_accuracy_mps));
    }

    auto& rate = row("RATE");
    if (present) {
        format(rate.value, "%.1f Hz", static_cast<double>(snapshot.observed_rate_hz));
    }
    auto& gaps = row("GAPS");
    if (present) {
        format(gaps.value, "%u", static_cast<unsigned>(snapshot.dropped_fixes));
    }

    return view;
}

}  // namespace track_timer::ui

#include "track_timer/ui/presenter.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cstdio>

namespace track_timer::ui {
namespace {

template <std::size_t Size>
void format_lap_time(std::array<char, Size>& output, const std::int64_t duration_ms) noexcept
{
    if (duration_ms == domain::kUnavailableTime) {
        std::snprintf(output.data(), output.size(), "--:--.---");
        return;
    }

    const auto absolute_ms = duration_ms < 0
                                 ? static_cast<std::uint64_t>(-(duration_ms + 1)) + 1U
                                 : static_cast<std::uint64_t>(duration_ms);
    const auto minutes = absolute_ms / 60'000;
    const auto seconds = (absolute_ms / 1'000) % 60;
    const auto milliseconds = absolute_ms % 1'000;
    std::snprintf(output.data(), output.size(), "%s%llu:%02llu.%03llu",
                  duration_ms < 0 ? "+" : "", static_cast<unsigned long long>(minutes),
                  static_cast<unsigned long long>(seconds),
                  static_cast<unsigned long long>(milliseconds));
}

template <std::size_t Size>
void format_session_time(std::array<char, Size>& output, const std::int64_t duration_ms) noexcept
{
    if (duration_ms == domain::kUnavailableTime) {
        std::snprintf(output.data(), output.size(), "--:--");
        return;
    }

    const auto absolute_ms = duration_ms < 0
                                 ? static_cast<std::uint64_t>(-(duration_ms + 1)) + 1U
                                 : static_cast<std::uint64_t>(duration_ms);
    const auto minutes = absolute_ms / 60'000;
    const auto seconds = (absolute_ms / 1'000) % 60;
    std::snprintf(output.data(), output.size(), "%s%02llu:%02llu",
                  duration_ms < 0 ? "+" : "", static_cast<unsigned long long>(minutes),
                  static_cast<unsigned long long>(seconds));
}

const char* gnss_label(const domain::GnssHealth health) noexcept
{
    switch (health) {
    case domain::GnssHealth::unavailable:
        return "NO GPS";
    case domain::GnssHealth::searching:
        return "GPS SEARCH";
    case domain::GnssHealth::poor:
        return "GPS POOR";
    case domain::GnssHealth::good:
        return "GPS GOOD";
    case domain::GnssHealth::stale:
        return "GPS STALE";
    }
    return "NO GPS";
}

void set_session_accent(DeviceViewModel& model, const std::int64_t remaining_ms) noexcept
{
    if (remaining_ms == domain::kUnavailableTime) {
        model.accent_rgb = color::surface;
        std::snprintf(model.session_status.data(), model.session_status.size(), "SESSION");
    }
    else if (remaining_ms <= 0) {
        model.accent_rgb = color::overtime;
        std::snprintf(model.session_status.data(), model.session_status.size(), "OVERTIME");
    }
    else if (remaining_ms <= 5 * 60'000) {
        model.accent_rgb = color::critical;
        std::snprintf(model.session_status.data(), model.session_status.size(), "FINAL 5 MIN");
    }
    else if (remaining_ms <= 10 * 60'000) {
        model.accent_rgb = color::warning;
        std::snprintf(model.session_status.data(), model.session_status.size(), "UNDER 10 MIN");
    }
    else if (remaining_ms <= 20 * 60'000) {
        model.accent_rgb = color::caution;
        std::snprintf(model.session_status.data(), model.session_status.size(), "UNDER 20 MIN");
    }
    else {
        model.accent_rgb = color::positive;
        std::snprintf(model.session_status.data(), model.session_status.size(), "SESSION");
    }
    model.accent_text_rgb = contrast_text_rgb(model.accent_rgb);
}

}  // namespace

DeviceViewModel present(const domain::UiSnapshot& snapshot) noexcept
{
    DeviceViewModel model{};
    std::snprintf(model.lap_label.data(), model.lap_label.size(), "LAP %02u",
                  static_cast<unsigned>(snapshot.lap_index));
    format_lap_time(model.current_lap, snapshot.current_lap_ms);
    format_lap_time(model.previous_lap, snapshot.previous_lap_ms);
    format_lap_time(model.best_lap, snapshot.best_lap_ms);
    format_session_time(model.session_remaining, snapshot.session_remaining_ms);
    std::snprintf(model.gnss_status.data(), model.gnss_status.size(), "%s",
                  gnss_label(snapshot.gnss_health));
    std::snprintf(model.logging_status.data(), model.logging_status.size(), "%s",
                  snapshot.logging_available ? "LOGGING" : "NO LOG");
    model.gnss_health = snapshot.gnss_health;
    set_session_accent(model, snapshot.session_remaining_ms);
    return model;
}

}  // namespace track_timer::ui

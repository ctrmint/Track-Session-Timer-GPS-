#include "track_timer/ui/diagnostics.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cinttypes>
#include <cstdio>

namespace track_timer::ui {
namespace {

template <std::size_t Capacity>
void set_text(std::array<char, Capacity>& target, const char* text) noexcept
{
    std::snprintf(target.data(), target.size(), "%s", text);
}

const char* subsystem_name(const diagnostics::SubsystemState state) noexcept
{
    switch (state) {
    case diagnostics::SubsystemState::ready:
        return "READY";
    case diagnostics::SubsystemState::degraded:
        return "DEGRADED";
    case diagnostics::SubsystemState::unavailable:
        return "UNAVAILABLE";
    case diagnostics::SubsystemState::not_simulated:
        return "NOT SIMULATED";
    }
    return "UNAVAILABLE";
}

std::uint32_t subsystem_color(const diagnostics::SubsystemState state) noexcept
{
    switch (state) {
    case diagnostics::SubsystemState::ready:
        return color::positive_bright;
    case diagnostics::SubsystemState::degraded:
        return color::caution_bright;
    case diagnostics::SubsystemState::unavailable:
        return color::critical_bright;
    case diagnostics::SubsystemState::not_simulated:
        return color::text_secondary;
    }
    return color::critical_bright;
}

const char* reset_reason_name(const diagnostics::ResetReason reason) noexcept
{
    switch (reason) {
    case diagnostics::ResetReason::unknown:
        return "UNKNOWN";
    case diagnostics::ResetReason::power_on:
        return "POWER ON";
    case diagnostics::ResetReason::software:
        return "SOFTWARE";
    case diagnostics::ResetReason::watchdog:
        return "WATCHDOG";
    case diagnostics::ResetReason::brownout:
        return "BROWNOUT";
    case diagnostics::ResetReason::panic:
        return "PANIC";
    }
    return "UNKNOWN";
}

const char* backend_name(const diagnostics::BackendKind backend) noexcept
{
    switch (backend) {
    case diagnostics::BackendKind::hardware:
        return "HARDWARE";
    case diagnostics::BackendKind::simulator:
        return "SIMULATOR";
    case diagnostics::BackendKind::unavailable:
        return "UNAVAILABLE";
    }
    return "UNAVAILABLE";
}

const char* fix_type_name(const domain::FixType type) noexcept
{
    switch (type) {
    case domain::FixType::no_fix:
        return "NO FIX";
    case domain::FixType::dead_reckoning:
        return "DEAD RECKONING";
    case domain::FixType::fix_2d:
        return "2D FIX";
    case domain::FixType::fix_3d:
        return "3D FIX";
    case domain::FixType::gnss_dead_reckoning:
        return "GNSS + DR";
    case domain::FixType::time_only:
        return "TIME ONLY";
    }
    return "NO FIX";
}

void set_row(DiagnosticsRow& row, const char* label, const char* value,
             const std::uint32_t color_rgb = color::text_primary) noexcept
{
    set_text(row.label, label);
    set_text(row.value, value);
    row.color_rgb = color_rgb;
}

void set_state_row(DiagnosticsRow& row, const char* label,
                   const diagnostics::SubsystemState state) noexcept
{
    set_row(row, label, subsystem_name(state), subsystem_color(state));
}

void format_bytes(char* target, const std::size_t capacity,
                  const diagnostics::MemorySnapshot& memory) noexcept
{
    if (memory.state != diagnostics::SubsystemState::ready &&
        memory.state != diagnostics::SubsystemState::degraded) {
        std::snprintf(target, capacity, "%s", subsystem_name(memory.state));
        return;
    }
    std::snprintf(target, capacity, "%.1f / %.1f MB",
                  static_cast<double>(memory.free_bytes) / 1'048'576.0,
                  static_cast<double>(memory.total_bytes) / 1'048'576.0);
}

void format_queue(char* target, const std::size_t capacity,
                  const diagnostics::QueueSnapshot& queue) noexcept
{
    std::snprintf(target, capacity, "%zu / %zu", queue.depth, queue.capacity);
}

}  // namespace

void DiagnosticsController::begin(const diagnostics::DiagnosticsSnapshot& snapshot) noexcept
{
    snapshot_ = snapshot;
    view_ = {};
    view_.current_page = DiagnosticsPage::system;
    open_ = true;
    refresh();
}

void DiagnosticsController::update(const diagnostics::DiagnosticsSnapshot& snapshot) noexcept
{
    snapshot_ = snapshot;
    if (open_) {
        refresh();
    }
}

void DiagnosticsController::close() noexcept
{
    open_ = false;
    view_ = {};
}

void DiagnosticsController::previous_page() noexcept
{
    const auto page = static_cast<std::size_t>(view_.current_page);
    if (open_ && page > 0) {
        view_.current_page = static_cast<DiagnosticsPage>(page - 1);
        refresh();
    }
}

void DiagnosticsController::next_page() noexcept
{
    const auto page = static_cast<std::size_t>(view_.current_page);
    if (open_ && page + 1 < kDiagnosticsPageCount) {
        view_.current_page = static_cast<DiagnosticsPage>(page + 1);
        refresh();
    }
}

const DiagnosticsViewModel& DiagnosticsController::view_model() const noexcept
{
    return view_;
}

void DiagnosticsController::refresh() noexcept
{
    for (auto& row : view_.rows) {
        row = {};
    }
    set_text(view_.title, "SYSTEM DIAGNOSTICS");
    switch (snapshot_.overall) {
    case diagnostics::OverallState::normal:
        set_text(view_.status, "ALL MONITORED SYSTEMS NORMAL");
        view_.status_color_rgb = color::positive_bright;
        break;
    case diagnostics::OverallState::degraded:
        set_text(view_.status, "DEGRADED - CHECK CAUTION VALUES");
        view_.status_color_rgb = color::caution_bright;
        break;
    case diagnostics::OverallState::missing:
        set_text(view_.status, "MISSING HARDWARE - TIMER STILL AVAILABLE");
        view_.status_color_rgb = color::critical_bright;
        break;
    case diagnostics::OverallState::recovered:
        set_text(view_.status, "RECOVERED - COUNTERS RETAIN FAULT HISTORY");
        view_.status_color_rgb = color::positive_bright;
        break;
    }

    switch (view_.current_page) {
    case DiagnosticsPage::system:
        build_system_page();
        break;
    case DiagnosticsPage::gnss:
        build_gnss_page();
        break;
    case DiagnosticsPage::logging:
        build_logging_page();
        break;
    case DiagnosticsPage::peripherals:
        build_peripherals_page();
        break;
    }
    const auto page = static_cast<std::size_t>(view_.current_page);
    view_.previous_enabled = page > 0;
    view_.next_enabled = page + 1 < kDiagnosticsPageCount;
    std::snprintf(view_.page.data(), view_.page.size(), "%s  %zu/%zu",
                  diagnostics_page_name(view_.current_page), page + 1,
                  kDiagnosticsPageCount);
}

void DiagnosticsController::build_system_page() noexcept
{
    char value[48]{};
    set_row(view_.rows[0], "FIRMWARE",
            snapshot_.firmware_version[0] == '\0' ? "UNAVAILABLE"
                                                    : snapshot_.firmware_version.data());
    const auto total_seconds = snapshot_.uptime_ms / 1'000;
    std::snprintf(value, sizeof(value), "%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64,
                  total_seconds / 3'600, (total_seconds / 60) % 60,
                  total_seconds % 60);
    set_row(view_.rows[1], "UPTIME", value);
    set_row(view_.rows[2], "RESET REASON", reset_reason_name(snapshot_.reset_reason));
    format_bytes(value, sizeof(value), snapshot_.internal_ram);
    set_row(view_.rows[3], "INTERNAL FREE/TOTAL", value,
            subsystem_color(snapshot_.internal_ram.state));
    format_bytes(value, sizeof(value), snapshot_.psram);
    set_row(view_.rows[4], "PSRAM FREE/TOTAL", value,
            subsystem_color(snapshot_.psram.state));
    set_row(view_.rows[5], "BACKEND", backend_name(snapshot_.backend),
            snapshot_.backend == diagnostics::BackendKind::unavailable
                ? color::critical_bright
                : color::positive_bright);
    std::snprintf(value, sizeof(value), "%" PRIu32 " / %" PRIu32 " us",
                  snapshot_.display_frame_count, snapshot_.display_average_update_us);
    set_row(view_.rows[6], "DISPLAY FRAMES/AVG", value);
    std::snprintf(value, sizeof(value), "%" PRIu32 " us / %zu B",
                  snapshot_.display_maximum_update_us, snapshot_.display_maximum_lvgl_bytes);
    set_row(view_.rows[7], "DISPLAY MAX/MEM", value);
}

void DiagnosticsController::build_gnss_page() noexcept
{
    char value[48]{};
    set_state_row(view_.rows[0], "GNSS STATE", snapshot_.gnss);
    if (snapshot_.gnss == diagnostics::SubsystemState::unavailable) {
        set_row(view_.rows[1], "UPDATE RATE", "UNAVAILABLE", color::critical_bright);
        set_row(view_.rows[2], "FIX TYPE", "NO FIX", color::critical_bright);
        set_row(view_.rows[3], "SATELLITES", "UNAVAILABLE", color::critical_bright);
        set_row(view_.rows[4], "H ACCURACY", "UNAVAILABLE", color::critical_bright);
    }
    else {
        std::snprintf(value, sizeof(value), "%u Hz",
                      static_cast<unsigned>(snapshot_.gnss_rate_hz));
        set_row(view_.rows[1], "UPDATE RATE", value);
        set_row(view_.rows[2], "FIX TYPE", fix_type_name(snapshot_.gnss_fix_type));
        std::snprintf(value, sizeof(value), "%u",
                      static_cast<unsigned>(snapshot_.gnss_satellites));
        set_row(view_.rows[3], "SATELLITES", value);
        std::snprintf(value, sizeof(value), "%.1f m",
                      static_cast<double>(snapshot_.gnss_horizontal_accuracy_m));
        set_row(view_.rows[4], "H ACCURACY", value);
    }
    format_queue(value, sizeof(value), snapshot_.gnss_queue);
    set_row(view_.rows[5], "QUEUE DEPTH/CAP", value);
    std::snprintf(value, sizeof(value), "%zu", snapshot_.gnss_queue.high_water_mark);
    set_row(view_.rows[6], "QUEUE HIGH WATER", value);
    std::snprintf(value, sizeof(value), "%" PRIu64 " / %" PRIu32,
                  snapshot_.gnss_queue.dropped, snapshot_.gnss_recoveries);
    set_row(view_.rows[7], "DROPS / RECOVERY", value,
            snapshot_.gnss_queue.dropped == 0 ? color::text_primary : color::caution_bright);
}

void DiagnosticsController::build_logging_page() noexcept
{
    char value[48]{};
    set_state_row(view_.rows[0], "SD STATE", snapshot_.storage);
    if (snapshot_.storage == diagnostics::SubsystemState::unavailable) {
        set_row(view_.rows[1], "SD AVAILABLE", "UNAVAILABLE", color::critical_bright);
    }
    else {
        std::snprintf(value, sizeof(value), "%.1f MB",
                      static_cast<double>(snapshot_.storage_available_bytes) / 1'048'576.0);
        set_row(view_.rows[1], "SD AVAILABLE", value);
    }
    format_queue(value, sizeof(value), snapshot_.logger_queue);
    set_row(view_.rows[2], "LOGGER DEPTH/CAP", value);
    std::snprintf(value, sizeof(value), "%zu", snapshot_.logger_queue.high_water_mark);
    set_row(view_.rows[3], "LOGGER HIGH WATER", value);
    std::snprintf(value, sizeof(value), "%" PRIu64, snapshot_.logger_queue.dropped);
    set_row(view_.rows[4], "LOGGER DROPS", value,
            snapshot_.logger_queue.dropped == 0 ? color::text_primary : color::critical_bright);
    std::snprintf(value, sizeof(value), "%" PRIu64, snapshot_.logger_write_failures);
    set_row(view_.rows[5], "WRITE FAILURES", value,
            snapshot_.logger_write_failures == 0 ? color::text_primary
                                                 : color::critical_bright);
    std::snprintf(value, sizeof(value), "%" PRIu32, snapshot_.storage_recoveries);
    set_row(view_.rows[6], "RECOVERIES", value,
            snapshot_.storage_recoveries == 0 ? color::text_primary : color::positive_bright);
    std::snprintf(value, sizeof(value), "%" PRId64 " us", snapshot_.logger_p95_latency_us);
    set_row(view_.rows[7], "LOGGER P95", value);
}

void DiagnosticsController::build_peripherals_page() noexcept
{
    char value[48]{};
    set_state_row(view_.rows[0], "IMU STATE", snapshot_.imu);
    format_queue(value, sizeof(value), snapshot_.imu_queue);
    set_row(view_.rows[1], "IMU QUEUE", value);
    std::snprintf(value, sizeof(value), "%" PRIu64, snapshot_.imu_samples);
    set_row(view_.rows[2], "IMU SAMPLES", value);
    set_state_row(view_.rows[3], "RTC STATE", snapshot_.rtc);
    set_state_row(view_.rows[4], "TOUCH STATE", snapshot_.touch);
    format_queue(value, sizeof(value), snapshot_.touch_queue);
    set_row(view_.rows[5], "TOUCH QUEUE", value);
    set_state_row(view_.rows[6], "DISPLAY STATE", snapshot_.display);
    set_row(view_.rows[7], "BACKEND", backend_name(snapshot_.backend),
            snapshot_.backend == diagnostics::BackendKind::unavailable
                ? color::critical_bright
                : color::positive_bright);
}

const char* diagnostics_page_name(const DiagnosticsPage page) noexcept
{
    switch (page) {
    case DiagnosticsPage::system:
        return "SYSTEM";
    case DiagnosticsPage::gnss:
        return "GNSS";
    case DiagnosticsPage::logging:
        return "LOGGING";
    case DiagnosticsPage::peripherals:
        return "PERIPHERALS";
    }
    return "SYSTEM";
}

const char* diagnostics_overall_name(const diagnostics::OverallState state) noexcept
{
    switch (state) {
    case diagnostics::OverallState::normal:
        return "normal";
    case diagnostics::OverallState::degraded:
        return "degraded";
    case diagnostics::OverallState::missing:
        return "missing";
    case diagnostics::OverallState::recovered:
        return "recovered";
    }
    return "degraded";
}

}  // namespace track_timer::ui

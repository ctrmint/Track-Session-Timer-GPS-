#include "track_timer/ui/session_review.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

template <std::size_t Capacity>
void set_text(std::array<char, Capacity>& target, const char* text) noexcept
{
    std::snprintf(target.data(), target.size(), "%s", text);
}

template <std::size_t Capacity>
void format_duration_ms(std::array<char, Capacity>& target, const std::int64_t value_ms,
                        const bool signed_value = false) noexcept
{
    const auto safe_ms = std::max<std::int64_t>(0, value_ms);
    const auto total_seconds = safe_ms / 1'000;
    const auto hours = total_seconds / 3'600;
    const auto minutes = (total_seconds / 60) % 60;
    const auto seconds = total_seconds % 60;
    const char* sign = signed_value && safe_ms > 0 ? "+" : "";
    if (hours > 0) {
        std::snprintf(target.data(), target.size(), "%s%lld:%02lld:%02lld", sign,
                      static_cast<long long>(hours), static_cast<long long>(minutes),
                      static_cast<long long>(seconds));
    }
    else {
        std::snprintf(target.data(), target.size(), "%s%02lld:%02lld", sign,
                      static_cast<long long>(minutes), static_cast<long long>(seconds));
    }
}

template <std::size_t Capacity>
void format_lap_ns(std::array<char, Capacity>& target, const std::int64_t value_ns) noexcept
{
    const auto total_ms = std::max<std::int64_t>(0, value_ns / 1'000'000);
    const auto minutes = total_ms / 60'000;
    const auto seconds = (total_ms / 1'000) % 60;
    const auto milliseconds = total_ms % 1'000;
    std::snprintf(target.data(), target.size(), "%lld:%02lld.%03lld",
                  static_cast<long long>(minutes), static_cast<long long>(seconds),
                  static_cast<long long>(milliseconds));
}

const char* completion_name(const logger::SessionCompletionReason reason) noexcept
{
    switch (reason) {
    case logger::SessionCompletionReason::driver_stop:
        return "DRIVER STOP";
    case logger::SessionCompletionReason::reset_recovery:
        return "RESET RECOVERY";
    case logger::SessionCompletionReason::pit_entry:
        return "PIT ENTRY";
    case logger::SessionCompletionReason::none:
        return "INCOMPLETE";
    }
    return "UNKNOWN";
}

void append_degraded(char* target, const std::size_t capacity, const char* name) noexcept
{
    const auto used = std::strlen(target);
    if (used >= capacity) {
        return;
    }
    std::snprintf(target + used, capacity - used, "%s%s", used > 10 ? " / " : "", name);
}

}  // namespace

void SessionReviewController::begin(logger::SessionSummaryProvider* provider) noexcept
{
    provider_ = provider;
    summary_ = {};
    page_ = {};
    view_ = {};
    if (provider_ == nullptr) {
        set_failure(logger::SummaryReadResult::storage_unavailable);
        return;
    }

    std::size_t count = 0;
    const auto result = provider_->session_count(count);
    if (result == logger::SummaryReadResult::empty ||
        (result == logger::SummaryReadResult::ready && count == 0)) {
        view_.status = SessionReviewStatus::empty;
        update_view();
        return;
    }
    if (result != logger::SummaryReadResult::ready) {
        set_failure(result);
        return;
    }
    view_.session_count = count;
    view_.history_index = 0;
    load_session();
}

void SessionReviewController::close() noexcept
{
    provider_ = nullptr;
    summary_ = {};
    page_ = {};
    view_ = {};
}

void SessionReviewController::newer_session() noexcept
{
    if (provider_ == nullptr || view_.history_index == 0) {
        return;
    }
    --view_.history_index;
    load_session();
}

void SessionReviewController::older_session() noexcept
{
    if (provider_ == nullptr || view_.history_index + 1 >= view_.session_count) {
        return;
    }
    ++view_.history_index;
    load_session();
}

void SessionReviewController::previous_lap_page() noexcept
{
    if (provider_ == nullptr || page_.offset == 0) {
        return;
    }
    const auto offset = page_.offset > logger::kSummaryLapPageCapacity
                            ? page_.offset - logger::kSummaryLapPageCapacity
                            : 0;
    load_laps(offset);
}

void SessionReviewController::next_lap_page() noexcept
{
    if (provider_ == nullptr || page_.offset + page_.count >= page_.total_count) {
        return;
    }
    load_laps(page_.offset + page_.count);
}

const SessionReviewViewModel& SessionReviewController::view_model() const noexcept
{
    return view_;
}

void SessionReviewController::set_failure(const logger::SummaryReadResult result) noexcept
{
    switch (result) {
    case logger::SummaryReadResult::storage_unavailable:
        view_.status = SessionReviewStatus::storage_unavailable;
        break;
    case logger::SummaryReadResult::unsupported_version:
        view_.status = SessionReviewStatus::unsupported_version;
        break;
    case logger::SummaryReadResult::corrupt:
        view_.status = SessionReviewStatus::corrupt;
        break;
    case logger::SummaryReadResult::empty:
        view_.status = SessionReviewStatus::empty;
        break;
    case logger::SummaryReadResult::ready:
        view_.status = SessionReviewStatus::corrupt;
        break;
    }
    update_view();
}

void SessionReviewController::load_session() noexcept
{
    summary_ = {};
    page_ = {};
    const auto result = provider_->read_summary(view_.history_index, summary_);
    if (result != logger::SummaryReadResult::ready) {
        set_failure(result);
        return;
    }
    if (!logger::valid_summary(summary_)) {
        view_.status = summary_.schema_version == logger::kLogFormatVersion
                           ? SessionReviewStatus::corrupt
                           : SessionReviewStatus::unsupported_version;
        update_view();
        return;
    }
    view_.status = summary_.integrity == logger::SummaryIntegrity::partial_log
                       ? SessionReviewStatus::partial_log
                       : SessionReviewStatus::ready;
    load_laps(0);
}

void SessionReviewController::load_laps(const std::size_t offset) noexcept
{
    logger::SummaryLapPage candidate{};
    const auto result = provider_->read_lap_page(summary_.session_id, offset, candidate);
    if (result == logger::SummaryReadResult::empty && summary_.lap_count == 0) {
        candidate.offset = 0;
        candidate.total_count = 0;
    }
    else if (result != logger::SummaryReadResult::ready) {
        set_failure(result);
        return;
    }

    const auto remaining = offset <= summary_.lap_count ? summary_.lap_count - offset : 0;
    if (candidate.offset != offset || candidate.total_count != summary_.lap_count ||
        candidate.count > logger::kSummaryLapPageCapacity || candidate.count > remaining ||
        (summary_.lap_count > 0 && candidate.count == 0)) {
        view_.status = SessionReviewStatus::corrupt;
        update_view();
        return;
    }
    for (std::size_t index = 0; index < candidate.count; ++index) {
        if (!logger::valid_summary_lap(candidate.laps[index])) {
            view_.status = candidate.laps[index].schema_version == logger::kLogFormatVersion
                               ? SessionReviewStatus::corrupt
                               : SessionReviewStatus::unsupported_version;
            update_view();
            return;
        }
    }
    page_ = candidate;
    update_view();
}

void SessionReviewController::update_view() noexcept
{
    view_.title.fill('\0');
    view_.duration.fill('\0');
    view_.overrun.fill('\0');
    view_.completion.fill('\0');
    view_.integrity.fill('\0');
    view_.message.fill('\0');
    view_.peak_total.fill('\0');
    view_.peak_longitudinal.fill('\0');
    view_.peak_lateral.fill('\0');
    view_.peak_vertical.fill('\0');
    for (auto& row : view_.laps) {
        row = {};
    }
    view_.newer_session_enabled = false;
    view_.older_session_enabled = false;
    view_.previous_page_enabled = false;
    view_.next_page_enabled = false;
    view_.lap_offset = 0;
    view_.lap_count = 0;

    switch (view_.status) {
    case SessionReviewStatus::closed:
        set_text(view_.title, "REVIEW");
        set_text(view_.message, "Review is closed");
        return;
    case SessionReviewStatus::empty:
        set_text(view_.title, "SESSION REVIEW");
        set_text(view_.message, "NO COMPLETED SESSIONS");
        set_text(view_.integrity, "Complete a session to create a review summary");
        return;
    case SessionReviewStatus::storage_unavailable:
        set_text(view_.title, "SESSION REVIEW");
        set_text(view_.message, "STORAGE UNAVAILABLE");
        set_text(view_.integrity, "Timer remains available; history cannot be read");
        return;
    case SessionReviewStatus::corrupt:
        set_text(view_.title, "SESSION REVIEW");
        set_text(view_.message, "SUMMARY DATA CORRUPT");
        set_text(view_.integrity, "Source logs are preserved for host recovery");
        return;
    case SessionReviewStatus::unsupported_version:
        set_text(view_.title, "SESSION REVIEW");
        set_text(view_.message, "SUMMARY VERSION UNSUPPORTED");
        set_text(view_.integrity, "Update firmware or review the logs on a host");
        return;
    case SessionReviewStatus::ready:
    case SessionReviewStatus::partial_log:
        break;
    }

    std::snprintf(view_.title.data(), view_.title.size(), "SESSION %zu OF %zu",
                  view_.history_index + 1, view_.session_count);
    std::array<char, 24> formatted{};
    format_duration_ms(formatted, summary_.session_duration_ms);
    std::snprintf(view_.duration.data(), view_.duration.size(), "DURATION %s",
                  formatted.data());
    format_duration_ms(formatted, summary_.session_overrun_ms, true);
    std::snprintf(view_.overrun.data(), view_.overrun.size(), "OVERRUN %s",
                  formatted.data());
    std::snprintf(view_.completion.data(), view_.completion.size(), "ENDED %s",
                  completion_name(summary_.completion_reason));

    // Two decimals, because the difference between 0.94 and 1.02 g is the difference
    // between a good corner and a very good one.
    const auto& peaks = summary_.peaks;
    std::snprintf(view_.peak_total.data(), view_.peak_total.size(), "%.2f",
                  static_cast<double>(peaks.total_g));
    std::snprintf(view_.peak_longitudinal.data(), view_.peak_longitudinal.size(),
                  "ACC %.2f   BRK %.2f", static_cast<double>(peaks.acceleration_g),
                  static_cast<double>(peaks.braking_g));
    std::snprintf(view_.peak_lateral.data(), view_.peak_lateral.size(),
                  "LEFT %.2f   RIGHT %.2f", static_cast<double>(peaks.left_g),
                  static_cast<double>(peaks.right_g));
    // Up and down kept apart: a kerb and a compression are different events.
    std::snprintf(view_.peak_vertical.data(), view_.peak_vertical.size(),
                  "UP %.2f   DOWN %.2f", static_cast<double>(peaks.up_g),
                  static_cast<double>(peaks.down_g));

    if (view_.status == SessionReviewStatus::partial_log) {
        set_text(view_.integrity, "PARTIAL LOG - RESULTS MAY BE INCOMPLETE");
    }
    else if (summary_.degraded_subsystems == logger::degraded_none) {
        set_text(view_.integrity, "COMPLETE LOG / SYSTEMS OK");
    }
    else {
        set_text(view_.integrity, "DEGRADED: ");
        if ((summary_.degraded_subsystems & logger::degraded_gnss) != 0) {
            append_degraded(view_.integrity.data(), view_.integrity.size(), "GPS");
        }
        if ((summary_.degraded_subsystems & logger::degraded_storage) != 0) {
            append_degraded(view_.integrity.data(), view_.integrity.size(), "STORAGE");
        }
        if ((summary_.degraded_subsystems & logger::degraded_imu) != 0) {
            append_degraded(view_.integrity.data(), view_.integrity.size(), "IMU");
        }
        if ((summary_.degraded_subsystems & logger::degraded_touch) != 0) {
            append_degraded(view_.integrity.data(), view_.integrity.size(), "TOUCH");
        }
        if ((summary_.degraded_subsystems & logger::degraded_rtc) != 0) {
            append_degraded(view_.integrity.data(), view_.integrity.size(), "RTC");
        }
    }

    if (summary_.lap_count == 0) {
        set_text(view_.message, "NO VALID LAPS RECORDED");
    }
    else {
        std::snprintf(view_.message.data(), view_.message.size(),
                      "LAPS %zu-%zu OF %" PRIu32,
                      page_.offset + 1, page_.offset + page_.count, summary_.lap_count);
    }
    for (std::size_t index = 0; index < page_.count; ++index) {
        auto& row = view_.laps[index];
        const auto& lap = page_.laps[index];
        row.visible = true;
        row.best = lap.lap_index == summary_.best_lap_index;
        row.previous = lap.lap_index == summary_.lap_count;
        std::snprintf(row.lap.data(), row.lap.size(), "LAP %" PRIu32, lap.lap_index);
        format_lap_ns(row.duration, lap.lap_duration_ns);
        if (row.best && row.previous) {
            set_text(row.emphasis, "BEST / PREVIOUS");
        }
        else if (row.best) {
            set_text(row.emphasis, "BEST");
        }
        else if (row.previous) {
            set_text(row.emphasis, "PREVIOUS");
        }
    }

    view_.newer_session_enabled = view_.history_index > 0;
    view_.older_session_enabled = view_.history_index + 1 < view_.session_count;
    view_.previous_page_enabled = page_.offset > 0;
    view_.next_page_enabled = page_.offset + page_.count < page_.total_count;
    view_.lap_offset = page_.offset;
    view_.lap_count = page_.total_count;
}

const char* session_review_status_name(const SessionReviewStatus status) noexcept
{
    switch (status) {
    case SessionReviewStatus::closed:
        return "closed";
    case SessionReviewStatus::ready:
        return "ready";
    case SessionReviewStatus::partial_log:
        return "partial-log";
    case SessionReviewStatus::empty:
        return "empty";
    case SessionReviewStatus::storage_unavailable:
        return "storage-unavailable";
    case SessionReviewStatus::corrupt:
        return "corrupt";
    case SessionReviewStatus::unsupported_version:
        return "unsupported-version";
    }
    return "corrupt";
}

}  // namespace track_timer::ui

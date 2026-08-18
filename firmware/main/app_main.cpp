#include "esp_log.h"

#include <cstddef>
#include "screen_router.hpp"
#include "track_timer/display/panel.hpp"
#include "track_timer/domain/contracts.hpp"
#include "track_timer/storage/sd_card.hpp"

namespace {
constexpr const char* kTag = "track_timer";
}

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "TrackSessionTimer GPS bootstrap");
    ESP_LOGI(kTag, "GNSS fix queue capacity: %u",
             static_cast<unsigned>(track_timer::domain::queue_capacity::gnss_fixes));

    const auto panel = track_timer::display::start_panel();
    ESP_LOGI(kTag, "panel: %s", track_timer::display::panel_result_name(panel));
    if (panel != track_timer::display::PanelResult::ready) {
        ESP_LOGE(kTag, "display unavailable; nothing can be rendered");
        return;
    }

    const auto touch = track_timer::display::start_touch();
    ESP_LOGI(kTag, "touch: %s", track_timer::display::touch_result_name(touch));

    // Inspection only: format_if_unreadable stays false so that simply booting can
    // never destroy the contents of a card.
    namespace storage = track_timer::storage;
#ifdef TRACK_TIMER_FORMAT_SD_ONCE
    // In the opt-in format build only, let the mount lay down a fresh filesystem when
    // the card cannot be read; otherwise an unreadable card could never be recovered.
    constexpr bool kFormatOnMountFailure = true;
#else
    constexpr bool kFormatOnMountFailure = false;
#endif
    const auto mount = storage::mount(kFormatOnMountFailure);
    ESP_LOGI(kTag, "sd-card: %s", storage::mount_result_name(mount));
    if (mount == storage::MountResult::mounted) {
        const auto card = storage::info();
        ESP_LOGI(kTag, "sd-card name=%s capacity=%llu MB fs-total=%llu MB free=%llu MB",
                 card.name,
                 static_cast<unsigned long long>(card.capacity_bytes / (1024 * 1024)),
                 static_cast<unsigned long long>(card.total_bytes / (1024 * 1024)),
                 static_cast<unsigned long long>(card.free_bytes / (1024 * 1024)));
        static char names[32][64];
        const auto count = storage::list_entries(names, 32);
        ESP_LOGI(kTag, "sd-card top-level entries: %u", static_cast<unsigned>(count));
        for (std::size_t index = 0; index < count; ++index) {
            ESP_LOGI(kTag, "  [%u] %s", static_cast<unsigned>(index), names[index]);
        }
        if (count == 0) {
            ESP_LOGI(kTag, "  (card is empty)");
        }
        storage::MediaSurvey survey{};
        if (storage::survey_media(survey)) {
            ESP_LOGW(kTag, "sd-card CONTENTS: %u file(s), %llu MB total",
                     static_cast<unsigned>(survey.file_count),
                     static_cast<unsigned long long>(survey.total_bytes / (1024 * 1024)));
            for (std::size_t index = 0; index < survey.sample_count; ++index) {
                ESP_LOGW(kTag, "  sample: %s", survey.samples[index]);
            }
        }
    }

#ifdef TRACK_TIMER_FORMAT_SD_ONCE
    // Deliberately unreachable in a normal build. Enabled only by setting
    // TRACK_TIMER_FORMAT_SD=1 in the build environment, so that ordinary firmware can
    // never destroy the contents of an inserted card.
    if (mount == storage::MountResult::mounted ||
        mount == storage::MountResult::unreadable_filesystem) {
        ESP_LOGW(kTag, "FORMATTING the microSD card now");
        const auto formatted = storage::format();
        ESP_LOGW(kTag, "sd-card format: %s", storage::format_result_name(formatted));
        if (formatted == storage::FormatResult::formatted) {
            const auto fresh = storage::info();
            storage::MediaSurvey after{};
            (void)storage::survey_media(after);
            ESP_LOGW(kTag, "after format: %u file(s), %u top-level entr(ies)",
                     static_cast<unsigned>(after.file_count),
                     static_cast<unsigned>(fresh.entry_count));
        }
    }
#endif

    if (!track_timer::main_app::start_screen_router()) {
        ESP_LOGE(kTag, "screen router failed to build");
        return;
    }

    ESP_LOGI(kTag, "ready screen presented on the 600x450 panel");
    ESP_LOGI(kTag, "GNSS, storage and IMU drivers are not implemented yet");
}

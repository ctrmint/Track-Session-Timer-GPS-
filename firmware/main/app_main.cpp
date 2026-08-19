#include "esp_log.h"

#include <cstddef>
#include "screen_router.hpp"
#include "track_timer/display/panel.hpp"
#include "track_timer/domain/contracts.hpp"
#include "track_timer/catalog/track_catalog.hpp"
#include "track_timer/catalog/track_loader.hpp"
#include "track_timer/imu/qmi8658.hpp"
#include "track_timer/storage/sd_card.hpp"
#include "track_timer/storage/sd_track_store.hpp"
#include "track_timer/timing/engine.hpp"
#include "esp_heap_caps.h"

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

    const auto imu = track_timer::imu::start();
    ESP_LOGI(kTag, "imu: %s", track_timer::imu::imu_start_result_name(imu));

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

    // Track catalog. The definition array and the load scratch both live in PSRAM:
    // TrackDefinition is 3.6 KB (about 115 KB for 32 entries) and the file blob alone is
    // 16 KB, so neither belongs in internal RAM or on a task stack. The card is the only
    // source; nothing is built into the firmware.
    namespace catalog = track_timer::catalog;
    static track_timer::storage::SdTrackStore track_store{};
    auto* catalog_storage = static_cast<track_timer::track::TrackDefinition*>(
        heap_caps_calloc(track_timer::track::kMaximumCatalogTracks,
                         sizeof(track_timer::track::TrackDefinition),
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (catalog_storage == nullptr) {
        ESP_LOGE(kTag, "no PSRAM for the track catalog; running timer-only");
    }
    static catalog::TrackCatalog track_catalog{
        catalog_storage, catalog_storage == nullptr
                             ? 0U
                             : track_timer::track::kMaximumCatalogTracks};

    static track_timer::catalog::TrackLoadScratch* load_scratch =
        static_cast<track_timer::catalog::TrackLoadScratch*>(heap_caps_calloc(
            1, sizeof(track_timer::catalog::TrackLoadScratch),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (load_scratch == nullptr) {
        ESP_LOGE(kTag, "no PSRAM for the track load scratch; running timer-only");
    }

    const auto catalog_status =
        load_scratch == nullptr
            ? track_timer::catalog::CatalogStatus{}
            : track_catalog.rebuild(track_store, track_store, load_scratch->blob);
    ESP_LOGI(kTag, "track catalog: %s (%u discovered, %u loaded, %u rejected, cap %u)",
             catalog::catalog_build_result_name(catalog_status.result),
             static_cast<unsigned>(catalog_status.discovered),
             static_cast<unsigned>(catalog_status.loaded),
             static_cast<unsigned>(catalog_status.rejected),
             static_cast<unsigned>(catalog_status.capacity));
    if (catalog_status.truncated()) {
        ESP_LOGW(kTag, "track catalog TRUNCATED: the card holds more tracks than the "
                       "%u-entry catalog; some circuits are not selectable",
                 static_cast<unsigned>(catalog_status.capacity));
    }
    if (catalog_status.loaded == 0) {
        ESP_LOGW(kTag, "no track geometry on the card at %s; timer-only operation",
                 track_store.root());
    }
    for (std::size_t index = 0; index < track_catalog.view().count; ++index) {
        const auto& definition = track_catalog.view().definitions[index];
        ESP_LOGI(kTag, "  track[%u] %s \"%s\" rev %u",
                 static_cast<unsigned>(index), definition.track_id.data(),
                 definition.name.data(), static_cast<unsigned>(definition.revision));
    }

    if (!track_timer::main_app::start_screen_router()) {
        ESP_LOGE(kTag, "screen router failed to build");
        return;
    }

    track_timer::display::set_service_callback(track_timer::main_app::service_screen_router);
    ESP_LOGI(kTag, "ready screen presented on the 600x450 panel");
    ESP_LOGI(kTag, "GNSS, storage and IMU drivers are not implemented yet");
}

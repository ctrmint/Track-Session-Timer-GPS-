#include "esp_log.h"
#include "track_timer/domain/contracts.hpp"

namespace {
constexpr const char *kTag = "track_timer";
}

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "TrackSessionTimer GPS bootstrap");
    ESP_LOGI(kTag, "GNSS fix queue capacity: %u",
             static_cast<unsigned>(track_timer::domain::queue_capacity::gnss_fixes));
    ESP_LOGI(kTag, "Hardware bring-up not yet implemented");
}

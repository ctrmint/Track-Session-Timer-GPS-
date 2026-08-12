#include "esp_log.h"

namespace {
constexpr const char *kTag = "track_timer";
}

extern "C" void app_main(void)
{
    ESP_LOGI(kTag, "TrackSessionTimer GPS bootstrap");
    ESP_LOGI(kTag, "Hardware bring-up not yet implemented");
}

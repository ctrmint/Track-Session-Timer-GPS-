#include "track_timer/logger/task.hpp"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace track_timer::logger {
namespace {

void logger_task_entry(void* argument)
{
    auto& context = *static_cast<LoggerTaskContext*>(argument);
    const auto delay_ticks = pdMS_TO_TICKS(context.configuration.service_period_ms);
    for (;;) {
        (void)context.logger->service(esp_timer_get_time());
        vTaskDelay(delay_ticks);
    }
}

}  // namespace

bool start_logger_task(LoggerTaskContext& context, AsyncLogger& logger,
                       const LoggerTaskConfiguration configuration) noexcept
{
    if (context.native_handle != nullptr || configuration.stack_size_bytes < 2'048 ||
        configuration.service_period_ms == 0 || configuration.priority == 0) {
        return false;
    }

    context.logger = &logger;
    context.configuration = configuration;
    TaskHandle_t handle = nullptr;
    const auto core = configuration.core < 0 ? tskNO_AFFINITY : configuration.core;
    const auto result = xTaskCreatePinnedToCore(
        logger_task_entry, "session_logger", configuration.stack_size_bytes, &context,
        configuration.priority, &handle, core);
    if (result != pdPASS) {
        context.logger = nullptr;
        return false;
    }
    context.native_handle = handle;
    return true;
}

}  // namespace track_timer::logger

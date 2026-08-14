#include "track_timer/timing/embedded_runtime.hpp"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>

namespace track_timer::timing {
namespace {

constexpr UBaseType_t kTimingTaskPriority = configMAX_PRIORITIES - 2;
constexpr std::uint32_t kTimingTaskStackWords = 4'096;

struct EmbeddedTimingState {
    TimingEngine engine{};
    StaticQueue_t fix_queue_control{};
    StaticQueue_t event_queue_control{};
    std::array<std::uint8_t, domain::queue_capacity::gnss_fixes * sizeof(domain::GnssFix)>
        fix_queue_storage{};
    std::array<std::uint8_t, domain::queue_capacity::lap_events * sizeof(domain::LapEvent)>
        event_queue_storage{};
    StaticTask_t task_control{};
    std::array<StackType_t, kTimingTaskStackWords> task_stack{};
    QueueHandle_t fix_queue{nullptr};
    QueueHandle_t event_queue{nullptr};
    TaskHandle_t task{nullptr};
    EmbeddedTimingMetrics metrics{};
    portMUX_TYPE metrics_lock = portMUX_INITIALIZER_UNLOCKED;
    bool started{false};
};

EmbeddedTimingState runtime{};

void update_queue_high_water_marks() noexcept
{
    const auto fix_depth = static_cast<std::size_t>(uxQueueMessagesWaiting(runtime.fix_queue));
    const auto event_depth =
        static_cast<std::size_t>(uxQueueMessagesWaiting(runtime.event_queue));
    portENTER_CRITICAL(&runtime.metrics_lock);
    runtime.metrics.fix_queue_high_water_mark =
        std::max(runtime.metrics.fix_queue_high_water_mark, fix_depth);
    runtime.metrics.lap_event_queue_high_water_mark =
        std::max(runtime.metrics.lap_event_queue_high_water_mark, event_depth);
    portEXIT_CRITICAL(&runtime.metrics_lock);
}

void timing_task(void*) noexcept
{
    domain::GnssFix fix{};
    while (true) {
        if (xQueueReceive(runtime.fix_queue, &fix, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const auto started_us = esp_timer_get_time();
        const auto decision = runtime.engine.process_fix(fix, started_us);
        bool event_emitted = false;
        bool event_dropped = false;
        if (decision.lap_update.has_lap_event) {
            if (xQueueSend(runtime.event_queue, &decision.lap_update.lap_event, 0) == pdTRUE) {
                event_emitted = true;
            } else {
                event_dropped = true;
            }
        }
        const auto elapsed_us = esp_timer_get_time() - started_us;
        portENTER_CRITICAL(&runtime.metrics_lock);
        ++runtime.metrics.fixes_processed;
        runtime.metrics.lap_events_emitted += event_emitted ? 1U : 0U;
        runtime.metrics.lap_event_queue_drops += event_dropped ? 1U : 0U;
        runtime.metrics.deadline_misses += elapsed_us >= kTimingFixDeadlineUs ? 1U : 0U;
        runtime.metrics.maximum_processing_us =
            std::max(runtime.metrics.maximum_processing_us, elapsed_us);
        portEXIT_CRITICAL(&runtime.metrics_lock);
        update_queue_high_water_marks();
    }
}

}  // namespace

bool start_embedded_timing_runtime(const TimingEngineConfig& config) noexcept
{
    if (runtime.started ||
        runtime.engine.configure(config) != TimingEngineConfigureResult::configured) {
        return false;
    }
    runtime.fix_queue = xQueueCreateStatic(
        domain::queue_capacity::gnss_fixes, sizeof(domain::GnssFix),
        runtime.fix_queue_storage.data(), &runtime.fix_queue_control);
    runtime.event_queue = xQueueCreateStatic(
        domain::queue_capacity::lap_events, sizeof(domain::LapEvent),
        runtime.event_queue_storage.data(), &runtime.event_queue_control);
    if (runtime.fix_queue == nullptr || runtime.event_queue == nullptr) {
        return false;
    }
    runtime.task = xTaskCreateStatic(timing_task, "lap_timing", kTimingTaskStackWords,
                                     nullptr, kTimingTaskPriority,
                                     runtime.task_stack.data(), &runtime.task_control);
    runtime.started = runtime.task != nullptr;
    return runtime.started;
}

bool enqueue_timing_fix(const domain::GnssFix& fix) noexcept
{
    if (!runtime.started) {
        return false;
    }
    const bool accepted = xQueueSend(runtime.fix_queue, &fix, 0) == pdTRUE;
    portENTER_CRITICAL(&runtime.metrics_lock);
    runtime.metrics.fixes_enqueued += accepted ? 1U : 0U;
    runtime.metrics.fix_queue_drops += accepted ? 0U : 1U;
    portEXIT_CRITICAL(&runtime.metrics_lock);
    update_queue_high_water_marks();
    return accepted;
}

bool try_receive_lap_event(domain::LapEvent& event) noexcept
{
    if (!runtime.started) {
        return false;
    }
    return xQueueReceive(runtime.event_queue, &event, 0) == pdTRUE;
}

EmbeddedTimingMetrics embedded_timing_metrics() noexcept
{
    portENTER_CRITICAL(&runtime.metrics_lock);
    const auto metrics = runtime.metrics;
    portEXIT_CRITICAL(&runtime.metrics_lock);
    return metrics;
}

}  // namespace track_timer::timing

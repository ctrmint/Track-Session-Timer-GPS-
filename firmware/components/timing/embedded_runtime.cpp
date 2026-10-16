#include "track_timer/timing/embedded_runtime.hpp"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>

namespace track_timer::timing {
namespace {

constexpr UBaseType_t kTimingTaskPriority = configMAX_PRIORITIES - 2;
constexpr std::uint32_t kTimingTaskStackWords = 4'096;

// How long a caller changing circuit will wait for the task to put the engine down.
//
// Two and a half times the per-fix deadline, which is the longest the lock can honestly be
// held. Both directions cost something: the caller is a track selection running on the UI
// tick, so this is also the longest the interface can freeze while a circuit is swapped -
// and giving up early is worse, because it leaves the device timing against a circuit the
// driver has just changed away from. In practice process_fix takes tens of microseconds
// and the wait is nothing; this bounds the pathological case at both ends.
constexpr TickType_t kEngineLockWaitTicks = pdMS_TO_TICKS(100);

// Woken this often even with nothing to time, so the task is not parked indefinitely on an
// empty queue. Nothing depends on the period; it exists so the task has a heartbeat.
constexpr TickType_t kIdleWakeTicks = pdMS_TO_TICKS(250);

struct EmbeddedTimingState {
    TimingEngine engine{};
    StaticQueue_t fix_queue_control{};
    StaticQueue_t event_queue_control{};
    StaticQueue_t gate_record_queue_control{};
    std::array<std::uint8_t, domain::queue_capacity::gnss_fixes * sizeof(domain::GnssFix)>
        fix_queue_storage{};
    std::array<std::uint8_t, domain::queue_capacity::lap_events * sizeof(domain::LapEvent)>
        event_queue_storage{};
    std::array<std::uint8_t, domain::queue_capacity::gate_crossing_records *
                                 sizeof(GateCrossingDecision)>
        gate_record_queue_storage{};
    StaticSemaphore_t engine_mutex_control{};
    SemaphoreHandle_t engine_mutex{nullptr};
    StaticTask_t task_control{};
    std::array<StackType_t, kTimingTaskStackWords> task_stack{};
    QueueHandle_t fix_queue{nullptr};
    QueueHandle_t event_queue{nullptr};
    QueueHandle_t gate_record_queue{nullptr};
    TaskHandle_t task{nullptr};
    EmbeddedTimingMetrics metrics{};
    LapStateSnapshot lap_snapshot{};
    portMUX_TYPE metrics_lock = portMUX_INITIALIZER_UNLOCKED;
    bool started{false};
};

EmbeddedTimingState runtime{};

void update_queue_high_water_marks() noexcept
{
    const auto fix_depth = static_cast<std::size_t>(uxQueueMessagesWaiting(runtime.fix_queue));
    const auto event_depth =
        static_cast<std::size_t>(uxQueueMessagesWaiting(runtime.event_queue));
    const auto gate_record_depth =
        static_cast<std::size_t>(uxQueueMessagesWaiting(runtime.gate_record_queue));
    portENTER_CRITICAL(&runtime.metrics_lock);
    runtime.metrics.fix_queue_high_water_mark =
        std::max(runtime.metrics.fix_queue_high_water_mark, fix_depth);
    runtime.metrics.lap_event_queue_high_water_mark =
        std::max(runtime.metrics.lap_event_queue_high_water_mark, event_depth);
    runtime.metrics.gate_record_queue_high_water_mark =
        std::max(runtime.metrics.gate_record_queue_high_water_mark, gate_record_depth);
    portEXIT_CRITICAL(&runtime.metrics_lock);
}

void publish_lap_snapshot(const LapStateSnapshot& snapshot) noexcept
{
    portENTER_CRITICAL(&runtime.metrics_lock);
    runtime.lap_snapshot = snapshot;
    portEXIT_CRITICAL(&runtime.metrics_lock);
}

void timing_task(void*) noexcept
{
    domain::GnssFix fix{};
    while (true) {
        // A timeout rather than portMAX_DELAY. The engine mutex is only ever contended
        // while this task is awake, and a task parked forever on an empty queue is also a
        // task that never shows up in a stack-usage reading.
        if (xQueueReceive(runtime.fix_queue, &fix, kIdleWakeTicks) != pdTRUE) {
            continue;
        }
        const auto started_us = esp_timer_get_time();
        // Held across process_fix and nothing else. A spinlock would be wrong here: this
        // runs for tens of microseconds and has a 40 ms budget, and portENTER_CRITICAL
        // disables interrupts for its whole duration.
        if (xSemaphoreTake(runtime.engine_mutex, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const auto decision = runtime.engine.process_fix(fix, started_us);
        publish_lap_snapshot(decision.lap_update.snapshot);
        xSemaphoreGive(runtime.engine_mutex);
        bool event_emitted = false;
        bool event_dropped = false;
        if (decision.lap_update.has_lap_event) {
            if (xQueueSend(runtime.event_queue, &decision.lap_update.lap_event, 0) == pdTRUE) {
                event_emitted = true;
            } else {
                event_dropped = true;
            }
        }
        std::uint64_t gate_events = 0;
        std::uint64_t gate_records = 0;
        std::uint64_t gate_record_drops = 0;
        for (const auto& gate_decision : decision.gate_decisions) {
            if (gate_decision.result == GateCrossingResult::not_evaluated ||
                gate_decision.result == GateCrossingResult::no_intersection) {
                continue;
            }
            gate_events += gate_decision.has_event ? 1U : 0U;
            if (xQueueSend(runtime.gate_record_queue, &gate_decision, 0) == pdTRUE) {
                ++gate_records;
            }
            else {
                ++gate_record_drops;
            }
        }
        const auto elapsed_us = esp_timer_get_time() - started_us;
        portENTER_CRITICAL(&runtime.metrics_lock);
        ++runtime.metrics.fixes_processed;
        runtime.metrics.lap_events_emitted += event_emitted ? 1U : 0U;
        runtime.metrics.lap_event_queue_drops += event_dropped ? 1U : 0U;
        runtime.metrics.gate_events_emitted += gate_events;
        runtime.metrics.gate_records_emitted += gate_records;
        runtime.metrics.gate_record_queue_drops += gate_record_drops;
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
    runtime.engine_mutex = xSemaphoreCreateMutexStatic(&runtime.engine_mutex_control);
    if (runtime.engine_mutex == nullptr) {
        return false;
    }
    runtime.fix_queue = xQueueCreateStatic(
        domain::queue_capacity::gnss_fixes, sizeof(domain::GnssFix),
        runtime.fix_queue_storage.data(), &runtime.fix_queue_control);
    runtime.event_queue = xQueueCreateStatic(
        domain::queue_capacity::lap_events, sizeof(domain::LapEvent),
        runtime.event_queue_storage.data(), &runtime.event_queue_control);
    runtime.gate_record_queue = xQueueCreateStatic(
        domain::queue_capacity::gate_crossing_records, sizeof(GateCrossingDecision),
        runtime.gate_record_queue_storage.data(), &runtime.gate_record_queue_control);
    if (runtime.fix_queue == nullptr || runtime.event_queue == nullptr ||
        runtime.gate_record_queue == nullptr) {
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

bool try_receive_gate_crossing_record(GateCrossingDecision& decision) noexcept
{
    if (!runtime.started) {
        return false;
    }
    return xQueueReceive(runtime.gate_record_queue, &decision, 0) == pdTRUE;
}

EmbeddedTimingMetrics embedded_timing_metrics() noexcept
{
    portENTER_CRITICAL(&runtime.metrics_lock);
    const auto metrics = runtime.metrics;
    portEXIT_CRITICAL(&runtime.metrics_lock);
    return metrics;
}

LapStateSnapshot embedded_lap_snapshot() noexcept
{
    portENTER_CRITICAL(&runtime.metrics_lock);
    const auto snapshot = runtime.lap_snapshot;
    portEXIT_CRITICAL(&runtime.metrics_lock);
    return snapshot;
}

TimingReconfigureResult reconfigure_embedded_timing(const TimingEngineConfig& config) noexcept
{
    if (!runtime.started) {
        return TimingReconfigureResult::not_started;
    }
    if (xSemaphoreTake(runtime.engine_mutex, kEngineLockWaitTicks) != pdTRUE) {
        return TimingReconfigureResult::busy;
    }
    const auto result = runtime.engine.configure(config);
    // Counted before the reset, because afterwards there is nothing left to count. These
    // are fixes that were accepted and queued and will now never be timed - which is a
    // real loss, just not a fault, and #18 asks for loss to be counted by cause.
    const auto abandoned = static_cast<std::uint64_t>(uxQueueMessagesWaiting(runtime.fix_queue));
    xQueueReset(runtime.fix_queue);
    xQueueReset(runtime.event_queue);
    xQueueReset(runtime.gate_record_queue);
    xSemaphoreGive(runtime.engine_mutex);

    portENTER_CRITICAL(&runtime.metrics_lock);
    runtime.metrics.fixes_abandoned_on_reconfigure += abandoned;
    runtime.lap_snapshot = {};
    portEXIT_CRITICAL(&runtime.metrics_lock);

    return result == TimingEngineConfigureResult::configured
               ? TimingReconfigureResult::reconfigured
               : TimingReconfigureResult::rejected;
}

}  // namespace track_timer::timing

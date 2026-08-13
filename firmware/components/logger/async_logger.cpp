#include "track_timer/logger/async_logger.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace track_timer::logger {
namespace {

constexpr std::array<std::int64_t, 7> kLatencyUpperBoundsUs{
    1'000,
    5'000,
    20'000,
    100'000,
    250'000,
    1'000'000,
    std::numeric_limits<std::int64_t>::max(),
};

bool storage_can_write(const board::StorageHealth health) noexcept
{
    return health == board::StorageHealth::ready || health == board::StorageHealth::degraded;
}

}  // namespace

AsyncLogger::AsyncLogger(board::StorageBackend& storage) noexcept : storage_(storage)
{
    for (auto& bucket : latency_histogram_) {
        bucket.store(0, std::memory_order_relaxed);
    }
}

EnqueueResult AsyncLogger::enqueue(const domain::LogRecord& record) noexcept
{
    if (record.ordering_monotonic_us < 0 || record.payload_size > domain::kMaxLogPayloadBytes) {
        rejected_invalid_.fetch_add(1, std::memory_order_relaxed);
        return EnqueueResult::invalid_record;
    }
    if (!try_lock_queue()) {
        dropped_busy_.fetch_add(1, std::memory_order_relaxed);
        return EnqueueResult::queue_busy;
    }
    if (size_ == kLoggerQueueCapacity) {
        unlock_queue();
        dropped_full_.fetch_add(1, std::memory_order_relaxed);
        return EnqueueResult::queue_full;
    }

    queue_[(head_ + size_) % kLoggerQueueCapacity] = record;
    ++size_;
    depth_.store(size_, std::memory_order_relaxed);
    auto high_water = high_water_mark_.load(std::memory_order_relaxed);
    while (size_ > high_water &&
           !high_water_mark_.compare_exchange_weak(high_water, size_, std::memory_order_relaxed)) {
    }
    unlock_queue();
    accepted_records_.fetch_add(1, std::memory_order_relaxed);
    return EnqueueResult::accepted;
}

void AsyncLogger::request_flush() noexcept
{
    flush_requests_.fetch_add(1, std::memory_order_relaxed);
    flush_requested_.store(true, std::memory_order_release);
}

ServiceResult AsyncLogger::service(const std::int64_t now_us) noexcept
{
    if (now_us < 0 ||
        (last_service_us_ != domain::kUnavailableTime && now_us < last_service_us_)) {
        return ServiceResult::non_monotonic_time;
    }
    last_service_us_ = now_us;

    if (staged_count_ != 0) {
        return write_staged(now_us);
    }

    const bool flush_requested = flush_requested_.load(std::memory_order_acquire);
    const auto depth = depth_.load(std::memory_order_relaxed);
    if (depth == 0) {
        if (!flush_requested) {
            return ServiceResult::idle;
        }
        if (!storage_can_write(storage_.status().health)) {
            storage_unavailable_attempts_.fetch_add(1, std::memory_order_relaxed);
            return ServiceResult::storage_unavailable;
        }
        if (!storage_.flush()) {
            failed_batch_attempts_.fetch_add(1, std::memory_order_relaxed);
            return ServiceResult::write_failed;
        }
        flush_requested_.store(false, std::memory_order_release);
        completed_flushes_.fetch_add(1, std::memory_order_relaxed);
        last_write_us_ = now_us;
        return ServiceResult::flushed;
    }

    if (last_write_us_ == domain::kUnavailableTime) {
        last_write_us_ = now_us;
    }
    const bool batch_ready = depth >= kLoggerBatchCapacity;
    const bool periodic_flush_due = now_us - last_write_us_ >= kLoggerPeriodicFlushUs;
    if (!batch_ready && !periodic_flush_due && !flush_requested) {
        return ServiceResult::idle;
    }
    if (!stage_next_batch(flush_requested)) {
        return ServiceResult::idle;
    }
    return write_staged(now_us);
}

LoggerMetrics AsyncLogger::metrics() const noexcept
{
    LoggerMetrics result{};
    result.queue.depth = depth_.load(std::memory_order_relaxed);
    result.queue.high_water_mark = high_water_mark_.load(std::memory_order_relaxed);
    result.queue.dropped_full = dropped_full_.load(std::memory_order_relaxed);
    result.queue.dropped_busy = dropped_busy_.load(std::memory_order_relaxed);
    result.queue.rejected_invalid = rejected_invalid_.load(std::memory_order_relaxed);
    result.accepted_records = accepted_records_.load(std::memory_order_relaxed);
    result.written_records = written_records_.load(std::memory_order_relaxed);
    result.written_batches = written_batches_.load(std::memory_order_relaxed);
    result.storage_unavailable_attempts =
        storage_unavailable_attempts_.load(std::memory_order_relaxed);
    result.failed_batch_attempts = failed_batch_attempts_.load(std::memory_order_relaxed);
    result.flush_requests = flush_requests_.load(std::memory_order_relaxed);
    result.completed_flushes = completed_flushes_.load(std::memory_order_relaxed);
    result.maximum_batch_size = maximum_batch_size_.load(std::memory_order_relaxed);
    result.latency.samples = latency_samples_.load(std::memory_order_relaxed);
    result.latency.total_us = latency_total_us_.load(std::memory_order_relaxed);
    result.latency.maximum_us = latency_maximum_us_.load(std::memory_order_relaxed);
    result.latency.p50_upper_bound_us = percentile_upper_bound(50, 100);
    result.latency.p95_upper_bound_us = percentile_upper_bound(95, 100);
    result.latency.p99_upper_bound_us = percentile_upper_bound(99, 100);
    return result;
}

void AsyncLogger::reset() noexcept
{
    while (!try_lock_queue()) {
    }
    head_ = 0;
    size_ = 0;
    staged_count_ = 0;
    staged_completes_flush_ = false;
    last_service_us_ = domain::kUnavailableTime;
    last_write_us_ = domain::kUnavailableTime;
    flush_requested_.store(false, std::memory_order_relaxed);
    depth_.store(0, std::memory_order_relaxed);
    high_water_mark_.store(0, std::memory_order_relaxed);
    accepted_records_.store(0, std::memory_order_relaxed);
    dropped_full_.store(0, std::memory_order_relaxed);
    dropped_busy_.store(0, std::memory_order_relaxed);
    rejected_invalid_.store(0, std::memory_order_relaxed);
    flush_requests_.store(0, std::memory_order_relaxed);
    written_records_.store(0, std::memory_order_relaxed);
    written_batches_.store(0, std::memory_order_relaxed);
    storage_unavailable_attempts_.store(0, std::memory_order_relaxed);
    failed_batch_attempts_.store(0, std::memory_order_relaxed);
    completed_flushes_.store(0, std::memory_order_relaxed);
    maximum_batch_size_.store(0, std::memory_order_relaxed);
    latency_samples_.store(0, std::memory_order_relaxed);
    latency_total_us_.store(0, std::memory_order_relaxed);
    latency_maximum_us_.store(0, std::memory_order_relaxed);
    for (auto& bucket : latency_histogram_) {
        bucket.store(0, std::memory_order_relaxed);
    }
    unlock_queue();
}

bool AsyncLogger::try_lock_queue() const noexcept
{
    return !queue_lock_.test_and_set(std::memory_order_acquire);
}

void AsyncLogger::unlock_queue() const noexcept
{
    queue_lock_.clear(std::memory_order_release);
}

bool AsyncLogger::stage_next_batch(const bool flush_requested) noexcept
{
    if (!try_lock_queue()) {
        return false;
    }
    staged_count_ = std::min(size_, kLoggerBatchCapacity);
    for (std::size_t index = 0; index < staged_count_; ++index) {
        staged_[index] = queue_[(head_ + index) % kLoggerQueueCapacity];
    }
    head_ = (head_ + staged_count_) % kLoggerQueueCapacity;
    size_ -= staged_count_;
    depth_.store(size_, std::memory_order_relaxed);
    staged_completes_flush_ = flush_requested && size_ == 0;
    unlock_queue();
    return staged_count_ != 0;
}

ServiceResult AsyncLogger::write_staged(const std::int64_t now_us) noexcept
{
    if (!storage_can_write(storage_.status().health)) {
        storage_unavailable_attempts_.fetch_add(1, std::memory_order_relaxed);
        return ServiceResult::storage_unavailable;
    }
    if (!storage_.append_batch(staged_.data(), staged_count_)) {
        failed_batch_attempts_.fetch_add(1, std::memory_order_relaxed);
        return ServiceResult::write_failed;
    }

    for (std::size_t index = 0; index < staged_count_; ++index) {
        record_latency(std::max<std::int64_t>(0, now_us - staged_[index].ordering_monotonic_us));
    }
    written_records_.fetch_add(staged_count_, std::memory_order_relaxed);
    written_batches_.fetch_add(1, std::memory_order_relaxed);
    auto maximum_batch = maximum_batch_size_.load(std::memory_order_relaxed);
    while (staged_count_ > maximum_batch &&
           !maximum_batch_size_.compare_exchange_weak(maximum_batch, staged_count_,
                                                      std::memory_order_relaxed)) {
    }
    staged_count_ = 0;
    last_write_us_ = now_us;

    if (staged_completes_flush_) {
        staged_completes_flush_ = false;
        if (!storage_.flush()) {
            failed_batch_attempts_.fetch_add(1, std::memory_order_relaxed);
            return ServiceResult::write_failed;
        }
        flush_requested_.store(false, std::memory_order_release);
        completed_flushes_.fetch_add(1, std::memory_order_relaxed);
        return ServiceResult::flushed;
    }
    return ServiceResult::batch_written;
}

void AsyncLogger::record_latency(const std::int64_t latency_us) noexcept
{
    latency_samples_.fetch_add(1, std::memory_order_relaxed);
    latency_total_us_.fetch_add(static_cast<std::uint64_t>(latency_us),
                                std::memory_order_relaxed);
    auto maximum = latency_maximum_us_.load(std::memory_order_relaxed);
    while (latency_us > maximum &&
           !latency_maximum_us_.compare_exchange_weak(maximum, latency_us,
                                                      std::memory_order_relaxed)) {
    }
    for (std::size_t index = 0; index < kLatencyUpperBoundsUs.size(); ++index) {
        if (latency_us <= kLatencyUpperBoundsUs[index]) {
            latency_histogram_[index].fetch_add(1, std::memory_order_relaxed);
            break;
        }
    }
}

std::int64_t AsyncLogger::percentile_upper_bound(const std::uint64_t numerator,
                                                  const std::uint64_t denominator) const noexcept
{
    const auto samples = latency_samples_.load(std::memory_order_relaxed);
    if (samples == 0) {
        return 0;
    }
    const auto target = (samples * numerator + denominator - 1) / denominator;
    std::uint64_t cumulative = 0;
    for (std::size_t index = 0; index < kLatencyUpperBoundsUs.size(); ++index) {
        cumulative += latency_histogram_[index].load(std::memory_order_relaxed);
        if (cumulative >= target) {
            return kLatencyUpperBoundsUs[index] == std::numeric_limits<std::int64_t>::max()
                       ? latency_maximum_us_.load(std::memory_order_relaxed)
                       : kLatencyUpperBoundsUs[index];
        }
    }
    return latency_maximum_us_.load(std::memory_order_relaxed);
}

}  // namespace track_timer::logger

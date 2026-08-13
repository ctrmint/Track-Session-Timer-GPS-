#pragma once

#include "track_timer/board/platform.hpp"
#include "track_timer/domain/contracts.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace track_timer::logger {

inline constexpr std::size_t kLoggerQueueCapacity = domain::queue_capacity::log_records;
inline constexpr std::size_t kLoggerBatchCapacity = 16;
inline constexpr std::int64_t kLoggerPeriodicFlushUs = 250'000;

enum class EnqueueResult : std::uint8_t {
    accepted,
    queue_full,
    queue_busy,
    invalid_record,
};

enum class ServiceResult : std::uint8_t {
    idle,
    batch_written,
    flushed,
    storage_unavailable,
    write_failed,
    non_monotonic_time,
};

struct LoggerQueueMetrics {
    std::size_t capacity{kLoggerQueueCapacity};
    std::size_t depth{0};
    std::size_t high_water_mark{0};
    std::uint64_t dropped_full{0};
    std::uint64_t dropped_busy{0};
    std::uint64_t rejected_invalid{0};

    [[nodiscard]] constexpr std::uint64_t dropped() const noexcept
    {
        return dropped_full + dropped_busy;
    }
};

struct LoggerLatencyMetrics {
    std::uint64_t samples{0};
    std::uint64_t total_us{0};
    std::int64_t maximum_us{0};
    std::int64_t p50_upper_bound_us{0};
    std::int64_t p95_upper_bound_us{0};
    std::int64_t p99_upper_bound_us{0};

    [[nodiscard]] constexpr std::uint64_t average_us() const noexcept
    {
        return samples == 0 ? 0 : total_us / samples;
    }
};

struct LoggerMetrics {
    LoggerQueueMetrics queue{};
    LoggerLatencyMetrics latency{};
    std::uint64_t accepted_records{0};
    std::uint64_t written_records{0};
    std::uint64_t written_batches{0};
    std::uint64_t storage_unavailable_attempts{0};
    std::uint64_t failed_batch_attempts{0};
    std::uint64_t flush_requests{0};
    std::uint64_t completed_flushes{0};
    std::size_t maximum_batch_size{0};
};

// Producers call only enqueue/request_flush. The dedicated logger task owns service(),
// and therefore owns every StorageBackend call.
class AsyncLogger {
  public:
    explicit AsyncLogger(board::StorageBackend& storage) noexcept;

    [[nodiscard]] EnqueueResult enqueue(const domain::LogRecord& record) noexcept;
    void request_flush() noexcept;
    [[nodiscard]] ServiceResult service(std::int64_t now_us) noexcept;
    [[nodiscard]] LoggerMetrics metrics() const noexcept;

    void reset() noexcept;

  private:
    [[nodiscard]] bool try_lock_queue() const noexcept;
    void unlock_queue() const noexcept;
    [[nodiscard]] bool stage_next_batch(bool flush_requested) noexcept;
    [[nodiscard]] ServiceResult write_staged(std::int64_t now_us) noexcept;
    void record_latency(std::int64_t latency_us) noexcept;
    [[nodiscard]] std::int64_t percentile_upper_bound(std::uint64_t numerator,
                                                       std::uint64_t denominator) const noexcept;

    board::StorageBackend& storage_;
    std::array<domain::LogRecord, kLoggerQueueCapacity> queue_{};
    std::array<domain::LogRecord, kLoggerBatchCapacity> staged_{};
    std::size_t head_{0};
    std::size_t size_{0};
    std::size_t staged_count_{0};
    std::int64_t last_service_us_{domain::kUnavailableTime};
    std::int64_t last_write_us_{domain::kUnavailableTime};
    bool staged_completes_flush_{false};

    mutable std::atomic_flag queue_lock_ = ATOMIC_FLAG_INIT;
    std::atomic<bool> flush_requested_{false};
    std::atomic<std::size_t> depth_{0};
    std::atomic<std::size_t> high_water_mark_{0};
    std::atomic<std::uint64_t> accepted_records_{0};
    std::atomic<std::uint64_t> dropped_full_{0};
    std::atomic<std::uint64_t> dropped_busy_{0};
    std::atomic<std::uint64_t> rejected_invalid_{0};
    std::atomic<std::uint64_t> flush_requests_{0};

    std::atomic<std::uint64_t> written_records_{0};
    std::atomic<std::uint64_t> written_batches_{0};
    std::atomic<std::uint64_t> storage_unavailable_attempts_{0};
    std::atomic<std::uint64_t> failed_batch_attempts_{0};
    std::atomic<std::uint64_t> completed_flushes_{0};
    std::atomic<std::size_t> maximum_batch_size_{0};
    std::atomic<std::uint64_t> latency_samples_{0};
    std::atomic<std::uint64_t> latency_total_us_{0};
    std::atomic<std::int64_t> latency_maximum_us_{0};
    std::array<std::atomic<std::uint64_t>, 7> latency_histogram_{};
};

}  // namespace track_timer::logger

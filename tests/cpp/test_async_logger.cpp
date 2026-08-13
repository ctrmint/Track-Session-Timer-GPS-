#include "track_timer/logger/async_logger.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

class RecordingStorage final : public track_timer::board::StorageBackend {
  public:
    [[nodiscard]] track_timer::board::StorageStatus status() const noexcept override
    {
        return {1ULL << 30U, failures_, health_};
    }

    bool append(const track_timer::domain::LogRecord&) noexcept override
    {
        ++direct_append_calls_;
        return false;
    }

    bool append_batch(const track_timer::domain::LogRecord* records,
                      const std::size_t count) noexcept override
    {
        if (records == nullptr && count != 0) {
            ++failures_;
            return false;
        }
        if (fail_next_batch_) {
            fail_next_batch_ = false;
            ++failures_;
            return false;
        }
        ++batch_calls_;
        for (std::size_t index = 0; index < count; ++index) {
            records_.push_back(records[index]);
        }
        return true;
    }

    bool flush() noexcept override
    {
        if (fail_next_flush_) {
            fail_next_flush_ = false;
            ++failures_;
            return false;
        }
        ++flush_calls_;
        return true;
    }

    void reset() noexcept
    {
        records_.clear();
        health_ = track_timer::board::StorageHealth::ready;
        fail_next_batch_ = false;
        fail_next_flush_ = false;
        failures_ = 0;
        direct_append_calls_ = 0;
        batch_calls_ = 0;
        flush_calls_ = 0;
    }

    std::vector<track_timer::domain::LogRecord> records_{};
    track_timer::board::StorageHealth health_{track_timer::board::StorageHealth::ready};
    bool fail_next_batch_{false};
    bool fail_next_flush_{false};
    std::uint32_t failures_{0};
    std::uint64_t direct_append_calls_{0};
    std::uint64_t batch_calls_{0};
    std::uint64_t flush_calls_{0};
};

track_timer::domain::LogRecord make_record(const std::uint32_t sequence,
                                           const std::int64_t ordering_us)
{
    track_timer::domain::LogRecord record{};
    record.ordering_monotonic_us = ordering_us;
    record.sequence_number = sequence;
    record.payload_size = 32;
    record.type = track_timer::domain::LogRecordType::gnss_fix;
    return record;
}

void run_rate_stress(const std::int64_t period_us, const std::uint32_t expected_records)
{
    using namespace track_timer::logger;
    RecordingStorage storage;
    AsyncLogger logger{storage};

    std::int64_t now_us = 0;
    for (std::uint32_t sequence = 0; sequence < expected_records; ++sequence) {
        now_us += period_us;
        assert(logger.enqueue(make_record(sequence, now_us)) == EnqueueResult::accepted);
        const auto result = logger.service(now_us);
        assert(result != ServiceResult::write_failed);
        assert(result != ServiceResult::storage_unavailable);
    }
    logger.request_flush();
    while (logger.metrics().completed_flushes == 0) {
        now_us += 1'000;
        const auto result = logger.service(now_us);
        assert(result != ServiceResult::write_failed);
    }

    const auto metrics = logger.metrics();
    assert(metrics.accepted_records == expected_records);
    assert(metrics.written_records == expected_records);
    assert(metrics.queue.dropped() == 0);
    assert(metrics.queue.depth == 0);
    assert(metrics.maximum_batch_size <= kLoggerBatchCapacity);
    assert(metrics.latency.samples == expected_records);
    assert(metrics.latency.p95_upper_bound_us <= kLoggerPeriodicFlushUs);
    assert(storage.records_.size() == expected_records);
    assert(storage.direct_append_calls_ == 0);
}

}  // namespace

int main()
{
    using namespace track_timer;
    using namespace track_timer::logger;

    RecordingStorage storage;
    AsyncLogger logger{storage};

    domain::LogRecord invalid{};
    assert(logger.enqueue(invalid) == EnqueueResult::invalid_record);
    assert(logger.metrics().queue.rejected_invalid == 1);

    for (std::uint32_t sequence = 0; sequence < 15; ++sequence) {
        assert(logger.enqueue(make_record(sequence, sequence * 10'000)) ==
               EnqueueResult::accepted);
    }
    assert(logger.service(140'000) == ServiceResult::idle);
    assert(logger.service(389'999) == ServiceResult::idle);
    assert(logger.service(390'000) == ServiceResult::batch_written);
    assert(storage.records_.size() == 15);
    assert(storage.batch_calls_ == 1);
    assert(storage.direct_append_calls_ == 0);
    auto metrics = logger.metrics();
    assert(metrics.written_records == 15);
    assert(metrics.latency.maximum_us == 390'000);
    assert(metrics.latency.p95_upper_bound_us == 1'000'000);

    logger.reset();
    storage.reset();
    for (std::uint32_t sequence = 0; sequence < kLoggerBatchCapacity; ++sequence) {
        assert(logger.enqueue(make_record(sequence, 0)) == EnqueueResult::accepted);
    }
    assert(logger.service(0) == ServiceResult::batch_written);
    assert(storage.records_.size() == kLoggerBatchCapacity);
    assert(logger.metrics().maximum_batch_size == kLoggerBatchCapacity);

    logger.reset();
    storage.reset();
    for (std::uint32_t sequence = 0; sequence < 3; ++sequence) {
        assert(logger.enqueue(make_record(sequence, 100)) == EnqueueResult::accepted);
    }
    logger.request_flush();
    assert(logger.service(200) == ServiceResult::flushed);
    assert(storage.records_.size() == 3);
    assert(storage.flush_calls_ == 1);
    assert(logger.metrics().completed_flushes == 1);

    logger.reset();
    storage.reset();
    for (std::uint32_t sequence = 0; sequence < kLoggerBatchCapacity; ++sequence) {
        assert(logger.enqueue(make_record(sequence, 0)) == EnqueueResult::accepted);
    }
    storage.health_ = board::StorageHealth::unavailable;
    assert(logger.service(0) == ServiceResult::storage_unavailable);
    storage.health_ = board::StorageHealth::ready;
    storage.fail_next_batch_ = true;
    assert(logger.service(1'000) == ServiceResult::write_failed);
    assert(storage.records_.empty());
    assert(logger.service(2'000) == ServiceResult::batch_written);
    assert(storage.records_.size() == kLoggerBatchCapacity);
    assert(logger.metrics().failed_batch_attempts == 1);

    logger.reset();
    storage.reset();
    for (std::size_t sequence = 0; sequence < kLoggerQueueCapacity; ++sequence) {
        assert(logger.enqueue(make_record(static_cast<std::uint32_t>(sequence), 0)) ==
               EnqueueResult::accepted);
    }
    assert(logger.enqueue(make_record(999, 0)) == EnqueueResult::queue_full);
    metrics = logger.metrics();
    assert(metrics.queue.depth == kLoggerQueueCapacity);
    assert(metrics.queue.high_water_mark == kLoggerQueueCapacity);
    assert(metrics.queue.dropped_full == 1);
    assert(logger.service(-1) == ServiceResult::non_monotonic_time);

    // Thirty simulated minutes at the production rates, serviced on a 40 ms display cadence.
    run_rate_stress(50'000, 36'000);
    run_rate_stress(40'000, 45'000);

    std::cout << "Bounded async logging, failure visibility, and 20/25 Hz stress passed\n";
    return 0;
}

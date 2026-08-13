#pragma once

#include "track_timer/board/platform.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace track_timer::simulator {

struct QueueMetrics {
    std::size_t capacity{0};
    std::size_t depth{0};
    std::size_t high_water_mark{0};
    std::uint64_t dropped{0};
};

namespace detail {

template <typename Value, std::size_t Capacity>
class BoundedQueue {
  public:
    bool push(const Value& value) noexcept
    {
        if (size_ == Capacity) {
            ++dropped_;
            return false;
        }
        values_[(head_ + size_) % Capacity] = value;
        ++size_;
        if (size_ > high_water_mark_) {
            high_water_mark_ = size_;
        }
        return true;
    }

    bool pop(Value& value) noexcept
    {
        if (size_ == 0) {
            return false;
        }
        value = values_[head_];
        head_ = (head_ + 1) % Capacity;
        --size_;
        return true;
    }

    void reset() noexcept
    {
        head_ = 0;
        size_ = 0;
        high_water_mark_ = 0;
        dropped_ = 0;
    }

    [[nodiscard]] QueueMetrics metrics() const noexcept
    {
        return QueueMetrics{Capacity, size_, high_water_mark_, dropped_};
    }

  private:
    std::array<Value, Capacity> values_{};
    std::size_t head_{0};
    std::size_t size_{0};
    std::size_t high_water_mark_{0};
    std::uint64_t dropped_{0};
};

}  // namespace detail

struct GnssFixture {
    std::uint32_t format_version{1};
    std::string name{};
    std::vector<domain::GnssFix> fixes{};
};

[[nodiscard]] GnssFixture make_synthetic_gnss_fixture();
[[nodiscard]] bool load_gnss_fixture(const std::string& path, GnssFixture& fixture,
                                     std::string& error);

enum class GnssReplayRate : std::uint8_t {
    hz20 = 20,
    hz25 = 25,
};

enum class GnssMode : std::uint8_t {
    normal,
    loss,
    stale,
    corrupt,
};

[[nodiscard]] const char* gnss_mode_name(GnssMode mode) noexcept;

struct GnssDiagnostics {
    std::uint64_t scheduled{0};
    std::uint64_t emitted{0};
    std::uint64_t lost{0};
    std::uint64_t stale{0};
    std::uint64_t corrupt{0};
    std::uint32_t recoveries{0};
    QueueMetrics queue{};
};

class SimulatedClock final : public board::MonotonicClock {
  public:
    [[nodiscard]] std::int64_t now_us() const noexcept override;
    void advance_us(std::int64_t elapsed_us) noexcept;
    void reset() noexcept;

  private:
    std::int64_t now_us_{0};
};

class SimulatedGnss final : public board::GnssInput {
  public:
    SimulatedGnss(SimulatedClock& clock, GnssFixture fixture, GnssReplayRate rate);

    void advance() noexcept;
    void reset() noexcept;
    void set_mode(GnssMode mode) noexcept;
    bool try_read(domain::GnssFix& fix) noexcept override;

    [[nodiscard]] GnssMode mode() const noexcept;
    [[nodiscard]] GnssReplayRate rate() const noexcept;
    [[nodiscard]] const std::string& fixture_name() const noexcept;
    [[nodiscard]] GnssDiagnostics diagnostics() const noexcept;

  private:
    SimulatedClock& clock_;
    GnssFixture fixture_;
    GnssReplayRate rate_;
    GnssMode mode_{GnssMode::normal};
    detail::BoundedQueue<domain::GnssFix, domain::queue_capacity::gnss_fixes> queue_{};
    std::int64_t next_emit_us_{0};
    std::int64_t last_measurement_time_ns_{domain::kUnavailableTime};
    std::uint64_t scheduled_{0};
    std::uint64_t emitted_{0};
    std::uint64_t lost_{0};
    std::uint64_t stale_{0};
    std::uint64_t corrupt_{0};
    std::uint32_t recoveries_{0};
};

class SimulatedTouch final : public board::TouchInput {
  public:
    explicit SimulatedTouch(SimulatedClock& clock) noexcept;

    bool inject(std::int16_t x, std::int16_t y, bool pressed) noexcept;
    bool try_read(board::TouchSample& sample) noexcept override;
    void reset() noexcept;
    [[nodiscard]] QueueMetrics metrics() const noexcept;

  private:
    SimulatedClock& clock_;
    detail::BoundedQueue<board::TouchSample, 16> queue_{};
};

class SimulatedImu final : public board::ImuInput {
  public:
    explicit SimulatedImu(SimulatedClock& clock) noexcept;

    void advance() noexcept;
    void reset() noexcept;
    void set_available(bool available) noexcept;
    bool try_read(board::ImuSample& sample) noexcept override;
    [[nodiscard]] QueueMetrics metrics() const noexcept;
    [[nodiscard]] std::uint64_t emitted() const noexcept;

  private:
    SimulatedClock& clock_;
    detail::BoundedQueue<board::ImuSample, 64> queue_{};
    std::int64_t next_emit_us_{0};
    std::uint64_t emitted_{0};
    bool available_{true};
};

class SimulatedRtc final : public board::RtcSource {
  public:
    SimulatedRtc(SimulatedClock& clock, board::RtcDateTime epoch) noexcept;

    [[nodiscard]] board::RtcDateTime now() const noexcept override;
    void set_epoch(board::RtcDateTime epoch) noexcept;

  private:
    SimulatedClock& clock_;
    board::RtcDateTime epoch_{};
};

enum class StorageMode : std::uint8_t {
    ready,
    missing,
    full,
    slow,
    write_failed,
};

[[nodiscard]] const char* storage_mode_name(StorageMode mode) noexcept;

struct StorageDiagnostics {
    std::uint64_t accepted_records{0};
    std::uint64_t written_records{0};
    std::uint64_t written_bytes{0};
    std::uint64_t accepted_batches{0};
    std::uint64_t flushes{0};
    std::uint32_t recoveries{0};
    std::int64_t simulated_write_latency_us{0};
    QueueMetrics queue{};
};

class SimulatedStorage final : public board::StorageBackend {
  public:
    bool append(const domain::LogRecord& record) noexcept override;
    bool append_batch(const domain::LogRecord* records, std::size_t count) noexcept override;
    bool flush() noexcept override;
    [[nodiscard]] board::StorageStatus status() const noexcept override;

    void advance(std::int64_t elapsed_us) noexcept;
    void reset() noexcept;
    void set_mode(StorageMode mode) noexcept;
    [[nodiscard]] StorageMode mode() const noexcept;
    [[nodiscard]] StorageDiagnostics diagnostics() const noexcept;

  private:
    [[nodiscard]] std::int64_t write_latency_us() const noexcept;

    detail::BoundedQueue<domain::LogRecord, domain::queue_capacity::log_records> queue_{};
    StorageMode mode_{StorageMode::ready};
    std::int64_t write_budget_us_{0};
    std::uint64_t accepted_records_{0};
    std::uint64_t written_records_{0};
    std::uint64_t written_bytes_{0};
    std::uint32_t write_failures_{0};
    std::uint32_t recoveries_{0};
    std::uint64_t written_batches_{0};
    std::uint64_t flushes_{0};
};

struct DeviceDiagnostics {
    GnssDiagnostics gnss{};
    QueueMetrics touch_queue{};
    QueueMetrics imu_queue{};
    std::uint64_t imu_emitted{0};
    StorageDiagnostics storage{};
};

class SimulatedDevice {
  public:
    SimulatedDevice(GnssFixture fixture, GnssReplayRate rate);

    void advance(std::int64_t elapsed_us) noexcept;
    void reset() noexcept;
    [[nodiscard]] DeviceDiagnostics diagnostics() const noexcept;

    [[nodiscard]] SimulatedClock& clock() noexcept;
    [[nodiscard]] SimulatedGnss& gnss() noexcept;
    [[nodiscard]] SimulatedTouch& touch() noexcept;
    [[nodiscard]] SimulatedImu& imu() noexcept;
    [[nodiscard]] SimulatedRtc& rtc() noexcept;
    [[nodiscard]] SimulatedStorage& storage() noexcept;

    SimulatedDevice(const SimulatedDevice&) = delete;
    SimulatedDevice& operator=(const SimulatedDevice&) = delete;
    SimulatedDevice(SimulatedDevice&&) = delete;
    SimulatedDevice& operator=(SimulatedDevice&&) = delete;

  private:
    SimulatedClock clock_{};
    SimulatedGnss gnss_;
    SimulatedTouch touch_;
    SimulatedImu imu_;
    SimulatedRtc rtc_;
    SimulatedStorage storage_{};
};

}  // namespace track_timer::simulator

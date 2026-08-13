#pragma once

#include "track_timer/domain/contracts.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::board {

struct TouchSample {
    std::int64_t monotonic_us{domain::kUnavailableTime};
    std::int16_t x{0};
    std::int16_t y{0};
    bool pressed{false};
};

struct ImuSample {
    std::int64_t monotonic_us{domain::kUnavailableTime};
    float acceleration_x_mps2{0.0F};
    float acceleration_y_mps2{0.0F};
    float acceleration_z_mps2{0.0F};
    float angular_rate_x_dps{0.0F};
    float angular_rate_y_dps{0.0F};
    float angular_rate_z_dps{0.0F};
    bool valid{false};
};

struct RtcDateTime {
    std::int16_t year{0};
    std::uint8_t month{0};
    std::uint8_t day{0};
    std::uint8_t hour{0};
    std::uint8_t minute{0};
    std::uint8_t second{0};
    bool valid{false};
};

enum class StorageHealth : std::uint8_t {
    unavailable,
    ready,
    degraded,
    full,
    write_failed,
};

struct StorageStatus {
    std::uint64_t available_bytes{0};
    std::uint32_t write_failures{0};
    StorageHealth health{StorageHealth::unavailable};
};

class MonotonicClock {
  public:
    virtual ~MonotonicClock() = default;
    [[nodiscard]] virtual std::int64_t now_us() const noexcept = 0;
};

class GnssInput {
  public:
    virtual ~GnssInput() = default;
    virtual bool try_read(domain::GnssFix& fix) noexcept = 0;
};

class TouchInput {
  public:
    virtual ~TouchInput() = default;
    virtual bool try_read(TouchSample& sample) noexcept = 0;
};

class ImuInput {
  public:
    virtual ~ImuInput() = default;
    virtual bool try_read(ImuSample& sample) noexcept = 0;
};

class RtcSource {
  public:
    virtual ~RtcSource() = default;
    [[nodiscard]] virtual RtcDateTime now() const noexcept = 0;
};

class StorageBackend {
  public:
    virtual ~StorageBackend() = default;
    [[nodiscard]] virtual StorageStatus status() const noexcept = 0;
    virtual bool append(const domain::LogRecord& record) noexcept = 0;
    virtual bool append_batch(const domain::LogRecord* records, std::size_t count) noexcept
    {
        if (count == 0) {
            return true;
        }
        if (records == nullptr || count != 1) {
            return false;
        }
        return append(records[0]);
    }
    virtual bool flush() noexcept
    {
        return true;
    }
};

static_assert(sizeof(TouchSample) <= 16);
static_assert(sizeof(ImuSample) <= 40);

}  // namespace track_timer::board

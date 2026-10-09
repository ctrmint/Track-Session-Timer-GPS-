#pragma once

#include "track_timer/gnss/transport.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::gnss {

// The I2C side of the staged transport (ADR-005): solderless, so it is what bring-up can
// use, and polled, so it is what the pipeline's gap counting exists for.
//
// u-blox call this DDC. It is ordinary I2C with a register convention:
//
//   0xFD / 0xFE   how many bytes are waiting, high byte first
//   0xFF          the stream itself
//
// Reading 0xFF without asking the count first is legal and returns 0xFF padding when the
// receiver has nothing to say. This implementation asks first anyway. Padding is not free:
// every pad byte travels over a bus shared with the touch controller, the IMU and the RTC,
// and then has to be thrown away by the parser, where it would land in discarded_bytes and
// make a healthy link look like a noisy one.

inline constexpr std::uint8_t kDefaultDeviceAddress = 0x42;
inline constexpr std::uint8_t kBytesAvailableRegister = 0xFD;
inline constexpr std::uint8_t kStreamRegister = 0xFF;

// The receiver's DDC buffer holds roughly a kilobyte. A reported count far beyond that is
// not a busy receiver, it is a failed read: a NACKed transfer reads back as 0xFF 0xFF,
// which is 65535 bytes waiting. Believing it would mean a read far larger than anything
// real, on a shared bus, every time the receiver was absent.
inline constexpr std::uint16_t kMaximumPlausibleAvailable = 2048;

// How many consecutive failed transfers before the bus itself is called unhealthy. One
// failure is a glitch on a bus four devices share; three in a row is a wiring fault.
inline constexpr std::uint32_t kUnhealthyAfterConsecutiveErrors = 3;

// The seam that keeps the protocol host-testable. One method, shaped like the ESP-IDF
// call it wraps: write the register address, then read the reply in the same transaction.
class I2cBus {
  public:
    virtual ~I2cBus() = default;

    [[nodiscard]] virtual bool transfer(std::uint8_t device_address, const std::uint8_t* write,
                                        std::size_t write_length, std::uint8_t* read,
                                        std::size_t read_length) noexcept = 0;
};

struct I2cTransportCounters {
    std::uint32_t polls{0};
    std::uint32_t bytes_delivered{0};
    std::uint32_t bus_errors{0};
    std::uint32_t consecutive_bus_errors{0};
    std::uint32_t implausible_counts{0};
    std::uint32_t empty_polls{0};
};

class I2cGnssTransport : public GnssTransport {
  public:
    explicit I2cGnssTransport(I2cBus& bus,
                              std::uint8_t device_address = kDefaultDeviceAddress) noexcept;

    [[nodiscard]] std::size_t read(std::uint8_t* output, std::size_t capacity) noexcept override;
    [[nodiscard]] bool healthy() const noexcept override;

    [[nodiscard]] const I2cTransportCounters& counters() const noexcept;
    void reset() noexcept;

  private:
    [[nodiscard]] std::uint16_t available_bytes() noexcept;
    void note_transfer(bool succeeded) noexcept;

    I2cBus& bus_;
    std::uint8_t device_address_;
    I2cTransportCounters counters_{};
};

// Attaches the receiver to the board's shared I2C bus and returns a transport for it, or
// nullptr if the bus or the device could not be opened. Device-only: the host build has no
// ESP-IDF driver, which is why the protocol above lives behind I2cBus rather than calling
// it directly.
[[nodiscard]] GnssTransport* shared_bus_gnss_transport(
    std::uint8_t device_address = kDefaultDeviceAddress) noexcept;

}  // namespace track_timer::gnss

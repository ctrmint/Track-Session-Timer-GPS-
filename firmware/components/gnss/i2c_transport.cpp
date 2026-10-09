#include "track_timer/gnss/i2c_transport.hpp"

namespace track_timer::gnss {

I2cGnssTransport::I2cGnssTransport(I2cBus& bus, const std::uint8_t device_address) noexcept
    : bus_(bus), device_address_(device_address)
{
}

void I2cGnssTransport::note_transfer(const bool succeeded) noexcept
{
    if (succeeded) {
        counters_.consecutive_bus_errors = 0;
        return;
    }
    ++counters_.bus_errors;
    ++counters_.consecutive_bus_errors;
}

std::uint16_t I2cGnssTransport::available_bytes() noexcept
{
    std::uint8_t reply[2]{};
    const std::uint8_t reg = kBytesAvailableRegister;
    // One transaction: the register address auto-increments, so 0xFD and 0xFE arrive
    // together and cannot be split by another device winning the bus in between.
    if (!bus_.transfer(device_address_, &reg, 1, reply, sizeof(reply))) {
        note_transfer(false);
        return 0;
    }
    note_transfer(true);

    const auto available =
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(reply[0]) << 8U | reply[1]);
    if (available > kMaximumPlausibleAvailable) {
        // Almost certainly 0xFFFF from a transfer that was acknowledged but read nothing
        // real. Counted rather than believed, so an absent receiver shows up as a number
        // instead of a kilobyte of padding on a shared bus.
        ++counters_.implausible_counts;
        return 0;
    }
    return available;
}

std::size_t I2cGnssTransport::read(std::uint8_t* const output, const std::size_t capacity) noexcept
{
    ++counters_.polls;
    if (output == nullptr || capacity == 0) {
        return 0;
    }

    const auto available = available_bytes();
    if (available == 0) {
        ++counters_.empty_polls;
        return 0;
    }

    const auto wanted = static_cast<std::size_t>(available) < capacity
                            ? static_cast<std::size_t>(available)
                            : capacity;
    const std::uint8_t reg = kStreamRegister;
    if (!bus_.transfer(device_address_, &reg, 1, output, wanted)) {
        note_transfer(false);
        return 0;
    }
    note_transfer(true);
    counters_.bytes_delivered += static_cast<std::uint32_t>(wanted);
    // Whatever did not fit stays in the receiver's buffer and is read on the next poll.
    // The pipeline bounds its own work per poll for the same reason, so a backlog drains
    // over several polls rather than in one long one.
    return wanted;
}

bool I2cGnssTransport::healthy() const noexcept
{
    return counters_.consecutive_bus_errors < kUnhealthyAfterConsecutiveErrors;
}

const I2cTransportCounters& I2cGnssTransport::counters() const noexcept { return counters_; }

void I2cGnssTransport::reset() noexcept { counters_ = {}; }

}  // namespace track_timer::gnss

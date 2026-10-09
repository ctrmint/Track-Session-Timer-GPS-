#include "track_timer/board/i2c_bus.hpp"
#include "track_timer/gnss/i2c_transport.hpp"

#include "driver/i2c_master.h"

#include <new>

namespace track_timer::gnss {
namespace {

// 400 kHz, matching the other devices on this bus. At 25 Hz a fix is about 100 bytes, so
// the receiver needs roughly 2,500 B/s of the bus's ~44,400 B/s: about 5.6%. Bandwidth was
// never the reason ADR-005 prefers UART; being polled was.
constexpr std::uint32_t kBusSpeedHz = 400'000;

// Long enough to absorb another device holding the bus, short enough that a GNSS poll
// cannot itself become the thing that stalls the caller.
constexpr int kTimeoutMs = 50;

class EspI2cBus final : public I2cBus {
  public:
    [[nodiscard]] bool attach(const std::uint8_t address) noexcept
    {
        auto* const bus = board::shared_i2c_bus();
        if (bus == nullptr) {
            return false;
        }
        i2c_device_config_t config{};
        config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        config.device_address = address;
        config.scl_speed_hz = kBusSpeedHz;
        if (i2c_master_bus_add_device(bus, &config, &device_) != ESP_OK) {
            device_ = nullptr;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool transfer(std::uint8_t, const std::uint8_t* const write,
                                const std::size_t write_length, std::uint8_t* const read,
                                const std::size_t read_length) noexcept override
    {
        // The address is bound into the device handle when it is added to the bus, so the
        // parameter is not needed here; it stays in the interface because a host fake has
        // no handle to bind it to and must see which device is being addressed.
        if (device_ == nullptr) {
            return false;
        }
        return i2c_master_transmit_receive(device_, write, write_length, read, read_length,
                                           kTimeoutMs) == ESP_OK;
    }

  private:
    i2c_master_dev_handle_t device_{nullptr};
};

// Both live for the life of the process once created. Static storage rather than the heap
// so a transport that fails to attach cannot leak, and so nothing here can fragment the
// allocator the display is sharing.
EspI2cBus bus_storage{};
alignas(I2cGnssTransport) unsigned char transport_storage[sizeof(I2cGnssTransport)]{};
I2cGnssTransport* transport = nullptr;

}  // namespace

GnssTransport* shared_bus_gnss_transport(const std::uint8_t device_address) noexcept
{
    if (transport != nullptr) {
        return transport;
    }
    if (!bus_storage.attach(device_address)) {
        return nullptr;
    }
    transport = new (transport_storage) I2cGnssTransport(bus_storage, device_address);
    return transport;
}

}  // namespace track_timer::gnss

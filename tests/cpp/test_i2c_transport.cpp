#include "track_timer/gnss/i2c_transport.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

using namespace track_timer;

// A receiver on a bus that can misbehave in the ways a real one does: a buffer that
// empties as it is read, a transfer that fails, and the NACK that reads back as 0xFFFF.
class FakeDdcDevice : public gnss::I2cBus {
  public:
    void queue(const std::vector<std::uint8_t>& bytes)
    {
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }

    void fail_next_transfers(const int count) noexcept { failures_ = count; }
    void report_count_override(const int value) noexcept { count_override_ = value; }

    bool transfer(const std::uint8_t device_address, const std::uint8_t* const write,
                  const std::size_t write_length, std::uint8_t* const read,
                  const std::size_t read_length) noexcept override
    {
        last_address_ = device_address;
        ++transfers_;
        if (failures_ > 0) {
            --failures_;
            return false;
        }
        if (write == nullptr || write_length != 1) {
            return false;
        }

        if (write[0] == gnss::kBytesAvailableRegister) {
            if (read_length != 2) {
                return false;
            }
            const auto value = count_override_ >= 0
                                   ? static_cast<std::uint16_t>(count_override_)
                                   : static_cast<std::uint16_t>(buffer_.size());
            read[0] = static_cast<std::uint8_t>(value >> 8U);
            read[1] = static_cast<std::uint8_t>(value & 0xFFU);
            return true;
        }
        if (write[0] == gnss::kStreamRegister) {
            const auto count = std::min(read_length, buffer_.size());
            std::memcpy(read, buffer_.data(), count);
            buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(count));
            // Past the end the receiver sends padding, as the real one does.
            for (std::size_t index = count; index < read_length; ++index) {
                read[index] = 0xFF;
            }
            return true;
        }
        return false;
    }

    [[nodiscard]] std::size_t pending() const noexcept { return buffer_.size(); }
    [[nodiscard]] std::uint8_t last_address() const noexcept { return last_address_; }
    [[nodiscard]] int transfers() const noexcept { return transfers_; }

  private:
    std::vector<std::uint8_t> buffer_{};
    int failures_{0};
    int count_override_{-1};
    int transfers_{0};
    std::uint8_t last_address_{0};
};

void the_count_is_read_before_the_stream()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    device.queue({1, 2, 3, 4, 5});

    std::uint8_t out[32]{};
    const auto count = transport.read(out, sizeof(out));

    assert(count == 5);
    assert(out[0] == 1 && out[4] == 5);
    // Exactly two transactions: one for the count, one for the stream. Nothing is read
    // speculatively, so no padding crosses a bus shared with touch, the IMU and the RTC.
    assert(device.transfers() == 2);
    assert(device.last_address() == gnss::kDefaultDeviceAddress);
    assert(transport.counters().bytes_delivered == 5);
    assert(transport.healthy());
}

// Zero waiting is the ordinary case between epochs, not an error, and it must cost one
// transaction rather than a wasted stream read.
void an_empty_receiver_costs_one_transaction_and_no_bytes()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};

    std::uint8_t out[32]{};
    assert(transport.read(out, sizeof(out)) == 0);
    assert(device.transfers() == 1);
    assert(transport.counters().empty_polls == 1);
    assert(transport.healthy());
}

// The failure that makes believing the count dangerous: a NACKed read comes back as all
// ones, which says 65535 bytes are waiting.
void an_implausible_count_is_counted_rather_than_believed()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    device.report_count_override(0xFFFF);

    std::uint8_t out[32]{};
    assert(transport.read(out, sizeof(out)) == 0);
    assert(transport.counters().implausible_counts == 1);
    // No stream read was attempted on the strength of it.
    assert(device.transfers() == 1);
    assert(transport.counters().bytes_delivered == 0);
}

// More waiting than the caller asked for leaves the remainder in the receiver, where the
// next poll finds it. The pipeline bounds its own work for the same reason.
void a_short_buffer_leaves_the_remainder_for_the_next_poll()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    device.queue(std::vector<std::uint8_t>(100, 0xAB));

    std::uint8_t out[32]{};
    assert(transport.read(out, sizeof(out)) == 32);
    assert(device.pending() == 68);
    assert(transport.read(out, sizeof(out)) == 32);
    assert(device.pending() == 36);
}

// One failure on a bus four devices share is a glitch. Three in a row is a wiring fault,
// and that is a different thing from a receiver with nothing to say - the two must not
// arrive at the pipeline as one.
void repeated_bus_errors_make_the_transport_unhealthy()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    std::uint8_t out[32]{};

    device.fail_next_transfers(1);
    assert(transport.read(out, sizeof(out)) == 0);
    assert(transport.healthy());  // a single glitch is survivable

    device.fail_next_transfers(static_cast<int>(gnss::kUnhealthyAfterConsecutiveErrors));
    for (std::uint32_t attempt = 0; attempt < gnss::kUnhealthyAfterConsecutiveErrors; ++attempt) {
        assert(transport.read(out, sizeof(out)) == 0);
    }
    assert(!transport.healthy());
    assert(transport.counters().bus_errors == gnss::kUnhealthyAfterConsecutiveErrors + 1);
}

void a_good_transfer_clears_the_error_run()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    std::uint8_t out[32]{};

    device.fail_next_transfers(static_cast<int>(gnss::kUnhealthyAfterConsecutiveErrors));
    for (std::uint32_t attempt = 0; attempt < gnss::kUnhealthyAfterConsecutiveErrors; ++attempt) {
        assert(transport.read(out, sizeof(out)) == 0);
    }
    assert(!transport.healthy());

    device.queue({7, 7, 7});
    assert(transport.read(out, sizeof(out)) == 3);
    assert(transport.healthy());
    assert(transport.counters().consecutive_bus_errors == 0);
}

// A stream read that fails after the count succeeded must deliver nothing rather than
// whatever was already in the caller's buffer.
void a_failed_stream_read_delivers_nothing()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    device.queue({1, 2, 3});

    std::uint8_t out[32]{};
    std::memset(out, 0x5A, sizeof(out));
    device.fail_next_transfers(2);  // the count read, then the retry's count read
    assert(transport.read(out, sizeof(out)) == 0);
    assert(transport.counters().bytes_delivered == 0);
}

void a_zero_capacity_request_touches_the_bus_not_at_all()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    device.queue({1, 2, 3});

    std::uint8_t out[1]{};
    assert(transport.read(out, 0) == 0);
    assert(transport.read(nullptr, 8) == 0);
    assert(device.transfers() == 0);
}

void the_device_address_is_configurable()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device, 0x43};
    device.queue({9});

    std::uint8_t out[8]{};
    assert(transport.read(out, sizeof(out)) == 1);
    assert(device.last_address() == 0x43);
}

void a_reset_clears_the_counters()
{
    FakeDdcDevice device{};
    gnss::I2cGnssTransport transport{device};
    device.queue({1, 2, 3});
    std::uint8_t out[8]{};
    (void)transport.read(out, sizeof(out));
    assert(transport.counters().polls == 1);

    transport.reset();
    assert(transport.counters().polls == 0);
    assert(transport.counters().bytes_delivered == 0);
    assert(transport.healthy());
}

}  // namespace

int main()
{
    the_count_is_read_before_the_stream();
    an_empty_receiver_costs_one_transaction_and_no_bytes();
    an_implausible_count_is_counted_rather_than_believed();
    a_short_buffer_leaves_the_remainder_for_the_next_poll();
    repeated_bus_errors_make_the_transport_unhealthy();
    a_good_transfer_clears_the_error_run();
    a_failed_stream_read_delivers_nothing();
    a_zero_capacity_request_touches_the_bus_not_at_all();
    the_device_address_is_configurable();
    a_reset_clears_the_counters();

    std::cout << "I2C DDC transport: the count is read before the stream, an implausible "
                 "count is refused, and a failing bus is distinguishable from a quiet "
                 "receiver\n";
    return 0;
}

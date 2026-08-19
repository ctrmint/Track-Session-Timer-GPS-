#include "track_timer/imu/qmi8658.hpp"

#include "esp_timer.h"
#include "track_timer/board/i2c_bus.hpp"

#include <cstring>

namespace track_timer::imu {
namespace {

// The part answers on 0x6B when SA0 is high and 0x6A when low; which one depends on the
// board, so both are probed rather than assumed.
constexpr std::uint8_t kAddressPrimary = 0x6B;
constexpr std::uint8_t kAddressSecondary = 0x6A;
constexpr std::uint8_t kWhoAmIValue = 0x05;
constexpr std::uint32_t kBusSpeedHz = 400'000;
constexpr int kTimeoutMs = 100;

enum Register : std::uint8_t {
    who_am_i = 0x00,
    ctrl1 = 0x02,
    ctrl2 = 0x03,
    ctrl3 = 0x04,
    ctrl7 = 0x08,
    ax_l = 0x35,
};

// CTRL2: accelerometer +/-8 g at 235 Hz ODR.
constexpr std::uint8_t kCtrl2AccelConfig = 0x24;
// CTRL3: gyroscope 512 dps at 235 Hz ODR.
constexpr std::uint8_t kCtrl3GyroConfig = 0x54;
// CTRL7: enable accelerometer and gyroscope.
constexpr std::uint8_t kCtrl7EnableBoth = 0x03;
// CTRL1: auto-increment register address on read, which is what makes the twelve data
// bytes readable in one transaction rather than twelve.
constexpr std::uint8_t kCtrl1AddressAutoIncrement = 0x40;

// Least-significant bits per unit, from the configured full scales.
constexpr float kAccelLsbPerG = 4096.0F;   // +/-8 g over a signed 16-bit range
constexpr float kGyroLsbPerDps = 64.0F;    // 512 dps over a signed 16-bit range
constexpr float kStandardGravityMps2 = 9.80665F;

i2c_master_dev_handle_t device = nullptr;
bool started = false;

[[nodiscard]] bool write_register(const std::uint8_t reg, const std::uint8_t value) noexcept
{
    const std::uint8_t payload[] = {reg, value};
    return i2c_master_transmit(device, payload, sizeof(payload), kTimeoutMs) == ESP_OK;
}

[[nodiscard]] bool read_registers(const std::uint8_t reg, std::uint8_t* out,
                                  const std::size_t count) noexcept
{
    return i2c_master_transmit_receive(device, &reg, 1, out, count, kTimeoutMs) == ESP_OK;
}

[[nodiscard]] std::int16_t to_int16(const std::uint8_t low, const std::uint8_t high) noexcept
{
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(high) << 8U | low);
}

[[nodiscard]] bool attach(i2c_master_bus_handle_t bus, const std::uint8_t address) noexcept
{
    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = address;
    config.scl_speed_hz = kBusSpeedHz;
    if (i2c_master_bus_add_device(bus, &config, &device) != ESP_OK) {
        device = nullptr;
        return false;
    }
    return true;
}

}  // namespace

ImuStartResult start() noexcept
{
    if (started) {
        return ImuStartResult::already_started;
    }
    auto* const bus = board::shared_i2c_bus();
    if (bus == nullptr) {
        return ImuStartResult::bus_failed;
    }

    bool detected = false;
    bool answered = false;
    for (const auto address : {kAddressPrimary, kAddressSecondary}) {
        if (!attach(bus, address)) {
            continue;
        }
        std::uint8_t identity = 0;
        if (read_registers(Register::who_am_i, &identity, 1)) {
            answered = true;
            if (identity == kWhoAmIValue) {
                detected = true;
                break;
            }
        }
        (void)i2c_master_bus_rm_device(device);
        device = nullptr;
    }
    if (!detected) {
        return answered ? ImuStartResult::wrong_device : ImuStartResult::not_detected;
    }

    if (!write_register(Register::ctrl1, kCtrl1AddressAutoIncrement) ||
        !write_register(Register::ctrl2, kCtrl2AccelConfig) ||
        !write_register(Register::ctrl3, kCtrl3GyroConfig) ||
        !write_register(Register::ctrl7, kCtrl7EnableBoth)) {
        (void)i2c_master_bus_rm_device(device);
        device = nullptr;
        return ImuStartResult::configure_failed;
    }

    started = true;
    return ImuStartResult::ready;
}

bool read(board::ImuSample& sample) noexcept
{
    sample = {};
    if (!started || device == nullptr) {
        return false;
    }
    // Twelve bytes in one transaction: ax, ay, az, gx, gy, gz, each little-endian.
    std::uint8_t raw[12]{};
    if (!read_registers(Register::ax_l, raw, sizeof(raw))) {
        return false;
    }

    sample.monotonic_us = esp_timer_get_time();
    sample.acceleration_x_mps2 =
        static_cast<float>(to_int16(raw[0], raw[1])) / kAccelLsbPerG * kStandardGravityMps2;
    sample.acceleration_y_mps2 =
        static_cast<float>(to_int16(raw[2], raw[3])) / kAccelLsbPerG * kStandardGravityMps2;
    sample.acceleration_z_mps2 =
        static_cast<float>(to_int16(raw[4], raw[5])) / kAccelLsbPerG * kStandardGravityMps2;
    sample.angular_rate_x_dps = static_cast<float>(to_int16(raw[6], raw[7])) / kGyroLsbPerDps;
    sample.angular_rate_y_dps = static_cast<float>(to_int16(raw[8], raw[9])) / kGyroLsbPerDps;
    sample.angular_rate_z_dps =
        static_cast<float>(to_int16(raw[10], raw[11])) / kGyroLsbPerDps;
    sample.valid = true;
    return true;
}

bool running() noexcept { return started; }

const char* imu_start_result_name(const ImuStartResult result) noexcept
{
    switch (result) {
    case ImuStartResult::ready:
        return "ready";
    case ImuStartResult::already_started:
        return "already-started";
    case ImuStartResult::bus_failed:
        return "bus-failed";
    case ImuStartResult::not_detected:
        return "not-detected";
    case ImuStartResult::wrong_device:
        return "wrong-device";
    case ImuStartResult::configure_failed:
        return "configure-failed";
    }
    return "unknown";
}

}  // namespace track_timer::imu

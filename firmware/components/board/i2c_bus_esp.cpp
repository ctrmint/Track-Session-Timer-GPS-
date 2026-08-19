#include "track_timer/board/i2c_bus.hpp"

namespace track_timer::board {
namespace {

constexpr gpio_num_t kPinSda = GPIO_NUM_47;
constexpr gpio_num_t kPinScl = GPIO_NUM_48;

i2c_master_bus_handle_t bus = nullptr;
bool attempted = false;

}  // namespace

i2c_master_bus_handle_t shared_i2c_bus() noexcept
{
    if (attempted) {
        return bus;
    }
    attempted = true;

    i2c_master_bus_config_t config{};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = kPinSda;
    config.scl_io_num = kPinScl;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&config, &bus) != ESP_OK) {
        bus = nullptr;
    }
    return bus;
}

}  // namespace track_timer::board

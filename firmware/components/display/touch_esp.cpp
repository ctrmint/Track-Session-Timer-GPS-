#include "track_timer/display/panel.hpp"

#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_ft5x06.h"

#include <lvgl.h>

namespace track_timer::display {
namespace {

// Shared I2C bus: touch, IMU, RTC and the IO expander all live here.
constexpr gpio_num_t kPinSda = GPIO_NUM_47;
constexpr gpio_num_t kPinScl = GPIO_NUM_48;
constexpr gpio_num_t kPinTouchReset = GPIO_NUM_3;
constexpr std::uint32_t kBusSpeedHz = 300'000;

// FT5x06 register interface: address only, no control phase.
constexpr std::uint32_t kTouchAddress = 0x38;

i2c_master_bus_handle_t i2c_bus = nullptr;
esp_lcd_touch_handle_t touch_handle = nullptr;
lv_indev_t* touch_indev = nullptr;

void read_touch(lv_indev_t*, lv_indev_data_t* data) noexcept
{
    data->state = LV_INDEV_STATE_RELEASED;
    if (touch_handle == nullptr) {
        return;
    }
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t strength = 0;
    std::uint8_t count = 0;
    (void)esp_lcd_touch_read_data(touch_handle);
    if (esp_lcd_touch_get_coordinates(touch_handle, &x, &y, &strength, &count, 1) &&
        count > 0) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
}

}  // namespace

TouchResult start_touch() noexcept
{
    if (touch_indev != nullptr) {
        return TouchResult::already_started;
    }

    i2c_master_bus_config_t bus_config{};
    bus_config.i2c_port = I2C_NUM_0;
    bus_config.sda_io_num = kPinSda;
    bus_config.scl_io_num = kPinScl;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    if (i2c_new_master_bus(&bus_config, &i2c_bus) != ESP_OK) {
        return TouchResult::bus_failed;
    }

    // Field-by-field rather than ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG(), which is a C
    // designated initialiser and does not compile clean as C++ under -Werror.
    esp_lcd_panel_io_i2c_config_t io_config{};
    io_config.dev_addr = kTouchAddress;
    io_config.scl_speed_hz = kBusSpeedHz;
    io_config.control_phase_bytes = 1;
    io_config.dc_bit_offset = 0;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 0;
    io_config.flags.disable_control_phase = 1;

    esp_lcd_panel_io_handle_t touch_io = nullptr;
    if (esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &touch_io) != ESP_OK) {
        return TouchResult::bus_failed;
    }

    // Maxima are given in the panel's native portrait axes; swap_xy/mirror_y then map
    // them onto the 600 x 450 landscape frame the display is rotated into.
    esp_lcd_touch_config_t touch_config{};
    touch_config.x_max = kPanelHeightPx - 1;
    touch_config.y_max = kPanelWidthPx - 1;
    touch_config.rst_gpio_num = kPinTouchReset;
    touch_config.int_gpio_num = GPIO_NUM_NC;
    touch_config.levels.reset = 0;
    touch_config.levels.interrupt = 0;
    touch_config.flags.swap_xy = 1;
    touch_config.flags.mirror_x = 0;
    touch_config.flags.mirror_y = 1;
    if (esp_lcd_touch_new_i2c_ft5x06(touch_io, &touch_config, &touch_handle) != ESP_OK) {
        return TouchResult::controller_failed;
    }

    touch_indev = lv_indev_create();
    if (touch_indev == nullptr) {
        return TouchResult::lvgl_failed;
    }
    lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_indev, read_touch);
    return TouchResult::ready;
}

const char* touch_result_name(const TouchResult result) noexcept
{
    switch (result) {
    case TouchResult::ready:
        return "ready";
    case TouchResult::already_started:
        return "already-started";
    case TouchResult::bus_failed:
        return "bus-failed";
    case TouchResult::controller_failed:
        return "controller-failed";
    case TouchResult::lvgl_failed:
        return "lvgl-failed";
    }
    return "unknown";
}

}  // namespace track_timer::display

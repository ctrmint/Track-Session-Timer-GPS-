#include "track_timer/display/panel.hpp"

#include "track_timer/board/i2c_bus.hpp"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_ft5x06.h"

#include <lvgl.h>

#include <cstdint>

namespace track_timer::display {
namespace {

constexpr gpio_num_t kPinTouchReset = GPIO_NUM_3;
constexpr std::uint32_t kBusSpeedHz = 300'000;

// FT5x06 register interface: address only, no control phase.
constexpr std::uint32_t kTouchAddress = 0x38;

esp_lcd_touch_handle_t touch_handle = nullptr;
lv_indev_t* touch_indev = nullptr;
std::uint32_t read_count = 0;
std::uint32_t press_count = 0;
std::uint32_t raw_report_count = 0;   // controller said "finger", before any mapping
std::int32_t last_x = -1;
std::int32_t last_y = -1;

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
    ++read_count;
    (void)esp_lcd_touch_read_data(touch_handle);
    const auto got = esp_lcd_touch_get_coordinates(touch_handle, &x, &y, &strength,
                                                   &count, 1);
    if (got) {
        ++raw_report_count;
    }
    if (got && count > 0) {
        ++press_count;
        // Recorded rather than logged: a cumulative last-value survives until the next
        // capture, where a momentary log line has to be caught as it happens.
        last_x = x;
        last_y = y;
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

    auto* const i2c_bus = board::shared_i2c_bus();
    if (i2c_bus == nullptr) {
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

TouchCounters touch_counters() noexcept
{
    return {read_count, press_count, raw_report_count, last_x, last_y};
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

#include "track_timer/display/panel.hpp"

#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_sh8601.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <lvgl.h>

namespace track_timer::display {
namespace {

constexpr auto kLcdHost = SPI2_HOST;

// Waveshare ESP32-S3-Touch-AMOLED-2.41 pin map (wiki pin table).
constexpr gpio_num_t kPinCs = GPIO_NUM_9;
constexpr gpio_num_t kPinPclk = GPIO_NUM_10;
constexpr gpio_num_t kPinData0 = GPIO_NUM_11;
constexpr gpio_num_t kPinData1 = GPIO_NUM_12;
constexpr gpio_num_t kPinData2 = GPIO_NUM_13;
constexpr gpio_num_t kPinData3 = GPIO_NUM_14;
constexpr gpio_num_t kPinReset = GPIO_NUM_21;

constexpr int kBitsPerPixel = 16;
constexpr std::uint32_t kLvglTickPeriodMs = 2;
// NOTE: raise this before track selection is wired to catalog::apply_track. Parsing a
// definition needs roughly 10 KB of stack on top of whatever LVGL is using.
constexpr std::uint32_t kLvglTaskStackBytes = 8 * 1024;
constexpr UBaseType_t kLvglTaskPriority = 2;
constexpr std::uint32_t kLvglMaxDelayMs = 500;
constexpr std::uint32_t kLvglMinDelayMs = 1;

// RM690B0 power-on sequence, transcribed from the Waveshare 09_LVGL_Test demo.
// 0x2A/0x2B describe the native 450 x 600 window; the panel's active area starts at
// column 16, which is why the column start is 0x0010. 0x36 = 0x30 rotates the panel
// into the 600 x 450 landscape orientation the UI is designed for.
const sh8601_lcd_init_cmd_t kInitCommands[] = {
    {0xFE, (const uint8_t[]){0x20}, 1, 0},
    {0x26, (const uint8_t[]){0x0A}, 1, 0},
    {0x24, (const uint8_t[]){0x80}, 1, 0},
    {0xFE, (const uint8_t[]){0x00}, 1, 0},
    {0x3A, (const uint8_t[]){0x55}, 1, 0},   // 16 bit/pixel
    {0xC2, (const uint8_t[]){0x00}, 1, 10},
    {0x35, (const uint8_t[]){0x00}, 0, 0},   // tearing effect on
    {0x51, (const uint8_t[]){0x00}, 1, 10},  // brightness off while initialising
    {0x11, (const uint8_t[]){0x00}, 0, 80},  // sleep out
    {0x2A, (const uint8_t[]){0x00, 0x10, 0x01, 0xD1}, 4, 0},
    {0x2B, (const uint8_t[]){0x00, 0x00, 0x02, 0x57}, 4, 0},
    {0x29, (const uint8_t[]){0x00}, 0, 10},  // display on
    {0x36, (const uint8_t[]){0x30}, 1, 10},  // landscape
    {0x51, (const uint8_t[]){0xFF}, 1, 0},   // brightness full
};

lv_display_t* lvgl_display = nullptr;
esp_lcd_panel_io_handle_t io_handle = nullptr;
esp_lcd_panel_handle_t panel_handle = nullptr;
SemaphoreHandle_t lvgl_mutex = nullptr;
esp_timer_handle_t tick_timer = nullptr;
bool started = false;

bool flush_ready(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) noexcept
{
    if (lvgl_display != nullptr) {
        lv_display_flush_ready(lvgl_display);
    }
    return false;
}

void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels) noexcept
{
    const auto width = area->x2 - area->x1 + 1;
    const auto height = area->y2 - area->y1 + 1;
    // The panel expects big-endian RGB565 on the wire; LVGL renders little-endian.
    lv_draw_sw_rgb565_swap(pixels, static_cast<std::uint32_t>(width * height));
    if (esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1,
                                  area->y2 + 1, pixels) != ESP_OK) {
        lv_display_flush_ready(display);
    }
}

// The RM690B0 addresses its frame memory in 2-pixel units, and esp_lcd_sh8601 passes the
// window straight through without enforcing that. An odd column start or an odd width
// therefore makes the panel interpret the pixel stream with the wrong stride, which
// renders as horizontal bands each shifted further than the last.
//
// A full-screen redraw happens to satisfy the alignment (0..599), which is why the first
// screens looked correct and only partial redraws broke.
void round_invalidated_area(lv_event_t* event) noexcept
{
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(event));
    if (area == nullptr) {
        return;
    }
    area->x1 &= ~1;              // start on an even column
    area->x2 |= 1;               // end on an odd column, so the width is even
    area->y1 &= ~1;
    area->y2 |= 1;
    if (area->x2 >= kPanelWidthPx) {
        area->x2 = kPanelWidthPx - 1;
    }
    if (area->y2 >= kPanelHeightPx) {
        area->y2 = kPanelHeightPx - 1;
    }
}

void tick(void*) noexcept { lv_tick_inc(kLvglTickPeriodMs); }

void lvgl_task(void*) noexcept
{
    auto delay_ms = kLvglMinDelayMs;
    for (;;) {
        if (lock(portMAX_DELAY)) {
            delay_ms = lv_timer_handler();
            unlock();
        }
        if (delay_ms > kLvglMaxDelayMs) {
            delay_ms = kLvglMaxDelayMs;
        }
        else if (delay_ms < kLvglMinDelayMs) {
            delay_ms = kLvglMinDelayMs;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

std::uint8_t* allocate_draw_buffer() noexcept
{
    // PSRAM first, per docs/UI_FOUNDATION.md. Falling back to internal DMA memory is
    // deliberate and loud rather than silent: two full frames must never land in
    // internal RAM, and 48 KB partial buffers are small enough to be safe there.
    auto* buffer = static_cast<std::uint8_t*>(
        heap_caps_malloc(kDrawBufferBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (buffer == nullptr) {
        buffer = static_cast<std::uint8_t*>(
            heap_caps_malloc(kDrawBufferBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    }
    return buffer;
}

}  // namespace

PanelResult start_panel() noexcept
{
    if (started) {
        return PanelResult::already_started;
    }

    // Field-by-field rather than SH8601_PANEL_BUS_QSPI_CONFIG / _IO_QSPI_CONFIG: the
    // vendor macros are C designated initialisers and trip -Werror on field order and
    // missing initialisers when compiled as C++. Values match the macros exactly.
    spi_bus_config_t bus_config{};
    bus_config.sclk_io_num = kPinPclk;
    bus_config.data0_io_num = kPinData0;
    bus_config.data1_io_num = kPinData1;
    bus_config.data2_io_num = kPinData2;
    bus_config.data3_io_num = kPinData3;
    bus_config.max_transfer_sz = kPanelWidthPx * kDrawBufferLines * kBitsPerPixel / 8;
    if (spi_bus_initialize(kLcdHost, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        return PanelResult::bus_failed;
    }

    esp_lcd_panel_io_spi_config_t io_config{};
    io_config.cs_gpio_num = kPinCs;
    io_config.dc_gpio_num = GPIO_NUM_NC;  // QSPI carries D/C in the command word
    io_config.spi_mode = 0;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = flush_ready;
    io_config.user_ctx = nullptr;
    io_config.lcd_cmd_bits = 32;
    io_config.lcd_param_bits = 8;
    io_config.flags.quad_mode = true;

    const auto bus = static_cast<esp_lcd_spi_bus_handle_t>(kLcdHost);
    if (esp_lcd_new_panel_io_spi(bus, &io_config, &io_handle) != ESP_OK) {
        return PanelResult::panel_io_failed;
    }

    sh8601_vendor_config_t vendor_config{};
    vendor_config.init_cmds = kInitCommands;
    vendor_config.init_cmds_size = sizeof(kInitCommands) / sizeof(kInitCommands[0]);
    vendor_config.flags.use_qspi_interface = 1;

    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = kPinReset;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = kBitsPerPixel;
    panel_config.vendor_config = &vendor_config;

    if (esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle) != ESP_OK ||
        esp_lcd_panel_reset(panel_handle) != ESP_OK ||
        esp_lcd_panel_init(panel_handle) != ESP_OK ||
        esp_lcd_panel_disp_on_off(panel_handle, true) != ESP_OK) {
        return PanelResult::panel_failed;
    }

    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mutex == nullptr) {
        return PanelResult::lvgl_failed;
    }

    lv_init();
    lvgl_display = lv_display_create(kPanelWidthPx, kPanelHeightPx);
    if (lvgl_display == nullptr) {
        return PanelResult::lvgl_failed;
    }

    auto* first = allocate_draw_buffer();
    auto* second = allocate_draw_buffer();
    if (first == nullptr || second == nullptr) {
        return PanelResult::buffer_allocation_failed;
    }

    lv_display_set_color_format(lvgl_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(lvgl_display, first, second, kDrawBufferBytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvgl_display, flush);
    lv_display_add_event_cb(lvgl_display, round_invalidated_area,
                            LV_EVENT_INVALIDATE_AREA, nullptr);

    const esp_timer_create_args_t tick_args{tick, nullptr, ESP_TIMER_TASK, "lvgl_tick",
                                            true};
    if (esp_timer_create(&tick_args, &tick_timer) != ESP_OK ||
        esp_timer_start_periodic(tick_timer, kLvglTickPeriodMs * 1000) != ESP_OK) {
        return PanelResult::lvgl_failed;
    }

    if (xTaskCreate(lvgl_task, "lvgl", kLvglTaskStackBytes, nullptr, kLvglTaskPriority,
                    nullptr) != pdPASS) {
        return PanelResult::task_failed;
    }

    started = true;
    return PanelResult::ready;
}

bool panel_ready() noexcept { return started; }

bool lock(const std::uint32_t timeout_ms) noexcept
{
    if (lvgl_mutex == nullptr) {
        return false;
    }
    const auto ticks =
        timeout_ms == portMAX_DELAY ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mutex, ticks) == pdTRUE;
}

void unlock() noexcept
{
    if (lvgl_mutex != nullptr) {
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}

void set_brightness(const std::uint8_t level) noexcept
{
    if (io_handle == nullptr) {
        return;
    }
    const std::uint8_t payload[] = {level};
    (void)esp_lcd_panel_io_tx_param(io_handle, 0x51, payload, sizeof(payload));
}

const char* panel_result_name(const PanelResult result) noexcept
{
    switch (result) {
    case PanelResult::ready:
        return "ready";
    case PanelResult::already_started:
        return "already-started";
    case PanelResult::bus_failed:
        return "bus-failed";
    case PanelResult::panel_io_failed:
        return "panel-io-failed";
    case PanelResult::panel_failed:
        return "panel-failed";
    case PanelResult::buffer_allocation_failed:
        return "buffer-allocation-failed";
    case PanelResult::lvgl_failed:
        return "lvgl-failed";
    case PanelResult::task_failed:
        return "task-failed";
    }
    return "unknown";
}

}  // namespace track_timer::display

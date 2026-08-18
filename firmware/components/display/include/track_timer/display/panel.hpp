#pragma once

#include <cstddef>
#include <cstdint>

// Waveshare ESP32-S3-Touch-AMOLED-2.41-B panel bring-up.
//
// The panel is a Raydium RM690B0 AMOLED on a QSPI interface. Its command set is
// compatible with the esp_lcd_sh8601 driver, which is how the vendor drives it, so
// the RM690B0 register sequence is supplied as a vendor init table rather than
// implemented as a separate esp_lcd panel driver.
//
// Pin assignment and the init sequence are transcribed from the Waveshare wiki and
// the vendor 09_LVGL_Test demo; see docs/HARDWARE_INTEGRATION.md.

namespace track_timer::display {

// Landscape orientation, matching docs/UI_FOUNDATION.md. The panel is natively
// 450 x 600 portrait and is rotated by MADCTL in the init table.
inline constexpr std::int32_t kPanelWidthPx = 600;
inline constexpr std::int32_t kPanelHeightPx = 450;

// Two partial buffers of 600 x 40 RGB565, the budget fixed by docs/UI_FOUNDATION.md.
inline constexpr std::int32_t kDrawBufferLines = 40;
inline constexpr std::size_t kDrawBufferBytes =
    static_cast<std::size_t>(kPanelWidthPx) * kDrawBufferLines * 2U;
inline constexpr std::size_t kDrawBufferTotalBytes = 2U * kDrawBufferBytes;

static_assert(kDrawBufferBytes == 48'000);
static_assert(kDrawBufferTotalBytes == 96'000);

enum class PanelResult : std::uint8_t {
    ready,
    already_started,
    bus_failed,
    panel_io_failed,
    panel_failed,
    buffer_allocation_failed,
    lvgl_failed,
    task_failed,
};

// Brings up the QSPI bus, the RM690B0, the LVGL display and the LVGL service task.
// Safe to call once; a second call reports already_started.
[[nodiscard]] PanelResult start_panel() noexcept;

[[nodiscard]] bool panel_ready() noexcept;

// LVGL is not thread safe. Anything touching lv_* from outside the service task must
// hold this lock. Returns false if the lock could not be taken within the timeout.
[[nodiscard]] bool lock(std::uint32_t timeout_ms) noexcept;
void unlock() noexcept;

// 0 is off, 255 is the panel maximum.
void set_brightness(std::uint8_t level) noexcept;

[[nodiscard]] const char* panel_result_name(PanelResult result) noexcept;

enum class TouchResult : std::uint8_t {
    ready,
    already_started,
    bus_failed,
    controller_failed,
    lvgl_failed,
};

// FT6336 capacitive controller on the shared I2C bus (GPIO47/48), registered as an
// LVGL pointer input device. Requires start_panel() to have succeeded first.
[[nodiscard]] TouchResult start_touch() noexcept;
[[nodiscard]] const char* touch_result_name(TouchResult result) noexcept;

}  // namespace track_timer::display

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

struct DisplayBufferStrategy {
    std::uint16_t width_px;
    std::uint16_t height_px;
    std::uint16_t lines_per_buffer;
    std::uint8_t bytes_per_pixel;
    std::uint8_t buffer_count;
    bool prefer_external_ram;

    [[nodiscard]] constexpr std::size_t bytes_per_buffer() const noexcept
    {
        return static_cast<std::size_t>(width_px) * lines_per_buffer * bytes_per_pixel;
    }

    [[nodiscard]] constexpr std::size_t total_bytes() const noexcept
    {
        return bytes_per_buffer() * buffer_count;
    }
};

inline constexpr DisplayBufferStrategy kDeviceDisplayBuffers{
    600,
    450,
    40,
    2,
    2,
    true,
};

namespace spacing {

inline constexpr std::int32_t x_small = 8;
inline constexpr std::int32_t small = 12;
inline constexpr std::int32_t medium = 20;
inline constexpr std::int32_t large = 30;
inline constexpr std::int32_t touch_target = 56;

}  // namespace spacing

namespace color {

inline constexpr std::uint32_t background = 0x050505;
inline constexpr std::uint32_t surface = 0x202020;
inline constexpr std::uint32_t text_primary = 0xFFFFFF;
inline constexpr std::uint32_t text_secondary = 0xA7A7A7;
inline constexpr std::uint32_t positive = 0x2E7D32;
inline constexpr std::uint32_t positive_bright = 0x76FF9A;
inline constexpr std::uint32_t caution = 0xFBC02D;
inline constexpr std::uint32_t caution_bright = 0xFFD54F;
inline constexpr std::uint32_t warning = 0xF57C00;
inline constexpr std::uint32_t critical = 0xD32F2F;
inline constexpr std::uint32_t critical_bright = 0xFF5252;
inline constexpr std::uint32_t overtime = 0x7E57C2;
inline constexpr std::uint32_t logging = 0x1B5E20;
inline constexpr std::uint32_t logging_unavailable = 0xB71C1C;

}  // namespace color

[[nodiscard]] double contrast_ratio(std::uint32_t first_rgb,
                                    std::uint32_t second_rgb) noexcept;
[[nodiscard]] std::uint32_t contrast_text_rgb(std::uint32_t background_rgb) noexcept;

struct RenderMetrics {
    std::uint32_t frame_count{0};
    std::uint64_t total_render_us{0};
    std::uint32_t maximum_render_us{0};
    std::size_t maximum_lvgl_bytes{0};

    [[nodiscard]] constexpr std::uint32_t average_render_us() const noexcept
    {
        return frame_count == 0 ? 0
                                : static_cast<std::uint32_t>(total_render_us / frame_count);
    }
};

class RenderProfiler {
  public:
    void record(std::uint32_t render_us, std::size_t lvgl_bytes) noexcept;
    [[nodiscard]] RenderMetrics metrics() const noexcept;

  private:
    RenderMetrics metrics_{};
};

static_assert(kDeviceDisplayBuffers.bytes_per_buffer() == 48'000);
static_assert(kDeviceDisplayBuffers.total_bytes() == 96'000);
static_assert(kDeviceDisplayBuffers.lines_per_buffer < kDeviceDisplayBuffers.height_px);
static_assert(std::is_trivially_copyable_v<DisplayBufferStrategy>);
static_assert(std::is_trivially_copyable_v<RenderMetrics>);

}  // namespace track_timer::ui

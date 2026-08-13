#include "track_timer/ui/foundation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace track_timer::ui {
namespace {

double linear_channel(const std::uint32_t channel) noexcept
{
    const auto normalized = static_cast<double>(channel) / 255.0;
    return normalized <= 0.04045 ? normalized / 12.92
                                 : std::pow((normalized + 0.055) / 1.055, 2.4);
}

double relative_luminance(const std::uint32_t rgb) noexcept
{
    return 0.2126 * linear_channel((rgb >> 16U) & 0xFFU) +
           0.7152 * linear_channel((rgb >> 8U) & 0xFFU) +
           0.0722 * linear_channel(rgb & 0xFFU);
}

}  // namespace

double contrast_ratio(const std::uint32_t first_rgb, const std::uint32_t second_rgb) noexcept
{
    const auto first = relative_luminance(first_rgb);
    const auto second = relative_luminance(second_rgb);
    const auto lighter = std::max(first, second);
    const auto darker = std::min(first, second);
    return (lighter + 0.05) / (darker + 0.05);
}

std::uint32_t contrast_text_rgb(const std::uint32_t background_rgb) noexcept
{
    return contrast_ratio(background_rgb, color::text_primary) >=
                   contrast_ratio(background_rgb, 0x000000)
               ? color::text_primary
               : 0x000000;
}

void RenderProfiler::record(const std::uint32_t render_us, const std::size_t lvgl_bytes) noexcept
{
    if (metrics_.frame_count < std::numeric_limits<std::uint32_t>::max()) {
        ++metrics_.frame_count;
    }
    if (metrics_.total_render_us <=
        std::numeric_limits<std::uint64_t>::max() - render_us) {
        metrics_.total_render_us += render_us;
    }
    metrics_.maximum_render_us = std::max(metrics_.maximum_render_us, render_us);
    metrics_.maximum_lvgl_bytes = std::max(metrics_.maximum_lvgl_bytes, lvgl_bytes);
}

RenderMetrics RenderProfiler::metrics() const noexcept
{
    return metrics_;
}

}  // namespace track_timer::ui

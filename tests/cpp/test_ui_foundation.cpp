#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/presenter.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <type_traits>

int main()
{
    using namespace track_timer;

    assert(ui::kDeviceDisplayBuffers.width_px == 600);
    assert(ui::kDeviceDisplayBuffers.height_px == 450);
    assert(ui::kDeviceDisplayBuffers.bytes_per_buffer() == 48'000);
    assert(ui::kDeviceDisplayBuffers.total_bytes() == 96'000);
    assert(ui::kDeviceDisplayBuffers.prefer_external_ram);
    assert(ui::spacing::touch_target >= 48);

    constexpr std::array state_backgrounds{
        ui::color::surface,  ui::color::positive, ui::color::caution,
        ui::color::warning, ui::color::critical, ui::color::overtime,
    };
    for (const auto background : state_backgrounds) {
        const auto foreground = ui::contrast_text_rgb(background);
        assert(ui::contrast_ratio(background, foreground) >= 4.5);
    }
    assert(ui::contrast_ratio(ui::color::background, ui::color::text_secondary) >= 4.5);

    ui::RenderProfiler profiler;
    profiler.record(100, 1'024);
    profiler.record(300, 2'048);
    const auto metrics = profiler.metrics();
    assert(metrics.frame_count == 2);
    assert(metrics.average_render_us() == 200);
    assert(metrics.maximum_render_us == 300);
    assert(metrics.maximum_lvgl_bytes == 2'048);

    domain::UiSnapshot snapshot{};
    snapshot.session_remaining_ms = 25 * 60'000;
    auto model = ui::present(snapshot);
    assert(std::strcmp(model.session_status.data(), "SESSION") == 0);
    assert(model.accent_text_rgb == ui::contrast_text_rgb(model.accent_rgb));

    snapshot.session_remaining_ms = 4 * 60'000;
    model = ui::present(snapshot);
    assert(std::strcmp(model.session_status.data(), "FINAL 5 MIN") == 0);

    snapshot.session_remaining_ms = -1'000;
    model = ui::present(snapshot);
    assert(std::strcmp(model.session_status.data(), "OVERTIME") == 0);
    assert(model.session_remaining[0] == '+');

    static_assert(std::is_trivially_copyable_v<domain::UiSnapshot>);
    static_assert(std::is_trivially_copyable_v<ui::DeviceViewModel>);
    std::cout << "UI foundation budgets, contrast, cues, and metrics passed\n";
    return 0;
}

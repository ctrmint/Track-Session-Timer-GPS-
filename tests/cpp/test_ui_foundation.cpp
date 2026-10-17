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

    // The three figures Race draws (#221). Asserted here because the screen takes this
    // model verbatim rather than formatting anything itself, so these strings are the
    // contract between the presenter and what a driver reads at speed.
    domain::UiSnapshot race{};
    race.session_active = true;
    race.session_remaining_ms = 11 * 60'000 + 5'000;
    auto race_model = ui::present(race);
    // Nothing has measured a lap yet, so neither figure may read as a time. A zero here
    // would be a lap time the device does not have.
    assert(std::strcmp(race_model.current_lap.data(), "--:--.---") == 0);
    assert(std::strcmp(race_model.best_lap.data(), "--:--.---") == 0);
    assert(std::strcmp(race_model.session_remaining.data(), "11:05") == 0);

    race.current_lap_ms = 83'456;
    race.best_lap_ms = 81'002;
    race_model = ui::present(race);
    assert(std::strcmp(race_model.current_lap.data(), "1:23.456") == 0);
    assert(std::strcmp(race_model.best_lap.data(), "1:21.002") == 0);

    // Past ten minutes the minutes field grows a digit, which is why Race lays its cells
    // out for mm:ss.sss rather than m:ss.sss - a nine-cell row still holds this.
    race.current_lap_ms = 12 * 60'000 + 34'567;
    race_model = ui::present(race);
    assert(std::strcmp(race_model.current_lap.data(), "12:34.567") == 0);
    assert(std::strlen(race_model.current_lap.data()) == 9);

    static_assert(std::is_trivially_copyable_v<domain::UiSnapshot>);
    static_assert(std::is_trivially_copyable_v<ui::DeviceViewModel>);
    std::cout << "UI foundation budgets, contrast, cues, and metrics passed\n";
    return 0;
}

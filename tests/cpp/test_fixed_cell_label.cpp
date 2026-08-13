#include "fixed_cell_label.hpp"

#include <SDL2/SDL.h>
#include <lvgl.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

template <std::size_t Size>
std::array<std::int32_t, Size> positions(const track_timer::simulator::FixedCellLabel& label)
{
    std::array<std::int32_t, Size> result{};
    for (std::size_t index = 0; index < Size; ++index) {
        result[index] = label.cell_x(index);
    }
    return result;
}

}  // namespace

int main()
{
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    lv_init();
    auto* display = lv_sdl_window_create(600, 450);
    assert(display != nullptr);

    track_timer::simulator::FixedCellLabel lap_time;
    lap_time.create(lv_screen_active(), &lv_font_montserrat_48, lv_color_white(), 10,
                    track_timer::simulator::lap_time_cell_widths(34, 16, 12), 62);
    lap_time.set_position(150, 112);
    lap_time.set_text("1:11.111");
    lv_obj_update_layout(lv_screen_active());
    const auto lap_positions = positions<10>(lap_time);
    const auto lap_width = lv_obj_get_width(lap_time.object());

    for (const auto* value : {"8:48.888", "10:41.141", "+1:18.818", "--:--.---",
                              "123:45.678", "1234:56.789"}) {
        lap_time.set_text(value);
        lv_obj_update_layout(lv_screen_active());
        assert(positions<10>(lap_time) == lap_positions);
        assert(lv_obj_get_width(lap_time.object()) == lap_width);
    }

    track_timer::simulator::FixedCellLabel session_time;
    session_time.create(lv_screen_active(), &lv_font_montserrat_48, lv_color_white(), 7,
                        track_timer::simulator::session_time_cell_widths(34, 16), 60);
    session_time.set_position(350, 355);
    session_time.set_text("11:11");
    lv_obj_update_layout(lv_screen_active());
    const auto session_positions = positions<7>(session_time);
    const auto session_width = lv_obj_get_width(session_time.object());

    for (const auto* value : {"88:48", "100:00", "+11:11", "+100:00", "--:--",
                              "1000:00"}) {
        session_time.set_text(value);
        lv_obj_update_layout(lv_screen_active());
        assert(positions<7>(session_time) == session_positions);
        assert(lv_obj_get_width(session_time.object()) == session_width);
    }

    lv_display_delete(display);
    lv_sdl_quit();
    lv_deinit();
    std::cout << "Fixed-cell LVGL timer positions passed\n";
    return 0;
}

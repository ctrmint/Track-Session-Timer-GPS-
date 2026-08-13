#include "device_screen.hpp"

#include <SDL2/SDL.h>
#include <lvgl.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

struct GnssIndicatorExpectation {
    track_timer::domain::GnssHealth health;
    std::uint32_t color_rgb;
    const char* text;
};

}  // namespace

int main()
{
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    lv_init();
    auto* display = lv_sdl_window_create(600, 450);
    assert(display != nullptr);

    track_timer::simulator::DeviceScreen screen{lv_screen_active()};
    auto* indicator = screen.gnss_indicator_object();
    assert(indicator != nullptr);
    assert(std::strcmp(lv_label_get_text(indicator), LV_SYMBOL_GPS " NO GPS") == 0);

    const auto initial_x = lv_obj_get_x(indicator);
    const auto initial_y = lv_obj_get_y(indicator);
    const auto initial_width = lv_obj_get_width(indicator);
    constexpr std::array expectations{
        GnssIndicatorExpectation{track_timer::domain::GnssHealth::good, 0x76FF9A,
                                 LV_SYMBOL_GPS " GOOD"},
        GnssIndicatorExpectation{track_timer::domain::GnssHealth::poor, 0xFFD54F,
                                 LV_SYMBOL_GPS " POOR"},
        GnssIndicatorExpectation{track_timer::domain::GnssHealth::unavailable, 0xFF5252,
                                 LV_SYMBOL_GPS " NO GPS"},
        GnssIndicatorExpectation{track_timer::domain::GnssHealth::searching, 0xFF5252,
                                 LV_SYMBOL_GPS " SEARCH"},
        GnssIndicatorExpectation{track_timer::domain::GnssHealth::stale, 0xFF5252,
                                 LV_SYMBOL_GPS " STALE"},
    };

    track_timer::ui::DeviceViewModel model{};
    for (const auto& expectation : expectations) {
        model.gnss_health = expectation.health;
        screen.update(model);
        assert(std::strcmp(lv_label_get_text(indicator), expectation.text) == 0);
        assert(lv_color_eq(lv_obj_get_style_text_color(indicator, LV_PART_MAIN),
                           lv_color_hex(expectation.color_rgb)));
        assert(lv_obj_get_x(indicator) == initial_x);
        assert(lv_obj_get_y(indicator) == initial_y);
        assert(lv_obj_get_width(indicator) == initial_width);
    }

    auto* session_status = screen.session_status_object();
    assert(session_status != nullptr);
    std::strcpy(model.session_status.data(), "FINAL 5 MIN");
    model.accent_rgb = 0xD32F2F;
    model.accent_text_rgb = 0xFFFFFF;
    screen.update(model);
    assert(std::strcmp(lv_label_get_text(session_status), "FINAL 5 MIN") == 0);

    lv_display_delete(display);
    lv_sdl_quit();
    lv_deinit();
    std::cout << "GPS indicator symbol, text cues, colours, and position passed\n";
    return 0;
}

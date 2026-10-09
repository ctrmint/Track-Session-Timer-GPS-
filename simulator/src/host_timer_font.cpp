#include "track_timer/ui/timer_font.hpp"

// The device rasterises a real face from a TTF the ESP-IDF build embeds in the image,
// reached through linker symbols that only that build produces. The simulator has no such
// file and does not enable LVGL's tiny_ttf, so it answers with the largest built-in font.
//
// The consequence is worth stating plainly: the simulator shows the GPS Only layout, its
// three receiver states and its number formatting, but not the device's numeral size -
// 48 px here against 120 px there. Whether road speed is big enough to read from a driving
// position is a question only the panel can answer.
namespace track_timer::ui {

const lv_font_t* countdown_font(const std::int32_t size_px) noexcept
{
    static_cast<void>(size_px);
    return &lv_font_montserrat_48;
}

}  // namespace track_timer::ui

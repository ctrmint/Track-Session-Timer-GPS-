#include "track_timer/ui/timer_font.hpp"

#include <array>
#include <cstddef>

extern const std::uint8_t montserrat_bold_digits_ttf_start[] asm(
    "_binary_montserrat_bold_digits_ttf_start");
extern const std::uint8_t montserrat_bold_digits_ttf_end[] asm(
    "_binary_montserrat_bold_digits_ttf_end");

namespace track_timer::ui {
namespace {

// Faces are cached by size: creating one parses the file and allocates, so asking for the
// same size twice must not leak a font per call.
struct Entry {
    std::int32_t size{0};
    lv_font_t* font{nullptr};
};

constexpr std::size_t kMaximumSizes = 4;
std::array<Entry, kMaximumSizes> cache{};
std::size_t cached = 0;

}  // namespace

const lv_font_t* countdown_font(const std::int32_t size_px) noexcept
{
    if (size_px <= 0) {
        return nullptr;
    }
    for (std::size_t index = 0; index < cached; ++index) {
        if (cache[index].size == size_px) {
            return cache[index].font;
        }
    }
    if (cached >= cache.size()) {
        return nullptr;
    }

    const auto length = static_cast<std::size_t>(montserrat_bold_digits_ttf_end -
                                                 montserrat_bold_digits_ttf_start);
    // Sixteen entries covers every glyph in the subset, so nothing is ever evicted and
    // each digit is rasterised exactly once for the life of the font.
    auto* font = lv_tiny_ttf_create_data_ex(montserrat_bold_digits_ttf_start, length,
                                            size_px, LV_FONT_KERNING_NONE, 16);
    if (font == nullptr) {
        return nullptr;
    }
    cache[cached++] = {size_px, font};
    return font;
}

}  // namespace track_timer::ui

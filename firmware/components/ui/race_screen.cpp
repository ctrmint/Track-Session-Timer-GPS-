#include "track_timer/ui/race_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"
#include "track_timer/ui/timer_font.hpp"

#include <cstring>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;
constexpr std::int32_t kSideMargin = 30;
constexpr std::int32_t kUsableWidth = kScreenWidth - 2 * kSideMargin;

// The numeral font's metrics, as every screen that uses it states them: a digit cell is
// 0.7235 of the size, a colon or point 0.288, and a line 1.218.
constexpr std::int32_t cell_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 7235 / 10000;
}
constexpr std::int32_t punctuation_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 288 / 1000;
}
constexpr std::int32_t cell_height_for(const std::int32_t font_px) noexcept
{
    return font_px * 1218 / 1000;
}

// mm:ss.sss is seven digits and two pieces of punctuation. At 92 px that is 519 px, which
// fits the 540 px between the margins with room to spare - and it is the largest size that
// does, which matters because this is the number being read at speed.
constexpr std::int32_t kCurrentPx = 92;
// The two supporting figures, sized so the pair sits on one row: a lap at 56 px is 316 px
// and mm:ss is 178 px, which leaves a 46 px gap between them.
constexpr std::int32_t kSupportPx = 56;

constexpr std::int32_t lap_block_width(const std::int32_t font_px) noexcept
{
    return cell_width_for(font_px) * 7 + punctuation_width_for(font_px) * 2;
}
constexpr std::int32_t session_block_width(const std::int32_t font_px) noexcept
{
    return cell_width_for(font_px) * 4 + punctuation_width_for(font_px);
}

constexpr std::int32_t kCurrentBlockWidth = lap_block_width(kCurrentPx);
constexpr std::int32_t kFastestBlockWidth = lap_block_width(kSupportPx);
constexpr std::int32_t kSessionBlockWidth = session_block_width(kSupportPx);

static_assert(kCurrentBlockWidth <= kUsableWidth,
              "the lap in progress must fit the panel at this size, margins included");
static_assert(kFastestBlockWidth + kSessionBlockWidth <= kUsableWidth,
              "the fastest lap and the session clock share one row and must both fit");

constexpr std::int32_t kScreenHeight = 450;
constexpr std::int32_t kBottomMargin = 30;

constexpr std::int32_t kTitleTop = 10;
// Where the title row stops mattering: the body font's line plus its top offset.
constexpr std::int32_t kTitleBottom = 44;
// The lap in progress sits centred in the band between the title and the supporting row,
// rather than pushed to the top of it. Derived so that moving the supporting row does not
// silently unbalance this one - the band is whatever is left between the two.
constexpr std::int32_t kCurrentCaptionHeight = 28;
constexpr std::int32_t kCurrentGroupHeight =
    kCurrentCaptionHeight + cell_height_for(kCurrentPx);

// The supporting row is anchored to the bottom of the panel rather than stacked under the
// lap in progress. Derived from the bottom margin so it stays put if either size changes,
// and so the empty space lands between the two groups where it separates them, instead of
// below both where it reads as the layout having been cut short.
constexpr std::int32_t kSupportTop =
    kScreenHeight - kBottomMargin - cell_height_for(kSupportPx);
constexpr std::int32_t kSupportCaptionTop = kSupportTop - 30;

constexpr std::int32_t kCurrentCaptionTop =
    kTitleBottom + (kSupportCaptionTop - kTitleBottom - kCurrentGroupHeight) / 2;
constexpr std::int32_t kCurrentTop = kCurrentCaptionTop + kCurrentCaptionHeight;

static_assert(kCurrentCaptionTop > kTitleBottom,
              "the lap in progress must clear the title row");
static_assert(kSupportCaptionTop > kCurrentTop + cell_height_for(kCurrentPx),
              "the supporting row must clear the lap in progress");
static_assert(kSupportTop + cell_height_for(kSupportPx) <= kScreenHeight - 1,
              "the whole layout must fit the panel height");

// Which cells hold punctuation rather than a digit. Stated once so the construction loop
// and the update loop cannot disagree about the shape of the row.
constexpr bool lap_cell_is_digit(const std::size_t index) noexcept
{
    return index != 2 && index != 5;
}
constexpr bool session_cell_is_digit(const std::size_t index) noexcept
{
    return index != 2;
}

lv_obj_t* make_cell(lv_obj_t* const parent, const lv_font_t* const font,
                    const std::int32_t width, const std::int32_t height,
                    const std::int32_t x, const std::int32_t y,
                    const std::uint32_t rgb) noexcept
{
    auto* cell = lv_label_create(parent);
    if (font != nullptr) {
        lv_obj_set_style_text_font(cell, font, 0);
    }
    lv_obj_set_style_text_color(cell, lv_color_hex(rgb), 0);
    lv_obj_set_size(cell, width, height);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(cell, "");
    return cell;
}

// Writes a formatted time into a row of fixed cells, one character per cell, touching only
// the cells whose character has changed.
//
// The source may be shorter than the row - a lap under ten minutes formats as "1:23.456",
// eight characters into nine cells - so it is laid in from the right. The leading cell is
// then left blank rather than padded with a zero that would read as part of the number.
template <std::size_t Cells>
void write_row(const std::array<lv_obj_t*, Cells>& cells,
               std::array<char, Cells + 1>& shown, const char* const text) noexcept
{
    const auto length = std::strlen(text);
    for (std::size_t index = 0; index < Cells; ++index) {
        const auto from_right = Cells - index;
        const char character = length >= from_right ? text[length - from_right] : '\0';
        if (character == shown[index]) {
            continue;
        }
        shown[index] = character;
        if (cells[index] == nullptr) {
            continue;
        }
        const char glyph[2] = {character, '\0'};
        lv_label_set_text(cells[index], character == '\0' ? "" : glyph);
    }
}

}  // namespace

RaceScreen::RaceScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);

    track_label_ = create_label(root_, Typography::body, color::text_secondary,
                                LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(track_label_, kSideMargin, kTitleTop);
    lv_label_set_text(track_label_, "");

    mode_label_ = create_label(root_, Typography::body, color::text_secondary,
                               LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(mode_label_, 200);
    lv_obj_set_pos(mode_label_, kScreenWidth - 200 - kSideMargin, kTitleTop);
    lv_label_set_text(mode_label_, "RACE");

    current_caption_ = create_label(root_, Typography::caption, color::text_secondary,
                                    LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(current_caption_, kSideMargin, kCurrentCaptionTop);
    lv_label_set_text(current_caption_, "CURRENT LAP");

    // The lap in progress, centred and in the brightest text on the screen: it is what the
    // driver is reading, and the other two are reference.
    const auto* const current_font = countdown_font(kCurrentPx);
    const auto current_left = (kScreenWidth - kCurrentBlockWidth) / 2;
    auto x = current_left;
    for (std::size_t index = 0; index < kLapCells; ++index) {
        const auto digit = lap_cell_is_digit(index);
        const auto width =
            digit ? cell_width_for(kCurrentPx) : punctuation_width_for(kCurrentPx);
        current_cells_[index] =
            make_cell(root_, current_font, width, cell_height_for(kCurrentPx), x,
                      kCurrentTop, color::text_primary);
        x += width;
    }

    const auto* const support_font = countdown_font(kSupportPx);
    const auto session_left = kScreenWidth - kSideMargin - kSessionBlockWidth;

    fastest_caption_ = create_label(root_, Typography::caption, color::text_secondary,
                                    LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(fastest_caption_, kSideMargin, kSupportCaptionTop);
    lv_label_set_text(fastest_caption_, "FASTEST");

    session_caption_ = create_label(root_, Typography::caption, color::text_secondary,
                                    LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(session_caption_, kSessionBlockWidth);
    lv_obj_set_pos(session_caption_, session_left, kSupportCaptionTop);
    lv_label_set_text(session_caption_, "SESSION LEFT");

    x = kSideMargin;
    for (std::size_t index = 0; index < kLapCells; ++index) {
        const auto digit = lap_cell_is_digit(index);
        const auto width =
            digit ? cell_width_for(kSupportPx) : punctuation_width_for(kSupportPx);
        fastest_cells_[index] =
            make_cell(root_, support_font, width, cell_height_for(kSupportPx), x,
                      kSupportTop, color::positive_bright);
        x += width;
    }

    x = session_left;
    for (std::size_t index = 0; index < kSessionCells; ++index) {
        const auto digit = session_cell_is_digit(index);
        const auto width =
            digit ? cell_width_for(kSupportPx) : punctuation_width_for(kSupportPx);
        session_cells_[index] =
            make_cell(root_, support_font, width, cell_height_for(kSupportPx), x,
                      kSupportTop, color::text_primary);
        x += width;
    }
}

void RaceScreen::update(const DeviceViewModel& model) noexcept
{
    write_row(current_cells_, shown_current_, model.current_lap.data());
    write_row(fastest_cells_, shown_fastest_, model.best_lap.data());
    write_row(session_cells_, shown_session_, model.session_remaining.data());
}

void RaceScreen::set_track_name(const char* const name) noexcept
{
    if (track_label_ == nullptr || name == nullptr) {
        return;
    }
    lv_label_set_text(track_label_, name);
}

lv_obj_t* RaceScreen::root() const noexcept { return root_; }

}  // namespace track_timer::ui

#include "track_timer/ui/navigation_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"
#include "track_timer/ui/timer_font.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kScreenWidth = 600;
constexpr std::int32_t kScreenHeight = 450;
constexpr std::int32_t kSideMargin = 30;
constexpr std::int32_t kBottomMargin = 30;
constexpr std::int32_t kUsableWidth = kScreenWidth - 2 * kSideMargin;

// The same metrics every other large numeral on this device takes from the face itself.
constexpr std::int32_t cell_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 7235 / 10000;
}
constexpr std::int32_t cell_height_for(const std::int32_t font_px) noexcept
{
    return font_px * 1218 / 1000;
}
// A colon is far narrower than a digit, and giving it a digit's cell would open a gap
// either side of it that reads as a missing character.
constexpr std::int32_t separator_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 288 / 1000;
}

// What the inline unit is allowed to take, and what the face is sized against.
//
// "MPH" at montserrat_28 measures around 66 px; 72 is that with room for a metrics change.
// The gap is what stops the unit reading as a fourth digit.
constexpr std::int32_t kUnitWidthBudget = 72;
constexpr std::int32_t kUnitGap = 14;

// 208 px, and the arithmetic is the whole reason it is not Speed Only's 248.
//
// Three digits plus the unit and its gap have to fit 540 px. That leaves 454 px for the
// digits, so a cell may be 151 px, so the face may be 151 / 0.7235 = 208 px. The inline
// unit costs 40 px of face and 49 px of glyph height against Speed Only, which is what
// buys the clock and the arrow a band to live in.
constexpr std::int32_t kSpeedPx = 208;
constexpr std::int32_t kDigitCellWidth = cell_width_for(kSpeedPx);
constexpr std::int32_t kDigitCellHeight = cell_height_for(kSpeedPx);
static_assert(kDigitCellWidth * 3 + kUnitGap + kUnitWidthBudget <= kUsableWidth,
              "a three-digit speed and the inline unit must both fit across the panel");

// Printed low, which is what Navigation asks for and Speed Only does not do - and low
// means the *ink* is low, not the cell.
//
// A label's box is a line box: it carries the face's descent under the baseline, and
// figures use none of it. Hanging the box off the bottom margin therefore leaves a quarter
// of the face's height as empty panel under the number. So the baseline is placed first
// and the box is hung from it.
//
// The descent comes from the same arithmetic as the rest of the face rather than from
// lv_font_get_line_height, and that is deliberate. The simulator's countdown_font is a
// stub that answers every size with one 52 px face, so on the host those metrics describe
// a font that is not the one being laid out. Every other large numeral on this device is
// positioned from this arithmetic for that reason, and a screen that measured instead
// would lay itself out differently in the simulator than on the panel.
constexpr std::int32_t descent_for(const std::int32_t font_px) noexcept
{
    return font_px * 251 / 1000;
}
constexpr std::int32_t kSpeedBaseline = kScreenHeight - kBottomMargin;
constexpr std::int32_t kDigitsTop =
    kSpeedBaseline - (kDigitCellHeight - descent_for(kSpeedPx));
// Where the ink starts. Montserrat's figures are about 0.70 em tall; this guards a gap of
// a hundred-odd pixels, so the estimate is ample for what it is asked to do.
constexpr std::int32_t kSpeedInkTop = kSpeedBaseline - kSpeedPx * 70 / 100;

// The clock. Large enough to read at a glance, small enough to leave the arrow its corner.
constexpr std::int32_t kTimePx = 48;
constexpr std::int32_t kTimeDigitWidth = cell_width_for(kTimePx);
constexpr std::int32_t kTimeSeparatorWidth = separator_width_for(kTimePx);
constexpr std::int32_t kTimeCellHeight = cell_height_for(kTimePx);
constexpr std::int32_t kTimeTop = 22;
constexpr std::int32_t kTimeStatusTop = kTimeTop + kTimeCellHeight + 6;
constexpr std::int32_t kSpeedStatusTop = kTimeStatusTop + 28;
constexpr std::int32_t kStatusWidth = 300;

constexpr bool time_cell_is_separator(const std::size_t index) noexcept
{
    return index == 2 || index == 5;
}
constexpr std::int32_t time_cell_width(const std::size_t index) noexcept
{
    return time_cell_is_separator(index) ? kTimeSeparatorWidth : kTimeDigitWidth;
}
constexpr std::int32_t time_cell_left(const std::size_t index) noexcept
{
    std::int32_t left = kSideMargin;
    for (std::size_t before = 0; before < index; ++before) {
        left += time_cell_width(before);
    }
    return left;
}
constexpr std::int32_t kTimeWidth = time_cell_left(8) - kSideMargin;

// The arrow's ring, in the top right corner and as large as the corner allows.
constexpr std::int32_t kRingRadius = 64;
constexpr std::int32_t kRingCentreX = kScreenWidth - kSideMargin - kRingRadius - 2;
constexpr std::int32_t kRingCentreY = 92;
static_assert(kRingCentreY + kRingRadius < kSpeedInkTop,
              "the ring must clear the speed it sits above");
static_assert(kRingCentreX - kRingRadius > kSideMargin + kTimeWidth,
              "the ring must clear the clock beside it");

constexpr std::int32_t kTickRadius = kRingRadius - 3;
constexpr std::int32_t kCardinalTickSize = 7;
constexpr std::int32_t kTickSize = 4;
// Outside the ring rather than inside it. Inside, the arrow sweeps through the letter
// twice a turn; outside, the one thing that establishes which way the card is oriented is
// never crossed by the thing being oriented.
constexpr std::int32_t kNorthTop = kRingCentreY - kRingRadius - 24;
static_assert(kNorthTop >= 0, "the north label must fit on the panel");

// A needle: thin at the tail, thickening to the tip, spaced so the dots very nearly touch.
// That near-continuity is what makes five circles read as one thing pointing somewhere
// rather than as five circles. A negative radius puts a dot on the far side of the centre,
// which is all a tail is.
constexpr std::int32_t kArrowRadii[5] = {-30, -14, 4, 24, 44};
constexpr std::int32_t kArrowSizes[5] = {7, 8, 10, 14, 20};
static_assert(kArrowRadii[4] + kArrowSizes[4] / 2 < kTickRadius - 2,
              "the tip must stay inside the ticks");
static_assert(-kArrowRadii[0] + kArrowSizes[0] / 2 < kTickRadius - 2,
              "the tail must stay inside the ticks too");

constexpr std::uint32_t kCard = 0x3A4556;
constexpr std::uint32_t kCardBright = 0x6C7E96;
constexpr std::uint32_t kArrow = 0xFF5252;

constexpr float kPi = 3.14159265F;

// Screen position of a bearing on the ring. North is up and bearings run clockwise, which
// is why sine drives x and cosine is subtracted from y rather than the other way about.
void place(lv_obj_t* const object, const float bearing_deg, const std::int32_t radius,
           const std::int32_t size) noexcept
{
    const auto radians = bearing_deg * kPi / 180.0F;
    const auto x = static_cast<std::int32_t>(std::lround(
        static_cast<float>(kRingCentreX) + static_cast<float>(radius) * std::sin(radians)));
    const auto y = static_cast<std::int32_t>(std::lround(
        static_cast<float>(kRingCentreY) - static_cast<float>(radius) * std::cos(radians)));
    lv_obj_set_pos(object, x - size / 2, y - size / 2);
}

lv_obj_t* make_dot(lv_obj_t* const parent, const std::int32_t size,
                   const std::uint32_t rgb) noexcept
{
    auto* dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(rgb), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    return dot;
}

lv_obj_t* make_cell(lv_obj_t* const parent, const lv_font_t* const font,
                    const std::int32_t width, const std::int32_t height,
                    const std::int32_t left, const std::int32_t top) noexcept
{
    auto* cell = lv_label_create(parent);
    if (font != nullptr) {
        lv_obj_set_style_text_font(cell, font, 0);
    }
    lv_obj_set_style_text_color(cell, lv_color_hex(color::text_primary), 0);
    lv_obj_set_size(cell, width, height);
    lv_obj_set_pos(cell, left, top);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(cell, "");
    return cell;
}

// How far a font's baseline sits below the top of its line box.
//
// Used for the unit alone, and only because the unit is a built-in face that is the same
// font on the host as on the panel - so unlike the numeral above it, measuring it tells
// the truth in both places. It is what sits the unit on the digits' baseline instead of at
// an offset picked by eye, which would not survive either face being resized.
[[nodiscard]] std::int32_t baseline_offset(const lv_font_t* const font,
                                           const std::int32_t fallback) noexcept
{
    if (font == nullptr) {
        return fallback;
    }
    return static_cast<std::int32_t>(lv_font_get_line_height(font)) -
           static_cast<std::int32_t>(font->base_line);
}

[[nodiscard]] std::int32_t measure(const lv_font_t* const font,
                                   const char* const text) noexcept
{
    if (font == nullptr) {
        return 0;
    }
    lv_point_t size{};
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return size.x;
}

}  // namespace

NavigationScreen::NavigationScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);

    // The clock, top left. Fixed cells, so the seconds ticking cannot shift the hours.
    const auto* time_font = countdown_font(kTimePx);
    for (std::size_t index = 0; index < time_cells_.size(); ++index) {
        time_cells_[index] =
            make_cell(root_, time_font, time_cell_width(index), kTimeCellHeight,
                      time_cell_left(index), kTimeTop);
    }
    time_shown_.fill('\0');

    time_status_ = create_label(root_, Typography::body, color::text_secondary,
                                LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(time_status_, kStatusWidth);
    lv_obj_set_pos(time_status_, kSideMargin, kTimeStatusTop);
    lv_label_set_text(time_status_, "");

    speed_status_ = create_label(root_, Typography::body, color::text_secondary,
                                 LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(speed_status_, kStatusWidth);
    lv_obj_set_pos(speed_status_, kSideMargin, kSpeedStatusTop);
    lv_label_set_text(speed_status_, "");

    // The ring: built once, never moved again, exactly as CompassScreen's card is.
    for (std::size_t index = 0; index < ticks_.size(); ++index) {
        const auto cardinal = index % 2 == 0;
        const auto size = cardinal ? kCardinalTickSize : kTickSize;
        ticks_[index] = make_dot(root_, size, cardinal ? kCardBright : kCard);
        place(ticks_[index],
              static_cast<float>(index) * 360.0F / static_cast<float>(ticks_.size()),
              kTickRadius, size);
    }

    auto* north = create_label(root_, Typography::body, color::text_secondary);
    lv_obj_set_width(north, 60);
    lv_obj_set_pos(north, kRingCentreX - 30, kNorthTop);
    lv_label_set_text(north, "N");

    // Created before the arrow so the arrow draws over it, and sized to the ring so a
    // reason for the arrow's absence has somewhere to go that is not the ring's edge.
    heading_status_ = create_label(root_, Typography::caption, color::text_secondary);
    lv_obj_set_width(heading_status_, kRingRadius * 2);
    lv_obj_set_pos(heading_status_, kRingCentreX - kRingRadius, kRingCentreY - 8);
    lv_label_set_text(heading_status_, "");

    // Hidden until there is a heading. An unhidden arrow at a default bearing is the exact
    // failure this screen inherits from Compass mode and must not repeat: a device
    // pointing somewhere when it knows nothing.
    for (std::size_t index = 0; index < arrow_.size(); ++index) {
        arrow_[index] = make_dot(root_, kArrowSizes[index], kArrow);
        lv_obj_add_flag(arrow_[index], LV_OBJ_FLAG_HIDDEN);
    }

    // The speed, low and large. The unit is measured first, because where the digits are
    // centred depends on how much width is left once it has taken its own.
    const auto* unit_font = font_for(Typography::timer_secondary);
    const auto measured = measure(unit_font, "MPH");
    unit_width_ = measured > 0 ? measured : kUnitWidthBudget;
    const auto unit_left = kScreenWidth - kSideMargin - unit_width_;
    digits_centre_ = (kSideMargin + unit_left - kUnitGap) / 2;

    const auto* speed_font = countdown_font(kSpeedPx);
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        digits_[index] = make_cell(root_, speed_font, kDigitCellWidth, kDigitCellHeight,
                                   digits_centre_ -
                                       static_cast<std::int32_t>(digits_.size()) *
                                           kDigitCellWidth / 2 +
                                       static_cast<std::int32_t>(index) * kDigitCellWidth,
                                   kDigitsTop);
        digits_shown_[index] = '\0';
    }
    shown_count_ = digits_.size();

    unit_ = create_label(root_, Typography::timer_secondary, color::text_secondary,
                         LV_TEXT_ALIGN_LEFT);
    lv_obj_set_width(unit_, unit_width_ + 4);
    // Sat on the digits' own baseline rather than at a measured-by-eye offset.
    lv_obj_set_pos(unit_, unit_left, kSpeedBaseline - baseline_offset(unit_font, 25));
    lv_label_set_text(unit_, "");

    time_status_shown_[0] = '\0';
    speed_status_shown_[0] = '\0';
    heading_status_shown_[0] = '\0';
}

void NavigationScreen::update(const NavigationViewModel& model) noexcept
{
    // The clock. A time that is not known shows dashes in the same cells, so the block
    // neither moves nor disappears between a known time and an unknown one.
    char time_text[kTimeCells + 1] = {};
    std::snprintf(time_text, sizeof(time_text), "%s:%s", model.time.time.data(),
                  model.time.seconds.data());
    for (std::size_t index = 0; index < time_cells_.size(); ++index) {
        const char character = time_text[index];
        if (character == time_shown_[index]) {
            continue;
        }
        time_shown_[index] = character;
        const char text[2] = {character, '\0'};
        lv_label_set_text(time_cells_[index], text);
    }
    if (model.time.digits_valid != time_valid_) {
        time_valid_ = model.time.digits_valid;
        for (auto* cell : time_cells_) {
            lv_obj_set_style_text_color(
                cell,
                lv_color_hex(time_valid_ ? color::text_primary : color::text_secondary), 0);
        }
    }

    // One fault stated once. With no receiver all three readouts are absent for the same
    // reason, and saying it three times reads as three faults.
    const auto* const time_status =
        model.has_shared_fault ? model.shared_fault.data() : model.time.status.data();
    if (std::strcmp(time_status_shown_.data(), time_status) != 0) {
        lv_label_set_text(time_status_, time_status);
        lv_obj_set_style_text_color(
            time_status_,
            lv_color_hex(model.has_shared_fault ? model.shared_fault_rgb
                                                : model.time.status_rgb),
            0);
        std::snprintf(time_status_shown_.data(), time_status_shown_.size(), "%s",
                      time_status);
    }

    // The unit carries "MPH" and the status line carries the fault, and the two are never
    // on screen together: "MPH" beside a row of dashes would read as a speed of nothing
    // rather than as no speed at all.
    const auto* const speed_status = (model.speed.speed_valid || model.has_shared_fault)
                                         ? ""
                                         : model.speed.status.data();
    if (std::strcmp(speed_status_shown_.data(), speed_status) != 0) {
        lv_label_set_text(speed_status_, speed_status);
        lv_obj_set_style_text_color(speed_status_, lv_color_hex(model.speed.status_rgb), 0);
        std::snprintf(speed_status_shown_.data(), speed_status_shown_.size(), "%s",
                      speed_status);
    }
    if (model.speed.speed_valid != speed_valid_) {
        speed_valid_ = model.speed.speed_valid;
        lv_label_set_text(unit_, speed_valid_ ? model.speed.status.data() : "");
        for (auto* cell : digits_) {
            lv_obj_set_style_text_color(
                cell,
                lv_color_hex(speed_valid_ ? color::text_primary : color::text_secondary), 0);
        }
    }

    // The block is centred on the digits actually shown, so every reading is centred
    // rather than only the two-digit ones. The unit does not move with it.
    const auto count = std::strlen(model.speed.speed.data());
    const auto shown = count == 0 || count > digits_.size() ? digits_.size() : count;
    if (shown != shown_count_) {
        shown_count_ = shown;
        const auto left =
            digits_centre_ - static_cast<std::int32_t>(shown) * kDigitCellWidth / 2;
        for (std::size_t index = 0; index < digits_.size(); ++index) {
            lv_obj_set_pos(digits_[index],
                           left + static_cast<std::int32_t>(index) * kDigitCellWidth,
                           kDigitsTop);
        }
    }
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        const char character = index < shown ? model.speed.speed[index] : '\0';
        if (character == digits_shown_[index]) {
            continue;
        }
        digits_shown_[index] = character;
        const char text[2] = {character, '\0'};
        lv_label_set_text(digits_[index], text);
    }

    // The arrow. Absent whenever the heading is not trustworthy, which includes a parked
    // car with sixteen satellites: there is no magnetometer on this board, so a car that
    // is not moving has no heading at all rather than a stale one.
    if (model.heading.heading_valid != arrow_visible_) {
        arrow_visible_ = model.heading.heading_valid;
        for (auto* dot : arrow_) {
            if (arrow_visible_) {
                lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            }
        }
        // Forget where it was, so the next heading always repositions rather than
        // reappearing at the bearing it held before the car stopped.
        arrow_bearing_ = -1;
    }
    if (arrow_visible_) {
        // Moved only when the bearing has changed by a whole degree. This is not
        // smoothing: the bearing shown is still exactly what the receiver said.
        const auto bearing = static_cast<int>(std::lround(model.heading.heading_deg)) % 360;
        if (bearing != arrow_bearing_) {
            arrow_bearing_ = bearing;
            for (std::size_t index = 0; index < arrow_.size(); ++index) {
                place(arrow_[index], static_cast<float>(bearing), kArrowRadii[index],
                      kArrowSizes[index]);
            }
        }
    }

    const auto* const heading_status =
        (model.heading.heading_valid || model.has_shared_fault)
            ? ""
            : model.heading.status.data();
    if (std::strcmp(heading_status_shown_.data(), heading_status) != 0) {
        lv_label_set_text(heading_status_, heading_status);
        lv_obj_set_style_text_color(heading_status_,
                                    lv_color_hex(model.heading.status_rgb), 0);
        std::snprintf(heading_status_shown_.data(), heading_status_shown_.size(), "%s",
                      heading_status);
    }
}

}  // namespace track_timer::ui

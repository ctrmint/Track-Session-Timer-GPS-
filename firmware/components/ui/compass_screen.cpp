#include "track_timer/ui/compass_screen.hpp"

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

// Centred below the title line, at the largest radius that keeps the card clear of both
// edges: 240 +/- 195 spans 45 to 435 of a 450 px panel.
constexpr std::int32_t kCentreX = kScreenWidth / 2;
constexpr std::int32_t kCentreY = 240;
constexpr std::int32_t kCardRadius = 195;
static_assert(kCentreY + kCardRadius <= kScreenHeight, "the card must fit the panel");
static_assert(kCentreY - kCardRadius >= 40, "the card must clear the title line");

constexpr std::int32_t kTickRadius = kCardRadius - 5;
constexpr std::int32_t kTickSize = 9;
constexpr std::int32_t kCardinalTickSize = 17;
constexpr std::int32_t kNorthLabelRadius = kCardRadius - 33;

// The same metrics every other large numeral on this device takes from the face itself.
constexpr std::int32_t cell_width_for(const std::int32_t font_px) noexcept
{
    return font_px * 7235 / 10000;
}
constexpr std::int32_t cell_height_for(const std::int32_t font_px) noexcept
{
    return font_px * 1218 / 1000;
}

// Sized to live inside the card rather than to fill the panel. Three cells at 120 px are
// 258 wide and 146 tall, which leaves the marker a clear ring to travel.
constexpr std::int32_t kDegreesPx = 120;
constexpr std::int32_t kDigitCellWidth = cell_width_for(kDegreesPx);
constexpr std::int32_t kDigitCellHeight = cell_height_for(kDegreesPx);
constexpr std::int32_t kDigitsLeft = kCentreX - kDigitCellWidth * 3 / 2;
constexpr std::int32_t kDigitsTop = kCentreY - kDigitCellHeight / 2;
constexpr std::int32_t kCardinalTop = kDigitsTop + kDigitCellHeight + 4;

// Inside the ticks and outside the degrees, so the marker never crosses either.
constexpr std::int32_t kMarkerRadii[3] = {kCardRadius - 45, kCardRadius - 27,
                                          kCardRadius - 9};
constexpr std::int32_t kMarkerSizes[3] = {10, 15, 21};
static_assert(kMarkerRadii[0] > kDigitCellWidth * 3 / 2,
              "the marker must clear the degrees at its closest approach");

constexpr std::uint32_t kCard = 0x3A4556;
constexpr std::uint32_t kCardBright = 0x6C7E96;
constexpr std::uint32_t kMarker = 0xFF5252;

constexpr float kPi = 3.14159265F;

// Screen position of a bearing on the card. North is up and bearings run clockwise, which
// is why sine drives x and cosine is subtracted from y rather than the other way about.
void place(lv_obj_t* const object, const float bearing_deg, const std::int32_t radius,
           const std::int32_t size) noexcept
{
    const auto radians = bearing_deg * kPi / 180.0F;
    const auto x = static_cast<std::int32_t>(
        std::lround(static_cast<float>(kCentreX) + static_cast<float>(radius) *
                                                       std::sin(radians)));
    const auto y = static_cast<std::int32_t>(
        std::lround(static_cast<float>(kCentreY) - static_cast<float>(radius) *
                                                       std::cos(radians)));
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

}  // namespace

CompassScreen::CompassScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);

    title_ = create_label(root_, Typography::body, color::text_secondary, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(title_, kSideMargin, 10);
    lv_label_set_text(title_, "COMPASS");

    status_ = create_label(root_, Typography::body, color::text_secondary,
                           LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_width(status_, 300);
    lv_obj_set_pos(status_, kScreenWidth - 300 - kSideMargin, 10);
    lv_label_set_text(status_, "");

    // The card: built once, never moved again. Every sixteenth of the turn gets a tick,
    // and the four quarters get a larger one so the quadrants read without labels.
    for (std::size_t index = 0; index < ticks_.size(); ++index) {
        const auto cardinal = index % 4 == 0;
        const auto size = cardinal ? kCardinalTickSize : kTickSize;
        auto* tick = make_dot(root_, size, cardinal ? kCardBright : kCard);
        place(tick, static_cast<float>(index) * 360.0F / static_cast<float>(ticks_.size()),
              kTickRadius, size);
        ticks_[index] = tick;
    }

    // One letter is all the card needs to say which way it is oriented. The bearing's own
    // cardinal is spelled out under the degrees, where three characters fit.
    auto* north = create_label(root_, Typography::heading, color::text_secondary);
    lv_obj_set_width(north, 60);
    lv_obj_set_pos(north, kCentreX - 30, kCentreY - kNorthLabelRadius - 15);
    lv_label_set_text(north, "N");

    const auto* font = countdown_font(kDegreesPx);
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        auto* cell = lv_label_create(root_);
        if (font != nullptr) {
            lv_obj_set_style_text_font(cell, font, 0);
        }
        lv_obj_set_style_text_color(cell, lv_color_hex(color::text_primary), 0);
        lv_obj_set_size(cell, kDigitCellWidth, kDigitCellHeight);
        lv_obj_set_pos(cell, kDigitsLeft + static_cast<std::int32_t>(index) * kDigitCellWidth,
                       kDigitsTop);
        lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(cell, "");
        digits_[index] = cell;
        digits_shown_[index] = '\0';
    }

    cardinal_ = create_label(root_, Typography::heading, color::text_secondary);
    lv_obj_set_width(cardinal_, kScreenWidth);
    lv_obj_set_pos(cardinal_, 0, kCardinalTop);
    lv_label_set_text(cardinal_, "");

    // Created last so they draw over the card, and hidden until there is a heading. An
    // unhidden marker at a default bearing would be the exact failure this mode is built
    // to avoid: a compass pointing somewhere when it knows nothing.
    for (std::size_t index = 0; index < marker_.size(); ++index) {
        marker_[index] = make_dot(root_, kMarkerSizes[index], kMarker);
        lv_obj_add_flag(marker_[index], LV_OBJ_FLAG_HIDDEN);
    }

    status_shown_[0] = '\0';
    cardinal_shown_[0] = '\0';
}

void CompassScreen::update(const CompassViewModel& model) noexcept
{
    if (std::strcmp(status_shown_.data(), model.status.data()) != 0) {
        lv_label_set_text(status_, model.status.data());
        lv_obj_set_style_text_color(status_, lv_color_hex(model.status_rgb), 0);
        std::snprintf(status_shown_.data(), status_shown_.size(), "%s", model.status.data());
        for (auto* cell : digits_) {
            lv_obj_set_style_text_color(
                cell, lv_color_hex(model.heading_valid ? color::text_primary
                                                       : model.status_rgb),
                0);
        }
    }

    if (model.heading_valid != marker_visible_) {
        marker_visible_ = model.heading_valid;
        for (auto* dot : marker_) {
            if (marker_visible_) {
                lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            }
        }
        // Forget where it was, so the next heading always repositions rather than
        // reappearing at the bearing it held before the car stopped.
        marker_bearing_ = -1;
    }

    if (marker_visible_) {
        // Moved only when the bearing has changed by a whole degree. At 20 Hz the
        // receiver reports a new course twenty times a second and most of them land on
        // the same pixel; this is not smoothing, because the value shown is still exactly
        // what the receiver said.
        const auto bearing = static_cast<int>(std::lround(model.heading_deg)) % 360;
        if (bearing != marker_bearing_) {
            marker_bearing_ = bearing;
            for (std::size_t index = 0; index < marker_.size(); ++index) {
                place(marker_[index], static_cast<float>(bearing), kMarkerRadii[index],
                      kMarkerSizes[index]);
            }
        }
    }

    // "---" fills the same three cells the digits use, so the layout does not move
    // between a known heading and an unknown one.
    for (std::size_t index = 0; index < digits_.size(); ++index) {
        const char character = model.degrees[index];
        if (character == digits_shown_[index]) {
            continue;
        }
        digits_shown_[index] = character;
        const char text[2] = {character, '\0'};
        lv_label_set_text(digits_[index], text);
    }

    if (std::strcmp(cardinal_shown_.data(), model.cardinal.data()) != 0) {
        lv_label_set_text(cardinal_, model.cardinal.data());
        std::snprintf(cardinal_shown_.data(), cardinal_shown_.size(), "%s",
                      model.cardinal.data());
    }
}

}  // namespace track_timer::ui

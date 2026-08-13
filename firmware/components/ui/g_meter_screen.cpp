#include "track_timer/ui/g_meter_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kMeterCenterX = 180;
constexpr std::int32_t kMeterCenterY = 226;
constexpr std::int32_t kMeterRadius = 118;

std::uint32_t state_color(const ImuMeterState state) noexcept
{
    switch (state) {
    case ImuMeterState::ready:
    case ImuMeterState::recovered:
        return color::positive_bright;
    case ImuMeterState::calibrating:
    case ImuMeterState::partial:
        return color::caution_bright;
    case ImuMeterState::unavailable:
        return color::critical_bright;
    }
    return color::critical_bright;
}

double displayed_g(const float value) noexcept
{
    return std::fabs(value) < 0.005F ? 0.0 : static_cast<double>(value);
}

lv_obj_t* make_button(lv_obj_t* parent, const char* text, const std::int32_t y,
                      const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(parent);
    style_flat_panel(button, background_rgb, 10);
    lv_obj_set_pos(button, 350, y);
    lv_obj_set_size(button, 230, 56);
    auto* label = create_label(button, Typography::body, contrast_text_rgb(background_rgb));
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

}  // namespace

GmeterScreen::GmeterScreen(lv_obj_t* root, const GmeterCallback callback,
                           void* context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    style_screen(root_);

    auto* title = create_label(root_, Typography::heading, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title, "G-METER");
    lv_obj_set_pos(title, 20, 16);
    lv_obj_set_size(title, 180, 34);

    status_ = create_label(root_, Typography::caption, color::critical_bright,
                           LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(status_, 205, 20);
    lv_obj_set_size(status_, 375, 24);

    auto* meter = lv_obj_create(root_);
    style_flat_panel(meter, color::surface, 145);
    lv_obj_set_pos(meter, 35, 81);
    lv_obj_set_size(meter, 290, 290);

    for (const auto coordinates : std::array<std::array<std::int32_t, 4>, 2>{
             std::array<std::int32_t, 4>{55, kMeterCenterY, 250, 2},
             std::array<std::int32_t, 4>{kMeterCenterX, 101, 2, 250}}) {
        auto* line = lv_obj_create(root_);
        style_flat_panel(line, 0x4A4A4A);
        lv_obj_set_pos(line, coordinates[0], coordinates[1]);
        lv_obj_set_size(line, coordinates[2], coordinates[3]);
    }

    direction_labels_[0] = create_label(root_, Typography::caption, color::text_secondary);
    lv_obj_set_pos(direction_labels_[0], 105, 88);
    lv_obj_set_size(direction_labels_[0], 150, 22);
    direction_labels_[1] = create_label(root_, Typography::caption, color::text_secondary);
    lv_obj_set_pos(direction_labels_[1], 105, 341);
    lv_obj_set_size(direction_labels_[1], 150, 22);
    direction_labels_[2] = create_label(root_, Typography::caption, color::text_secondary,
                                        LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(direction_labels_[2], 43, 205);
    lv_obj_set_size(direction_labels_[2], 105, 22);
    direction_labels_[3] = create_label(root_, Typography::caption, color::text_secondary,
                                        LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(direction_labels_[3], 212, 205);
    lv_obj_set_size(direction_labels_[3], 105, 22);

    for (std::size_t index = 0; index < trail_markers_.size(); ++index) {
        trail_markers_[index] = lv_obj_create(root_);
        style_flat_panel(trail_markers_[index], color::positive_bright, 4);
        lv_obj_set_size(trail_markers_[index], 6, 6);
        lv_obj_add_flag(trail_markers_[index], LV_OBJ_FLAG_HIDDEN);
    }

    peak_marker_ = lv_obj_create(root_);
    style_flat_panel(peak_marker_, color::caution_bright, 8);
    lv_obj_set_size(peak_marker_, 14, 14);
    lv_obj_add_flag(peak_marker_, LV_OBJ_FLAG_HIDDEN);

    current_marker_ = lv_obj_create(root_);
    style_flat_panel(current_marker_, color::text_primary, 10);
    lv_obj_set_size(current_marker_, 18, 18);
    lv_obj_add_flag(current_marker_, LV_OBJ_FLAG_HIDDEN);

    state_message_ = create_label(root_, Typography::body, color::critical_bright);
    lv_obj_set_pos(state_message_, 60, 180);
    lv_obj_set_size(state_message_, 240, 86);
    lv_label_set_long_mode(state_message_, LV_LABEL_LONG_WRAP);

    auto* summary = lv_obj_create(root_);
    style_flat_panel(summary, color::surface, 12);
    lv_obj_set_pos(summary, 350, 58);
    lv_obj_set_size(summary, 230, 252);
    auto* summary_title = create_label(summary, Typography::caption, color::text_secondary,
                                       LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(summary_title, "CURRENT / SESSION PEAKS");
    lv_obj_set_pos(summary_title, 15, 12);
    lv_obj_set_size(summary_title, 200, 22);
    current_values_ = create_label(summary, Typography::body, color::text_primary,
                                   LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(current_values_, 15, 42);
    lv_obj_set_size(current_values_, 200, 52);

    constexpr std::array<const char*, 5> captions{"ACCEL", "BRAKE", "LEFT", "RIGHT",
                                                   "TOTAL"};
    for (std::size_t index = 0; index < peak_values_.size(); ++index) {
        peak_values_[index] = create_label(summary, Typography::caption,
                                           index == 4 ? color::caution_bright
                                                      : color::text_primary,
                                           LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(peak_values_[index], 15, 102 + static_cast<std::int32_t>(index) * 28);
        lv_obj_set_size(peak_values_[index], 200, 22);
        lv_label_set_text_fmt(peak_values_[index], "%s 0.00 G", captions[index]);
    }

    buttons_[0] = make_button(root_, "RESET PEAKS", 322, color::surface);
    buttons_[1] = make_button(root_, LV_SYMBOL_LEFT " BACK", 386, color::surface);
    bindings_[0] = {this, GmeterAction::reset};
    bindings_[1] = {this, GmeterAction::back};
    lv_obj_add_event_cb(buttons_[0], button_event, LV_EVENT_CLICKED, &bindings_[0]);
    lv_obj_add_event_cb(buttons_[1], button_event, LV_EVENT_CLICKED, &bindings_[1]);
}

void GmeterScreen::update(const ImuMeterSnapshot& snapshot) noexcept
{
    lv_label_set_text(status_, imu_meter_status_text(snapshot.state));
    lv_obj_set_style_text_color(status_, lv_color_hex(state_color(snapshot.state)), 0);
    update_direction_labels(snapshot.orientation);
    lv_label_set_text_fmt(current_values_, "LAT %+0.2f G\nLONG %+0.2f G",
                          displayed_g(snapshot.current.lateral_g),
                          displayed_g(snapshot.current.longitudinal_g));

    const std::array<float, 5> values{
        snapshot.peaks.acceleration_g, snapshot.peaks.braking_g, snapshot.peaks.left_g,
        snapshot.peaks.right_g, snapshot.peaks.total_g};
    constexpr std::array<const char*, 5> captions{"ACCEL", "BRAKE", "LEFT", "RIGHT",
                                                   "TOTAL"};
    for (std::size_t index = 0; index < values.size(); ++index) {
        lv_label_set_text_fmt(peak_values_[index], "%s %.2f G", captions[index],
                              static_cast<double>(values[index]));
    }

    const auto first = snapshot.trail_count == kImuTrailCapacity ? snapshot.trail_next : 0;
    for (std::size_t index = 0; index < trail_markers_.size(); ++index) {
        if (index < snapshot.trail_count) {
            const auto point = snapshot.trail[(first + index) % kImuTrailCapacity];
            position_marker(trail_markers_[index], point, 6);
            const auto opacity = static_cast<lv_opa_t>(
                40 + (180 * (index + 1)) / std::max<std::size_t>(1, snapshot.trail_count));
            lv_obj_set_style_bg_opa(trail_markers_[index], opacity, 0);
            lv_obj_remove_flag(trail_markers_[index], LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(trail_markers_[index], LV_OBJ_FLAG_HIDDEN);
        }
    }

    const bool current_available = snapshot.current.lateral_valid ||
                                   snapshot.current.longitudinal_valid;
    if (current_available) {
        position_marker(current_marker_, snapshot.current, 18);
        lv_obj_remove_flag(current_marker_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(current_marker_, LV_OBJ_FLAG_HIDDEN);
    }
    if (snapshot.peaks.total_g > 0.0F) {
        position_marker(peak_marker_, snapshot.peaks.total_position, 14);
        lv_obj_remove_flag(peak_marker_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(peak_marker_, LV_OBJ_FLAG_HIDDEN);
    }

    if (snapshot.state == ImuMeterState::calibrating ||
        snapshot.state == ImuMeterState::unavailable) {
        lv_label_set_text(state_message_, snapshot.state == ImuMeterState::calibrating
                                               ? "KEEP DEVICE LEVEL\nCALIBRATION IN PROGRESS"
                                               : "NO ACCELERATION DATA\nTIMING CONTINUES NORMALLY");
        lv_obj_set_style_text_color(state_message_, lv_color_hex(state_color(snapshot.state)), 0);
        lv_obj_remove_flag(state_message_, LV_OBJ_FLAG_HIDDEN);
        for (auto* label : direction_labels_) {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    else {
        lv_obj_add_flag(state_message_, LV_OBJ_FLAG_HIDDEN);
        for (auto* label : direction_labels_) {
            lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (snapshot.reset_allowed) {
        lv_obj_remove_state(buttons_[0], LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(buttons_[0], LV_STATE_DISABLED);
    }
}

void GmeterScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        lv_group_add_obj(group, button);
    }
}

lv_obj_t* GmeterScreen::button_for(const GmeterAction action) const noexcept
{
    return action == GmeterAction::reset ? buttons_[0] : buttons_[1];
}

lv_obj_t* GmeterScreen::status_object() const noexcept
{
    return status_;
}

lv_obj_t* GmeterScreen::direction_object(const std::size_t index) const noexcept
{
    return index < direction_labels_.size() ? direction_labels_[index] : nullptr;
}

lv_obj_t* GmeterScreen::trail_object(const std::size_t index) const noexcept
{
    return index < trail_markers_.size() ? trail_markers_[index] : nullptr;
}

lv_obj_t* GmeterScreen::peak_marker_object() const noexcept
{
    return peak_marker_;
}

void GmeterScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->screen != nullptr &&
        binding->screen->callback_ != nullptr) {
        binding->screen->callback_(binding->action, binding->screen->context_);
    }
}

void GmeterScreen::update_direction_labels(const board::DisplayOrientation orientation) noexcept
{
    const char* acceleration = "ACCEL (+Y)";
    const char* braking = "BRAKE (-Y)";
    const char* left = "LEFT (-X)";
    const char* right = "RIGHT (+X)";
    switch (orientation) {
    case board::DisplayOrientation::degrees_0:
        break;
    case board::DisplayOrientation::degrees_90:
        acceleration = "ACCEL (+X)";
        braking = "BRAKE (-X)";
        left = "LEFT (+Y)";
        right = "RIGHT (-Y)";
        break;
    case board::DisplayOrientation::degrees_180:
        acceleration = "ACCEL (-Y)";
        braking = "BRAKE (+Y)";
        left = "LEFT (+X)";
        right = "RIGHT (-X)";
        break;
    case board::DisplayOrientation::degrees_270:
        acceleration = "ACCEL (-X)";
        braking = "BRAKE (+X)";
        left = "LEFT (-Y)";
        right = "RIGHT (+Y)";
        break;
    }
    lv_label_set_text(direction_labels_[0], acceleration);
    lv_label_set_text(direction_labels_[1], braking);
    lv_label_set_text(direction_labels_[2], left);
    lv_label_set_text(direction_labels_[3], right);
}

void GmeterScreen::position_marker(lv_obj_t* marker, const PlanarAcceleration& point,
                                   const std::int32_t diameter) noexcept
{
    const auto x = static_cast<std::int32_t>(std::lround(
        static_cast<double>(point.lateral_g / kImuDisplayLimitG) * kMeterRadius));
    const auto y = static_cast<std::int32_t>(std::lround(
        static_cast<double>(point.longitudinal_g / kImuDisplayLimitG) * kMeterRadius));
    lv_obj_set_pos(marker, kMeterCenterX + x - diameter / 2,
                   kMeterCenterY - y - diameter / 2);
}

}  // namespace track_timer::ui

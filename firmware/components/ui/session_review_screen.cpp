#include "track_timer/ui/session_review_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::ui {
namespace {

void set_enabled(lv_obj_t* object, const bool enabled) noexcept
{
    if (enabled) {
        lv_obj_remove_state(object, LV_STATE_DISABLED);
    }
    else {
        lv_obj_add_state(object, LV_STATE_DISABLED);
    }
}

}  // namespace

SessionReviewScreen::SessionReviewScreen(lv_obj_t* root,
                                         const SessionReviewCallback callback,
                                         void* context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    style_screen(root_);
    title_ = create_label(root_, Typography::heading, color::text_primary,
                          LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(title_, 20, 14);
    lv_obj_set_size(title_, 300, 36);

    buttons_[0] = make_button(0, SessionReviewAction::newer_session,
                              LV_SYMBOL_LEFT " NEWER", 330, 8, 120, 56, color::surface);
    buttons_[1] = make_button(1, SessionReviewAction::older_session,
                              "OLDER " LV_SYMBOL_RIGHT, 460, 8, 120, 56, color::surface);

    duration_ = create_label(root_, Typography::timer_secondary, color::text_primary,
                             LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(duration_, 20, 62);
    lv_obj_set_size(duration_, 300, 34);
    overrun_ = create_label(root_, Typography::timer_secondary, color::caution_bright,
                            LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(overrun_, 320, 62);
    lv_obj_set_size(overrun_, 260, 34);
    completion_ = create_label(root_, Typography::body, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(completion_, 20, 100);
    lv_obj_set_size(completion_, 300, 26);

    integrity_ = create_label(root_, Typography::body, color::text_secondary,
                              LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(integrity_, 320, 100);
    lv_obj_set_size(integrity_, 260, 26);
    message_ = create_label(root_, Typography::body, color::caution_bright,
                            LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(message_, 20, 288);
    lv_obj_set_size(message_, 560, 26);

    // The peaks occupy the space the lap rows will take once there is a receiver, so they
    // are shown only when a session has no laps to list - which is every session today.
    peak_caption_ = create_label(root_, Typography::body, color::text_secondary,
                                 LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(peak_caption_, 20, 138);
    lv_obj_set_size(peak_caption_, 200, 26);
    lv_label_set_text(peak_caption_, "PEAK G");
    peak_total_ = create_label(root_, Typography::timer_primary, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(peak_total_, 20, 160);
    lv_obj_set_size(peak_total_, 250, 56);
    peak_longitudinal_ = create_label(root_, Typography::body, color::text_primary,
                                      LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(peak_longitudinal_, 290, 152);
    lv_obj_set_size(peak_longitudinal_, 290, 26);
    peak_lateral_ = create_label(root_, Typography::body, color::text_primary,
                                 LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(peak_lateral_, 290, 180);
    lv_obj_set_size(peak_lateral_, 290, 26);
    peak_vertical_ = create_label(root_, Typography::body, color::text_primary,
                                  LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(peak_vertical_, 290, 208);
    lv_obj_set_size(peak_vertical_, 290, 26);

    for (std::size_t index = 0; index < lap_panels_.size(); ++index) {
        auto* panel = lv_obj_create(root_);
        style_flat_panel(panel, color::surface, 8);
        lv_obj_set_pos(panel, 20, 152 + static_cast<std::int32_t>(index * 40));
        lv_obj_set_size(panel, 560, 36);
        lap_panels_[index] = panel;
        lap_labels_[index] = create_label(panel, Typography::caption, color::text_primary,
                                          LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(lap_labels_[index], 12, 7);
        lv_obj_set_size(lap_labels_[index], 100, 22);
        lap_durations_[index] = create_label(panel, Typography::body, color::text_primary);
        lv_obj_set_pos(lap_durations_[index], 150, 3);
        lv_obj_set_size(lap_durations_[index], 190, 28);
        lap_emphasis_[index] = create_label(panel, Typography::caption,
                                            color::positive_bright, LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_pos(lap_emphasis_[index], 340, 7);
        lv_obj_set_size(lap_emphasis_[index], 208, 22);
    }

    buttons_[2] = make_button(2, SessionReviewAction::previous_page,
                              LV_SYMBOL_LEFT " LAPS", 20, 322, 130, 108, color::surface);
    buttons_[3] = make_button(3, SessionReviewAction::next_page,
                              "LAPS " LV_SYMBOL_RIGHT, 160, 322, 130, 108, color::surface);
    buttons_[4] = make_button(4, SessionReviewAction::return_to_rest,
                              LV_SYMBOL_PAUSE " REST", 305, 322, 130, 108, color::caution);
    buttons_[5] = make_button(5, SessionReviewAction::return_to_ready,
                              LV_SYMBOL_HOME " READY", 445, 322, 135, 108, color::positive);
}

void SessionReviewScreen::update(const SessionReviewViewModel& model) noexcept
{
    lv_label_set_text(title_, model.title.data());
    lv_label_set_text(duration_, model.duration.data());
    lv_label_set_text(overrun_, model.overrun.data());
    lv_label_set_text(completion_, model.completion.data());
    lv_label_set_text(integrity_, model.integrity.data());
    lv_label_set_text(message_, model.message.data());
    lv_obj_set_style_text_color(
        integrity_,
        lv_color_hex(model.status == SessionReviewStatus::ready ? color::positive_bright
                     : model.status == SessionReviewStatus::partial_log
                         ? color::caution_bright
                         : color::text_secondary),
        0);

    lv_label_set_text(peak_total_, model.peak_total.data());
    lv_label_set_text(peak_longitudinal_, model.peak_longitudinal.data());
    lv_label_set_text(peak_lateral_, model.peak_lateral.data());
    lv_label_set_text(peak_vertical_, model.peak_vertical.data());

    // Peaks and lap rows share the same space, so only one of them is ever up.
    const auto show_peaks = model.lap_count == 0 && model.peak_total[0] != '\0';
    for (auto* label : {peak_caption_, peak_total_, peak_longitudinal_, peak_lateral_,
                        peak_vertical_}) {
        if (show_peaks) {
            lv_obj_remove_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
        else {
            lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (std::size_t index = 0; index < model.laps.size(); ++index) {
        const auto& row = model.laps[index];
        if (!row.visible) {
            lv_obj_add_flag(lap_panels_[index], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(lap_panels_[index], LV_OBJ_FLAG_HIDDEN);
        const auto panel_color = row.best ? color::positive
                                 : row.previous ? color::caution
                                                : color::surface;
        style_flat_panel(lap_panels_[index], panel_color, 8);
        const auto text_color = contrast_text_rgb(panel_color);
        lv_label_set_text(lap_labels_[index], row.lap.data());
        lv_label_set_text(lap_durations_[index], row.duration.data());
        lv_label_set_text(lap_emphasis_[index], row.emphasis.data());
        lv_obj_set_style_text_color(lap_labels_[index], lv_color_hex(text_color), 0);
        lv_obj_set_style_text_color(lap_durations_[index], lv_color_hex(text_color), 0);
        lv_obj_set_style_text_color(lap_emphasis_[index], lv_color_hex(text_color), 0);
    }

    set_enabled(buttons_[0], model.newer_session_enabled);
    set_enabled(buttons_[1], model.older_session_enabled);
    set_enabled(buttons_[2], model.previous_page_enabled);
    set_enabled(buttons_[3], model.next_page_enabled);
}

void SessionReviewScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        lv_group_add_obj(group, button);
    }
}

lv_obj_t* SessionReviewScreen::button_for(const SessionReviewAction action) const noexcept
{
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (bindings_[index].action == action) {
            return buttons_[index];
        }
    }
    return nullptr;
}

lv_obj_t* SessionReviewScreen::message_object() const noexcept
{
    return message_;
}

lv_obj_t* SessionReviewScreen::lap_object(const std::size_t index) const noexcept
{
    return index < lap_durations_.size() ? lap_durations_[index] : nullptr;
}

void SessionReviewScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->screen != nullptr &&
        binding->screen->callback_ != nullptr) {
        binding->screen->callback_(binding->action, binding->screen->context_);
    }
}

lv_obj_t* SessionReviewScreen::make_button(const std::size_t index,
                                           const SessionReviewAction action,
                                           const char* text, const std::int32_t x,
                                           const std::int32_t y, const std::int32_t width,
                                           const std::int32_t height,
                                           const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(root_);
    style_flat_panel(button, background_rgb, 10);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    auto* label = create_label(button, Typography::caption,
                               contrast_text_rgb(background_rgb));
    lv_label_set_text(label, text);
    lv_obj_center(label);
    bindings_[index] = Binding{this, action};
    lv_obj_add_event_cb(button, button_event, LV_EVENT_CLICKED, &bindings_[index]);
    return button;
}

}  // namespace track_timer::ui

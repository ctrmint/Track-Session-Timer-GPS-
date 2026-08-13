#include "track_timer/ui/diagnostics_screen.hpp"

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

DiagnosticsScreen::DiagnosticsScreen(lv_obj_t* root, const DiagnosticsCallback callback,
                                     void* context) noexcept
    : callback_(callback), context_(context), root_(root)
{
    style_screen(root_);
    auto* title = create_label(root_, Typography::heading, color::text_primary,
                               LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(title, LV_SYMBOL_WARNING " SYSTEM DIAGNOSTICS");
    lv_obj_set_pos(title, 20, 12);
    lv_obj_set_size(title, 410, 36);

    page_ = create_label(root_, Typography::caption, color::text_secondary,
                         LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(page_, 430, 17);
    lv_obj_set_size(page_, 150, 28);

    status_ = create_label(root_, Typography::caption, color::positive_bright,
                           LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(status_, 20, 56);
    lv_obj_set_size(status_, 560, 28);

    for (std::size_t index = 0; index < row_values_.size(); ++index) {
        auto* panel = lv_obj_create(root_);
        style_flat_panel(panel, color::surface, 5);
        lv_obj_set_pos(panel, 20, 92 + static_cast<std::int32_t>(index * 32));
        lv_obj_set_size(panel, 560, 29);
        auto* label = create_label(panel, Typography::caption, color::text_secondary,
                                   LV_TEXT_ALIGN_LEFT);
        lv_obj_set_pos(label, 10, 4);
        lv_obj_set_size(label, 250, 22);
        row_values_[index] = create_label(panel, Typography::caption, color::text_primary,
                                          LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_pos(row_values_[index], 250, 4);
        lv_obj_set_size(row_values_[index], 300, 22);
    }

    buttons_[0] = make_button(0, DiagnosticsAction::previous_page,
                              LV_SYMBOL_LEFT " PREVIOUS", 20, 160, color::surface);
    buttons_[1] = make_button(1, DiagnosticsAction::next_page,
                              "NEXT " LV_SYMBOL_RIGHT, 190, 160, color::surface);
    buttons_[2] = make_button(2, DiagnosticsAction::back,
                              LV_SYMBOL_LEFT " READY", 360, 220, color::positive);
}

void DiagnosticsScreen::update(const DiagnosticsViewModel& model) noexcept
{
    lv_label_set_text(status_, model.status.data());
    lv_obj_set_style_text_color(status_, lv_color_hex(model.status_color_rgb), 0);
    lv_label_set_text(page_, model.page.data());
    for (std::size_t index = 0; index < model.rows.size(); ++index) {
        auto* panel = lv_obj_get_parent(row_values_[index]);
        auto* label = lv_obj_get_child(panel, 0);
        lv_label_set_text(label, model.rows[index].label.data());
        lv_label_set_text(row_values_[index], model.rows[index].value.data());
        lv_obj_set_style_text_color(row_values_[index],
                                    lv_color_hex(model.rows[index].color_rgb), 0);
    }
    set_enabled(buttons_[0], model.previous_enabled);
    set_enabled(buttons_[1], model.next_enabled);
}

void DiagnosticsScreen::add_buttons_to_group(lv_group_t* group) noexcept
{
    for (auto* button : buttons_) {
        lv_group_add_obj(group, button);
    }
}

lv_obj_t* DiagnosticsScreen::button_for(const DiagnosticsAction action) const noexcept
{
    for (std::size_t index = 0; index < bindings_.size(); ++index) {
        if (bindings_[index].action == action) {
            return buttons_[index];
        }
    }
    return nullptr;
}

lv_obj_t* DiagnosticsScreen::page_object() const noexcept
{
    return page_;
}

lv_obj_t* DiagnosticsScreen::row_value_object(const std::size_t index) const noexcept
{
    return index < row_values_.size() ? row_values_[index] : nullptr;
}

void DiagnosticsScreen::button_event(lv_event_t* event) noexcept
{
    auto* binding = static_cast<Binding*>(lv_event_get_user_data(event));
    if (binding != nullptr && binding->screen != nullptr &&
        binding->screen->callback_ != nullptr) {
        binding->screen->callback_(binding->action, binding->screen->context_);
    }
}

lv_obj_t* DiagnosticsScreen::make_button(const std::size_t index,
                                         const DiagnosticsAction action,
                                         const char* text, const std::int32_t x,
                                         const std::int32_t width,
                                         const std::uint32_t background_rgb) noexcept
{
    auto* button = lv_button_create(root_);
    style_flat_panel(button, background_rgb, 10);
    lv_obj_set_pos(button, x, 364);
    lv_obj_set_size(button, width, 66);
    auto* label = create_label(button, Typography::caption,
                               contrast_text_rgb(background_rgb));
    lv_label_set_text(label, text);
    lv_obj_center(label);
    bindings_[index] = Binding{this, action};
    lv_obj_add_event_cb(button, button_event, LV_EVENT_CLICKED, &bindings_[index]);
    return button;
}

}  // namespace track_timer::ui

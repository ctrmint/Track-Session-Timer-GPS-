#include "device_screen.hpp"

namespace track_timer::simulator {
namespace {

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font, const lv_color_t color)
{
    auto* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

void configure_panel(lv_obj_t* panel) noexcept
{
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace

DeviceScreen::DeviceScreen(lv_obj_t* root) : root_(root)
{
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x050505), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

    lap_label_ = make_label(root_, &lv_font_montserrat_24, lv_color_white());
    lv_obj_set_pos(lap_label_, 20, 18);
    lv_obj_set_size(lap_label_, 170, 34);
    lv_obj_set_style_text_align(lap_label_, LV_TEXT_ALIGN_LEFT, 0);

    gnss_label_ = make_label(root_, &lv_font_montserrat_20, lv_color_hex(0x76FF9A));
    lv_obj_set_pos(gnss_label_, 370, 21);
    lv_obj_set_size(gnss_label_, 210, 30);
    lv_obj_set_style_text_align(gnss_label_, LV_TEXT_ALIGN_RIGHT, 0);

    accent_line_ = lv_obj_create(root_);
    configure_panel(accent_line_);
    lv_obj_set_pos(accent_line_, 0, 62);
    lv_obj_set_size(accent_line_, 600, 6);

    auto* current_caption = make_label(root_, &lv_font_montserrat_14, lv_color_hex(0x8F8F8F));
    lv_label_set_text(current_caption, "CURRENT LAP");
    lv_obj_set_pos(current_caption, 0, 86);
    lv_obj_set_size(current_caption, 600, 24);

    current_lap_label_ = make_label(root_, &lv_font_montserrat_48, lv_color_white());
    lv_obj_set_pos(current_lap_label_, 0, 112);
    lv_obj_set_size(current_lap_label_, 600, 62);

    auto* previous_caption = make_label(root_, &lv_font_montserrat_14, lv_color_hex(0x8F8F8F));
    lv_label_set_text(previous_caption, "LAST");
    lv_obj_set_pos(previous_caption, 30, 210);
    lv_obj_set_size(previous_caption, 250, 22);

    previous_lap_label_ = make_label(root_, &lv_font_montserrat_28, lv_color_white());
    lv_obj_set_pos(previous_lap_label_, 30, 236);
    lv_obj_set_size(previous_lap_label_, 250, 38);

    auto* best_caption = make_label(root_, &lv_font_montserrat_14, lv_color_hex(0x8F8F8F));
    lv_label_set_text(best_caption, "BEST");
    lv_obj_set_pos(best_caption, 320, 210);
    lv_obj_set_size(best_caption, 250, 22);

    best_lap_label_ = make_label(root_, &lv_font_montserrat_28, lv_color_white());
    lv_obj_set_pos(best_lap_label_, 320, 236);
    lv_obj_set_size(best_lap_label_, 250, 38);

    logging_badge_ = lv_obj_create(root_);
    configure_panel(logging_badge_);
    lv_obj_set_style_radius(logging_badge_, 14, 0);
    lv_obj_set_pos(logging_badge_, 240, 289);
    lv_obj_set_size(logging_badge_, 120, 30);

    logging_label_ = make_label(logging_badge_, &lv_font_montserrat_14, lv_color_white());
    lv_obj_center(logging_label_);

    session_panel_ = lv_obj_create(root_);
    configure_panel(session_panel_);
    lv_obj_set_pos(session_panel_, 0, 330);
    lv_obj_set_size(session_panel_, 600, 120);

    auto* session_caption = make_label(session_panel_, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(session_caption, "SESSION");
    lv_obj_set_pos(session_caption, 30, 16);
    lv_obj_set_size(session_caption, 150, 30);
    lv_obj_set_style_text_align(session_caption, LV_TEXT_ALIGN_LEFT, 0);

    session_label_ = make_label(session_panel_, &lv_font_montserrat_48, lv_color_white());
    lv_obj_set_pos(session_label_, 190, 25);
    lv_obj_set_size(session_label_, 380, 60);
    lv_obj_set_style_text_align(session_label_, LV_TEXT_ALIGN_RIGHT, 0);
}

void DeviceScreen::update(const ui::DeviceViewModel& model) noexcept
{
    lv_label_set_text(lap_label_, model.lap_label.data());
    lv_label_set_text(gnss_label_, model.gnss_status.data());
    lv_label_set_text(current_lap_label_, model.current_lap.data());
    lv_label_set_text(previous_lap_label_, model.previous_lap.data());
    lv_label_set_text(best_lap_label_, model.best_lap.data());
    lv_label_set_text(logging_label_, model.logging_status.data());
    lv_label_set_text(session_label_, model.session_remaining.data());

    const auto accent = lv_color_hex(model.accent_rgb);
    const auto accent_text = lv_color_hex(model.accent_text_rgb);
    lv_obj_set_style_bg_color(accent_line_, accent, 0);
    lv_obj_set_style_bg_color(session_panel_, accent, 0);
    lv_obj_set_style_text_color(session_label_, accent_text, 0);

    const bool logging = model.logging_status[0] != 'N';
    lv_obj_set_style_bg_color(logging_badge_, lv_color_hex(logging ? 0x1B5E20 : 0xB71C1C), 0);
}

}  // namespace track_timer::simulator

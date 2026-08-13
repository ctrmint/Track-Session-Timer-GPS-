#include "device_screen.hpp"

#include "track_timer/ui/foundation.hpp"
#include "track_timer/ui/lvgl_visual_system.hpp"

namespace track_timer::simulator {
namespace {

constexpr std::size_t kLapTimeCellCount = 10;
constexpr std::size_t kSessionTimeCellCount = 7;
constexpr auto kLargeLapTimeCellWidths = lap_time_cell_widths(34, 16, 12);
constexpr auto kSmallLapTimeCellWidths = lap_time_cell_widths(20, 10, 8);
constexpr auto kSessionTimeCellWidths = session_time_cell_widths(34, 16);
constexpr std::uint32_t gnss_indicator_color(const domain::GnssHealth health) noexcept
{
    switch (health) {
    case domain::GnssHealth::good:
        return ui::color::positive_bright;
    case domain::GnssHealth::poor:
        return ui::color::caution_bright;
    case domain::GnssHealth::unavailable:
    case domain::GnssHealth::searching:
    case domain::GnssHealth::stale:
        return ui::color::critical_bright;
    }
    return ui::color::critical_bright;
}

const char* gnss_indicator_text(const domain::GnssHealth health) noexcept
{
    switch (health) {
    case domain::GnssHealth::unavailable:
        return LV_SYMBOL_GPS " NO GPS";
    case domain::GnssHealth::searching:
        return LV_SYMBOL_GPS " SEARCH";
    case domain::GnssHealth::poor:
        return LV_SYMBOL_GPS " POOR";
    case domain::GnssHealth::good:
        return LV_SYMBOL_GPS " GOOD";
    case domain::GnssHealth::stale:
        return LV_SYMBOL_GPS " STALE";
    }
    return LV_SYMBOL_GPS " NO GPS";
}

constexpr std::int32_t centered_lap_field_x(const std::int32_t area_x,
                                            const std::int32_t area_width,
                                            const std::int32_t total_width,
                                            const std::int32_t digit_width) noexcept
{
    constexpr std::int32_t kReservedLeadingCells = 2;
    const auto leading_width = kReservedLeadingCells * digit_width;
    const auto common_value_width = total_width - leading_width;
    return area_x + (area_width - common_value_width) / 2 - leading_width;
}

}  // namespace

DeviceScreen::DeviceScreen(lv_obj_t* root) : root_(root)
{
    ui::style_screen(root_);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);

    lap_label_ = ui::create_label(root_, ui::Typography::heading, ui::color::text_primary,
                                  LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(lap_label_, 20, 18);
    lv_obj_set_size(lap_label_, 170, 34);

    gnss_label_ = ui::create_label(root_, ui::Typography::body,
                                   ui::color::critical_bright, LV_TEXT_ALIGN_RIGHT);
    lv_label_set_text(gnss_label_, gnss_indicator_text(domain::GnssHealth::unavailable));
    lv_obj_set_pos(gnss_label_, 370, 21);
    lv_obj_set_size(gnss_label_, 210, 30);

    accent_line_ = lv_obj_create(root_);
    ui::style_flat_panel(accent_line_, ui::color::surface);
    lv_obj_set_pos(accent_line_, 0, 62);
    lv_obj_set_size(accent_line_, 600, 6);

    auto* current_caption = ui::create_label(root_, ui::Typography::caption,
                                             ui::color::text_secondary);
    lv_label_set_text(current_caption, "CURRENT LAP");
    lv_obj_set_pos(current_caption, 0, 86);
    lv_obj_set_size(current_caption, 600, 24);

    current_lap_label_.create(root_, &lv_font_montserrat_48, lv_color_white(),
                              kLapTimeCellCount, kLargeLapTimeCellWidths, 62);
    current_lap_label_.set_position(
        centered_lap_field_x(0, 600, current_lap_label_.width(), 34), 112);

    auto* previous_caption = ui::create_label(root_, ui::Typography::caption,
                                              ui::color::text_secondary);
    lv_label_set_text(previous_caption, "LAST");
    lv_obj_set_pos(previous_caption, 30, 210);
    lv_obj_set_size(previous_caption, 250, 22);

    previous_lap_label_.create(root_, &lv_font_montserrat_28, lv_color_white(),
                               kLapTimeCellCount, kSmallLapTimeCellWidths, 38);
    previous_lap_label_.set_position(
        centered_lap_field_x(30, 250, previous_lap_label_.width(), 20), 236);

    auto* best_caption = ui::create_label(root_, ui::Typography::caption,
                                          ui::color::text_secondary);
    lv_label_set_text(best_caption, "BEST");
    lv_obj_set_pos(best_caption, 320, 210);
    lv_obj_set_size(best_caption, 250, 22);

    best_lap_label_.create(root_, &lv_font_montserrat_28, lv_color_white(),
                           kLapTimeCellCount, kSmallLapTimeCellWidths, 38);
    best_lap_label_.set_position(
        centered_lap_field_x(320, 250, best_lap_label_.width(), 20), 236);

    logging_badge_ = lv_obj_create(root_);
    ui::style_flat_panel(logging_badge_, ui::color::logging_unavailable, 14);
    lv_obj_set_pos(logging_badge_, 240, 289);
    lv_obj_set_size(logging_badge_, 120, 30);

    logging_label_ = ui::create_label(logging_badge_, ui::Typography::caption,
                                      ui::color::text_primary);
    lv_obj_center(logging_label_);

    session_panel_ = lv_obj_create(root_);
    ui::style_flat_panel(session_panel_, ui::color::surface);
    lv_obj_set_pos(session_panel_, 0, 330);
    lv_obj_set_size(session_panel_, 600, 120);

    session_caption_ = ui::create_label(session_panel_, ui::Typography::body,
                                        ui::color::text_primary, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(session_caption_, "SESSION");
    lv_obj_set_pos(session_caption_, 30, 16);
    lv_obj_set_size(session_caption_, 200, 30);

    session_label_.create(session_panel_, &lv_font_montserrat_48, lv_color_white(),
                          kSessionTimeCellCount, kSessionTimeCellWidths, 60);
    session_label_.set_position(570 - session_label_.width(), 25);
}

void DeviceScreen::update(const ui::DeviceViewModel& model) noexcept
{
    lv_label_set_text(lap_label_, model.lap_label.data());
    lv_label_set_text(gnss_label_, gnss_indicator_text(model.gnss_health));
    lv_obj_set_style_text_color(gnss_label_, lv_color_hex(gnss_indicator_color(model.gnss_health)),
                                0);
    current_lap_label_.set_text(model.current_lap.data());
    previous_lap_label_.set_text(model.previous_lap.data());
    best_lap_label_.set_text(model.best_lap.data());
    lv_label_set_text(logging_label_, model.logging_status.data());
    lv_label_set_text(session_caption_, model.session_status.data());
    session_label_.set_text(model.session_remaining.data());

    const auto accent = lv_color_hex(model.accent_rgb);
    const auto accent_text = lv_color_hex(model.accent_text_rgb);
    lv_obj_set_style_bg_color(accent_line_, accent, 0);
    lv_obj_set_style_bg_color(session_panel_, accent, 0);
    lv_obj_set_style_text_color(session_caption_, accent_text, 0);
    session_label_.set_color(accent_text);

    const bool logging = model.logging_status[0] != 'N';
    lv_obj_set_style_bg_color(
        logging_badge_,
        lv_color_hex(logging ? ui::color::logging : ui::color::logging_unavailable), 0);
}

}  // namespace track_timer::simulator

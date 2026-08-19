#include "track_timer/ui/g_radar_screen.hpp"

#include "track_timer/ui/lvgl_visual_system.hpp"

#include <algorithm>
#include <cmath>

namespace track_timer::ui {
namespace {

constexpr std::int32_t kCentreX = 222;
constexpr std::int32_t kCentreY = 232;
constexpr std::int32_t kOuterRadius = 190;
constexpr std::int32_t kDotSize = 9;
constexpr std::int32_t kCurrentSize = 20;
constexpr std::int32_t kPeakSize = 16;

constexpr float kRingG[kRadarRingCount] = {0.5F, 1.0F, kRadarFullScaleG};

constexpr std::uint32_t kBackground = 0x000000;
constexpr std::uint32_t kGrid = 0x2A3340;
constexpr std::uint32_t kGridBright = 0x415064;
constexpr std::uint32_t kMuted = 0x7C8899;
constexpr std::uint32_t kTrail = 0x35B0FF;
constexpr std::uint32_t kCurrent = 0xFFFFFF;
constexpr std::uint32_t kPeak = 0xFF8FA3;

// Pixels per g. The outer ring is the full scale, so everything else follows from it.
constexpr float kPixelsPerG = static_cast<float>(kOuterRadius) / kRadarFullScaleG;

lv_obj_t* make_dot(lv_obj_t* parent, std::int32_t size, std::uint32_t rgb) noexcept
{
    auto* dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(rgb), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    return dot;
}

}  // namespace

GRadarScreen::GRadarScreen(lv_obj_t* const root) noexcept : root_(root)
{
    style_screen(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kBackground), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);

    // Crosshair first, so the rings and markers draw over it.
    for (std::size_t index = 0; index < crosshairs_.size(); ++index) {
        auto* line = lv_obj_create(root_);
        lv_obj_remove_style_all(line);
        const auto span = kOuterRadius * 2;
        lv_obj_set_size(line, index == 0 ? span : 1, index == 0 ? 1 : span);
        lv_obj_set_pos(line, index == 0 ? kCentreX - kOuterRadius : kCentreX,
                       index == 0 ? kCentreY : kCentreY - kOuterRadius);
        lv_obj_set_style_bg_color(line, lv_color_hex(kGrid), 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
        lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
        crosshairs_[index] = line;
    }

    for (std::size_t index = 0; index < kRadarRingCount; ++index) {
        const auto radius =
            static_cast<std::int32_t>(std::lround(kRingG[index] * kPixelsPerG));
        auto* ring = lv_obj_create(root_);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, radius * 2, radius * 2);
        lv_obj_set_pos(ring, kCentreX - radius, kCentreY - radius);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ring, index + 1 == kRadarRingCount ? 2 : 1, 0);
        lv_obj_set_style_border_color(
            ring, lv_color_hex(index + 1 == kRadarRingCount ? kGridBright : kGrid), 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        rings_[index] = ring;

        // Ring magnitudes are labelled, or the rings mean nothing to a driver.
        auto* label = create_label(root_, Typography::caption, kMuted);
        lv_label_set_text_fmt(label, "%d.%d", static_cast<int>(kRingG[index]),
                              static_cast<int>(kRingG[index] * 10) % 10);
        lv_obj_set_pos(label, kCentreX + 6, kCentreY - radius - 4);
        ring_labels_[index] = label;
    }

    // Named for what the driver feels, not for the axis sign.
    static constexpr const char* kAxisText[] = {"ACCEL", "BRAKE", "L", "R"};
    static constexpr std::int32_t kAxisX[] = {kCentreX - 28, kCentreX - 26,
                                              kCentreX - kOuterRadius - 22,
                                              kCentreX + kOuterRadius + 8};
    static constexpr std::int32_t kAxisY[] = {kCentreY - kOuterRadius - 26,
                                              kCentreY + kOuterRadius + 6, kCentreY - 10,
                                              kCentreY - 10};
    for (std::size_t index = 0; index < axis_labels_.size(); ++index) {
        auto* label = create_label(root_, Typography::caption, kMuted);
        lv_label_set_text(label, kAxisText[index]);
        lv_obj_set_pos(label, kAxisX[index], kAxisY[index]);
        axis_labels_[index] = label;
    }

    for (auto*& dot : trail_) {
        dot = make_dot(root_, kDotSize, kTrail);
    }
    peak_marker_ = make_dot(root_, kPeakSize, kPeak);
    current_ = make_dot(root_, kCurrentSize, kCurrent);

    // Numbers alongside the graphic: reviewing a session afterwards wants the value, not
    // only the picture.
    static constexpr const char* kPeakCaption[] = {"ACCEL", "BRAKE", "LEFT", "RIGHT",
                                                   "MAX"};
    for (std::size_t index = 0; index < peak_values_.size(); ++index) {
        auto* label = create_label(root_, Typography::body, kMuted, LV_TEXT_ALIGN_LEFT);
        lv_label_set_text_fmt(label, "%s  --", kPeakCaption[index]);
        lv_obj_set_pos(label, 440, 96 + static_cast<std::int32_t>(index) * 46);
        lv_obj_set_size(label, 150, 40);
        peak_values_[index] = label;
    }

    status_ = create_label(root_, Typography::body, kMuted, LV_TEXT_ALIGN_LEFT);
    lv_obj_set_pos(status_, 440, 40);
    lv_obj_set_size(status_, 150, 30);
    lv_label_set_text(status_, "");
}

lv_point_t GRadarScreen::plot(const PlanarAcceleration& point) const noexcept
{
    // Screen y grows downward, so acceleration (positive longitudinal) subtracts.
    const auto x = static_cast<std::int32_t>(
        std::lround(static_cast<double>(point.lateral_g) * kPixelsPerG));
    const auto y = static_cast<std::int32_t>(
        std::lround(static_cast<double>(point.longitudinal_g) * kPixelsPerG));
    return {kCentreX + x, kCentreY - y};
}

void GRadarScreen::update(const ImuMeterSnapshot& snapshot) noexcept
{
    lv_label_set_text(status_, imu_meter_status_text(snapshot.state));

    const auto usable = snapshot.state == ImuMeterState::ready;

    // Oldest first, so opacity can fade with age along the trace.
    for (std::size_t index = 0; index < trail_.size(); ++index) {
        auto* dot = trail_[index];
        if (!usable || index >= snapshot.trail_count) {
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const auto point = snapshot.trail[(snapshot.trail_next + trail_.size() -
                                           snapshot.trail_count + index) %
                                          trail_.size()];
        if (!point.lateral_valid || !point.longitudinal_valid) {
            lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const auto position = plot(point);
        lv_obj_set_pos(dot, position.x - kDotSize / 2, position.y - kDotSize / 2);
        const auto age = static_cast<std::int32_t>(
            LV_OPA_COVER * (index + 1) / std::max<std::size_t>(snapshot.trail_count, 1));
        lv_obj_set_style_bg_opa(
            dot, static_cast<lv_opa_t>(std::max<std::int32_t>(age, 30)), 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    }

    if (usable && snapshot.current.lateral_valid && snapshot.current.longitudinal_valid) {
        const auto position = plot(snapshot.current);
        lv_obj_set_pos(current_, position.x - kCurrentSize / 2,
                       position.y - kCurrentSize / 2);
        lv_obj_remove_flag(current_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(current_, LV_OBJ_FLAG_HIDDEN);
    }

    // The maximum is held where it happened, so a driver can see which corner produced it.
    const auto& peak_position = snapshot.peaks.total_position;
    if (usable && snapshot.peaks.total_g > 0.0F && peak_position.lateral_valid &&
        peak_position.longitudinal_valid) {
        const auto position = plot(peak_position);
        lv_obj_set_pos(peak_marker_, position.x - kPeakSize / 2,
                       position.y - kPeakSize / 2);
        lv_obj_remove_flag(peak_marker_, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(peak_marker_, LV_OBJ_FLAG_HIDDEN);
    }

    const float values[] = {snapshot.peaks.acceleration_g, snapshot.peaks.braking_g,
                            snapshot.peaks.left_g, snapshot.peaks.right_g,
                            snapshot.peaks.total_g};
    static constexpr const char* kCaption[] = {"ACCEL", "BRAKE", "LEFT", "RIGHT", "MAX"};
    for (std::size_t index = 0; index < peak_values_.size(); ++index) {
        if (usable) {
            lv_label_set_text_fmt(peak_values_[index], "%s  %d.%02d", kCaption[index],
                                  static_cast<int>(values[index]),
                                  static_cast<int>(values[index] * 100) % 100);
        }
        else {
            // An absent IMU must not read 0.00 G, which is a legitimate measurement.
            lv_label_set_text_fmt(peak_values_[index], "%s  --", kCaption[index]);
        }
        lv_obj_set_style_text_color(
            peak_values_[index],
            lv_color_hex(index + 1 == peak_values_.size() ? kPeak : kMuted), 0);
    }
}

lv_obj_t* GRadarScreen::root() const noexcept { return root_; }

}  // namespace track_timer::ui

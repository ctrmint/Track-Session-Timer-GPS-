#pragma once

#include "track_timer/ui/gps_only.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>

namespace track_timer::ui {

// GPS Only mode's panel: road speed large enough to read at speed, with the receiver's
// own report underneath it.
//
// Road speed uses the rasterised numeral face the countdown uses, for the same reason:
// LVGL's largest built-in font is 48 px, about 3.9 mm on this panel. That face is subset
// to digits and separators, so it cannot spell a placeholder - which is why the
// no-data case is a separate label in a built-in font rather than dashes in the big one.
class GpsOnlyScreen {
  public:
    explicit GpsOnlyScreen(lv_obj_t* root) noexcept;

    void update(const GpsOnlyViewModel& model) noexcept;

    GpsOnlyScreen(const GpsOnlyScreen&) = delete;
    GpsOnlyScreen& operator=(const GpsOnlyScreen&) = delete;

  private:
    static constexpr std::size_t kSpeedDigits = 3;

    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* unit_{nullptr};
    lv_obj_t* secondary_{nullptr};
    lv_obj_t* placeholder_{nullptr};
    std::array<lv_obj_t*, kSpeedDigits> digits_{};
    std::array<char, kSpeedDigits> digits_shown_{};
    std::array<lv_obj_t*, kGpsOnlyRowCount> labels_{};
    std::array<std::array<char, 8>, kGpsOnlyRowCount> labels_shown_{};
    std::array<lv_obj_t*, kGpsOnlyRowCount> values_{};
    std::array<std::array<char, 16>, kGpsOnlyRowCount> values_shown_{};
    std::array<char, 24> status_shown_{};
    std::array<char, 16> secondary_shown_{};
};

}  // namespace track_timer::ui

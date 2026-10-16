#pragma once

#include "track_timer/ui/acceleration.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>

namespace track_timer::ui {

// 0-X mode's panel: the elapsed time filling the width, the target beneath it.
//
// `12.34` is four digits and a point, and the numeral face carries `.` among its fourteen
// glyphs - so the whole reading renders in the large face. Its metrics come out identical
// to Clock's `HH:MM`: 536 px of the 540 available at 170 px, which is the countdown's size
// and already in the font cache.
//
// Two leading digits is the right width. A 0-60 is three to fifteen seconds, and a run
// still counting past ninety-nine did not happen - so the field is sized for every real
// measurement and no unreal one.
class AccelerationScreen {
  public:
    explicit AccelerationScreen(lv_obj_t* root) noexcept;

    void update(const AccelerationViewModel& model) noexcept;

    AccelerationScreen(const AccelerationScreen&) = delete;
    AccelerationScreen& operator=(const AccelerationScreen&) = delete;

  private:
    // "12.34" - four digits around a point that never moves.
    static constexpr std::size_t kTimeDigits = 4;

    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* target_{nullptr};
    lv_obj_t* point_{nullptr};
    std::array<lv_obj_t*, kTimeDigits> digits_{};
    std::array<char, kTimeDigits> digits_shown_{};
    std::array<char, kAccelerationStatusCapacity> status_shown_{};
    std::array<char, kAccelerationTargetCapacity> target_shown_{};
    std::uint32_t time_rgb_shown_{0};
};

}  // namespace track_timer::ui

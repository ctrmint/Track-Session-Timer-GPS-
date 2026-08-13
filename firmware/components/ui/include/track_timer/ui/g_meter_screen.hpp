#pragma once

#include "track_timer/ui/imu_meter.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class GmeterAction : std::uint8_t {
    reset,
    back,
};

using GmeterCallback = void (*)(GmeterAction action, void* context) noexcept;

class GmeterScreen {
  public:
    GmeterScreen(lv_obj_t* root, GmeterCallback callback, void* context) noexcept;

    void update(const ImuMeterSnapshot& snapshot) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;
    [[nodiscard]] lv_obj_t* button_for(GmeterAction action) const noexcept;
    [[nodiscard]] lv_obj_t* status_object() const noexcept;
    [[nodiscard]] lv_obj_t* direction_object(std::size_t index) const noexcept;
    [[nodiscard]] lv_obj_t* trail_object(std::size_t index) const noexcept;
    [[nodiscard]] lv_obj_t* peak_marker_object() const noexcept;

  private:
    struct Binding {
        GmeterScreen* screen{nullptr};
        GmeterAction action{GmeterAction::reset};
    };

    static void button_event(lv_event_t* event) noexcept;
    void update_direction_labels(board::DisplayOrientation orientation) noexcept;
    void position_marker(lv_obj_t* marker, const PlanarAcceleration& point,
                         std::int32_t diameter) noexcept;

    GmeterCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* state_message_{nullptr};
    lv_obj_t* current_values_{nullptr};
    std::array<lv_obj_t*, 5> peak_values_{};
    std::array<lv_obj_t*, 4> direction_labels_{};
    std::array<lv_obj_t*, kImuTrailCapacity> trail_markers_{};
    lv_obj_t* peak_marker_{nullptr};
    lv_obj_t* current_marker_{nullptr};
    std::array<lv_obj_t*, 2> buttons_{};
    std::array<Binding, 2> bindings_{};
};

}  // namespace track_timer::ui

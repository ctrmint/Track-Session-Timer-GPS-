#pragma once

#include "track_timer/ui/track_selection.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class TrackSelectionAction : std::uint8_t {
    previous,
    select,
    next,
    timer_only,
    capture_information,
    back,
};

using TrackSelectionCallback = void (*)(TrackSelectionAction action,
                                        void* context) noexcept;

class TrackSelectionScreen {
  public:
    TrackSelectionScreen(lv_obj_t* root, TrackSelectionCallback callback,
                         void* context) noexcept;
    void update(const TrackSelectionViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;
    [[nodiscard]] lv_obj_t* button_for(TrackSelectionAction action) const noexcept;
    [[nodiscard]] lv_obj_t* track_name_object() const noexcept;
    [[nodiscard]] lv_obj_t* status_object() const noexcept;

  private:
    struct Binding {
        TrackSelectionScreen* screen{nullptr};
        TrackSelectionAction action{TrackSelectionAction::previous};
    };
    static void button_event(lv_event_t* event) noexcept;
    lv_obj_t* make_button(std::size_t index, TrackSelectionAction action, const char* text,
                          std::int32_t x, std::int32_t y, std::int32_t width,
                          std::int32_t height, std::uint32_t background_rgb) noexcept;

    TrackSelectionCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* position_label_{nullptr};
    lv_obj_t* track_name_label_{nullptr};
    lv_obj_t* definition_label_{nullptr};
    lv_obj_t* status_label_{nullptr};
    std::array<lv_obj_t*, 6> buttons_{};
    std::array<Binding, 6> bindings_{};
};

}  // namespace track_timer::ui

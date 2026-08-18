#pragma once

#include "track_timer/ui/input.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

inline constexpr std::size_t kCarouselCapacity = 6;

struct CarouselEntry {
    const char* icon{nullptr};   // LV_SYMBOL_*
    const char* label{nullptr};
    std::uint32_t icon_rgb{0xFFFFFF};  // colour-coded by function
};

// One choice fills the panel: a large icon, a large label, pressable chevrons and a
// position indicator. Only one item is shown at a time, which is what buys the screen
// space that the previous dense menus lacked.
//
// The chevrons are deliberately pressable as well as decorative. A swipe is the primary
// gesture, but a gloved hand on a capacitive panel may not produce one, and every level
// has to remain escapable and navigable without it.
class CarouselScreen {
  public:
    CarouselScreen(lv_obj_t* root, InputCallback callback, void* context) noexcept;

    void set_entries(const CarouselEntry* entries, std::size_t count) noexcept;
    void set_position(std::size_t index) noexcept;
    void set_title(const char* title) noexcept;

    [[nodiscard]] lv_obj_t* root() const noexcept;

    CarouselScreen(const CarouselScreen&) = delete;
    CarouselScreen& operator=(const CarouselScreen&) = delete;

  private:
    static void chevron_event(lv_event_t* event) noexcept;
    void refresh() noexcept;

    InputCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* icon_{nullptr};
    lv_obj_t* label_{nullptr};
    lv_obj_t* hint_{nullptr};
    std::array<lv_obj_t*, 2> chevrons_{};
    std::array<lv_obj_t*, kCarouselCapacity> dots_{};
    std::array<CarouselEntry, kCarouselCapacity> entries_{};
    std::size_t count_{0};
    std::size_t index_{0};
};

}  // namespace track_timer::ui

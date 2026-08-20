#pragma once

#include "track_timer/ui/input.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

// Sized for a track list rather than a menu: the UK pack alone is 24 circuits.
inline constexpr std::size_t kCarouselCapacity = 34;

// Beyond this many entries the position dots stop being readable, so a "n / total"
// counter replaces them.
inline constexpr std::size_t kCarouselMaximumDots = 8;

struct CarouselEntry {
    const char* icon{nullptr};   // LV_SYMBOL_*
    const char* label{nullptr};
    std::uint32_t icon_rgb{0xFFFFFF};  // colour-coded by function
    // A second line under the label, for when the label alone does not say what it is: a
    // record value needs its name, and a choice benefits from a word on what it does.
    // Appended, because this struct is aggregate-initialised at every call site and a field
    // in the middle silently reassigns positional initialisers.
    const char* caption{nullptr};
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
    lv_obj_t* caption_{nullptr};
    lv_obj_t* hint_{nullptr};
    std::array<lv_obj_t*, 2> chevrons_{};
    std::array<lv_obj_t*, kCarouselMaximumDots> dots_{};
    lv_obj_t* position_text_{nullptr};
    std::array<CarouselEntry, kCarouselCapacity> entries_{};
    std::size_t count_{0};
    std::size_t index_{0};
};

}  // namespace track_timer::ui

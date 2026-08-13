#pragma once

#include "track_timer/ui/diagnostics.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class DiagnosticsAction : std::uint8_t {
    previous_page,
    next_page,
    back,
};

using DiagnosticsCallback = void (*)(DiagnosticsAction action, void* context) noexcept;

class DiagnosticsScreen {
  public:
    DiagnosticsScreen(lv_obj_t* root, DiagnosticsCallback callback, void* context) noexcept;
    void update(const DiagnosticsViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;
    [[nodiscard]] lv_obj_t* button_for(DiagnosticsAction action) const noexcept;
    [[nodiscard]] lv_obj_t* page_object() const noexcept;
    [[nodiscard]] lv_obj_t* row_value_object(std::size_t index) const noexcept;

  private:
    struct Binding {
        DiagnosticsScreen* screen{nullptr};
        DiagnosticsAction action{DiagnosticsAction::previous_page};
    };

    static void button_event(lv_event_t* event) noexcept;
    lv_obj_t* make_button(std::size_t index, DiagnosticsAction action, const char* text,
                          std::int32_t x, std::int32_t width,
                          std::uint32_t background_rgb) noexcept;

    DiagnosticsCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* status_{nullptr};
    lv_obj_t* page_{nullptr};
    std::array<lv_obj_t*, kDiagnosticsRowsPerPage> row_values_{};
    std::array<lv_obj_t*, 3> buttons_{};
    std::array<Binding, 3> bindings_{};
};

}  // namespace track_timer::ui

#pragma once

#include "track_timer/ui/session_review.hpp"

#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class SessionReviewAction : std::uint8_t {
    newer_session,
    older_session,
    previous_page,
    next_page,
    return_to_rest,
    return_to_ready,
};

using SessionReviewCallback = void (*)(SessionReviewAction action, void* context) noexcept;

class SessionReviewScreen {
  public:
    SessionReviewScreen(lv_obj_t* root, SessionReviewCallback callback,
                        void* context) noexcept;

    void update(const SessionReviewViewModel& model) noexcept;
    void add_buttons_to_group(lv_group_t* group) noexcept;
    [[nodiscard]] lv_obj_t* button_for(SessionReviewAction action) const noexcept;
    [[nodiscard]] lv_obj_t* message_object() const noexcept;
    [[nodiscard]] lv_obj_t* lap_object(std::size_t index) const noexcept;

  private:
    struct Binding {
        SessionReviewScreen* screen{nullptr};
        SessionReviewAction action{SessionReviewAction::newer_session};
    };

    static void button_event(lv_event_t* event) noexcept;
    lv_obj_t* make_button(std::size_t index, SessionReviewAction action, const char* text,
                          std::int32_t x, std::int32_t y, std::int32_t width,
                          std::int32_t height, std::uint32_t background_rgb) noexcept;

    SessionReviewCallback callback_{nullptr};
    void* context_{nullptr};
    lv_obj_t* root_{nullptr};
    lv_obj_t* title_{nullptr};
    lv_obj_t* duration_{nullptr};
    lv_obj_t* overrun_{nullptr};
    lv_obj_t* completion_{nullptr};
    lv_obj_t* integrity_{nullptr};
    lv_obj_t* message_{nullptr};
    std::array<lv_obj_t*, logger::kSummaryLapPageCapacity> lap_panels_{};
    std::array<lv_obj_t*, logger::kSummaryLapPageCapacity> lap_labels_{};
    std::array<lv_obj_t*, logger::kSummaryLapPageCapacity> lap_durations_{};
    std::array<lv_obj_t*, logger::kSummaryLapPageCapacity> lap_emphasis_{};
    std::array<lv_obj_t*, 6> buttons_{};
    std::array<Binding, 6> bindings_{};
};

}  // namespace track_timer::ui

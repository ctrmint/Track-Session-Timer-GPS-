#pragma once

#include "device_screen.hpp"

#include "track_timer/ui/navigation.hpp"
#include "track_timer/ui/presenter.hpp"
#include "track_timer/ui/ready_screen.hpp"

#include <lvgl.h>

namespace track_timer::simulator {

class ApplicationScreen {
  public:
    explicit ApplicationScreen(lv_obj_t* root) noexcept;

    void update(const ui::ReadyViewModel& ready, const ui::DeviceViewModel& active) noexcept;
    [[nodiscard]] ui::NavigationResult navigate(ui::NavigationAction action) noexcept;
    void synchronize_session(bool active) noexcept;
    [[nodiscard]] bool consume_start_request() noexcept;
    void add_controls_to_group(lv_group_t* group) noexcept;

    [[nodiscard]] ui::Destination destination() const noexcept;
    [[nodiscard]] ui::ReadyScreen& ready_screen() noexcept;
    [[nodiscard]] lv_obj_t* back_button_object() const noexcept;

    ApplicationScreen(const ApplicationScreen&) = delete;
    ApplicationScreen& operator=(const ApplicationScreen&) = delete;

  private:
    static void ready_navigation(ui::NavigationAction action, void* context) noexcept;
    static void back_event(lv_event_t* event) noexcept;
    void show_destination() noexcept;

    ui::NavigationController navigation_{};
    bool start_requested_{false};
    lv_obj_t* ready_root_{nullptr};
    lv_obj_t* active_root_{nullptr};
    lv_obj_t* destination_root_{nullptr};
    lv_obj_t* destination_title_{nullptr};
    lv_obj_t* destination_message_{nullptr};
    lv_obj_t* back_button_{nullptr};
    ui::ReadyScreen ready_screen_;
    DeviceScreen active_screen_;
};

}  // namespace track_timer::simulator

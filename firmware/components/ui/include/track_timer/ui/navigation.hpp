#pragma once

#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

enum class Destination : std::uint8_t {
    ready,
    active,
    setup,
    review,
    diagnostics,
    rest,
};

enum class NavigationAction : std::uint8_t {
    start_session,
    open_setup,
    open_review,
    open_diagnostics,
    back,
    session_ended,
    rest_started,
};

struct NavigationResult {
    Destination previous{Destination::ready};
    Destination current{Destination::ready};
    bool accepted{false};
    bool start_requested{false};
};

class NavigationController {
  public:
    [[nodiscard]] NavigationResult dispatch(NavigationAction action) noexcept;
    void synchronize_session(bool active) noexcept;

    [[nodiscard]] Destination destination() const noexcept;
    [[nodiscard]] bool session_active() const noexcept;
    [[nodiscard]] bool configuration_allowed() const noexcept;

  private:
    Destination destination_{Destination::ready};
    bool session_active_{false};
};

[[nodiscard]] const char* destination_name(Destination destination) noexcept;

static_assert(std::is_trivially_copyable_v<NavigationResult>);

}  // namespace track_timer::ui

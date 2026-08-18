#pragma once

#include "track_timer/ui/input.hpp"
#include "track_timer/ui/navigation.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// Gesture-driven shell over the existing NavigationController.
//
// This owns only what the older model cannot express: whether the gated menu is open,
// and where the carousel sits. When the driver commits a choice it emits an ordinary
// NavigationAction, so the tested destination logic in NavigationController is reused
// rather than duplicated.

enum class ShellLevel : std::uint8_t {
    ready,    // idle dashboard; START is directly available here
    menu,     // SETUP / REVIEW / DIAGNOSTICS carousel
    section,  // carousel inside SETUP
};

enum class MenuItem : std::uint8_t {
    setup,
    review,
    diagnostics,
};
inline constexpr std::size_t kMenuItemCount = 3;

enum class SetupItem : std::uint8_t {
    device_settings,
    track_selection,
    g_meter,
};
inline constexpr std::size_t kSetupItemCount = 3;

enum class ShellOutcome : std::uint8_t {
    ignored,
    menu_opened,
    moved,
    entered,
    exited,
    refused_session_active,
};

struct ShellState {
    ShellLevel level{ShellLevel::ready};
    MenuItem menu_item{MenuItem::setup};
    SetupItem setup_item{SetupItem::device_settings};
};

struct ShellResult {
    ShellState state{};
    ShellOutcome outcome{ShellOutcome::ignored};
    NavigationAction action{NavigationAction::back};
    bool emits_action{false};
};

class ShellNavigation {
  public:
    [[nodiscard]] ShellResult dispatch(InputAction action) noexcept;

    // A live session owns the screen. The menu cannot be opened while one is running,
    // matching the existing rule that Setup is unavailable during a session.
    void synchronize_session(bool active) noexcept;

    void close() noexcept;

    [[nodiscard]] const ShellState& state() const noexcept;
    [[nodiscard]] bool session_active() const noexcept;
    [[nodiscard]] bool menu_open() const noexcept;

  private:
    ShellState state_{};
    bool session_active_{false};
};

[[nodiscard]] const char* shell_level_name(ShellLevel level) noexcept;
[[nodiscard]] const char* shell_outcome_name(ShellOutcome outcome) noexcept;
[[nodiscard]] const char* menu_item_name(MenuItem item) noexcept;
[[nodiscard]] const char* setup_item_name(SetupItem item) noexcept;

static_assert(std::is_trivially_copyable_v<ShellState>);
static_assert(std::is_trivially_copyable_v<ShellResult>);

}  // namespace track_timer::ui

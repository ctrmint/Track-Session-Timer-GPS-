#pragma once

#include "track_timer/ui/input.hpp"
#include "track_timer/ui/navigation.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

// Gesture-driven shell over the existing NavigationController.
//
// This owns only what the older model cannot express: whether the gated menu is open and
// where each carousel sits. Committing a choice that changes screen emits an ordinary
// NavigationAction, so the tested destination logic is reused rather than duplicated.

enum class ShellLevel : std::uint8_t {
    ready,    // idle dashboard; START stays directly available here
    menu,     // MODE / SETUP / REVIEW / DIAGNOSTICS
    section,  // the chosen menu item's children
    field,    // a device setting
    value,    // that setting's values, each one press away
};

enum class MenuItem : std::uint8_t {
    mode,
    setup,
    review,
    diagnostics,
};
inline constexpr std::size_t kMenuItemCount = 4;

// Both MODE and SETUP happen to offer three children.
inline constexpr std::size_t kSectionItemCount = 3;

enum class SetupItem : std::uint8_t {
    device_settings,
    track_selection,
    g_meter,
};

enum class ShellOutcome : std::uint8_t {
    ignored,
    menu_opened,
    moved,
    entered,
    exited,
    refused_session_active,
    mode_selected,   // a Mode was chosen; the caller persists it
    value_selected,  // a setting value was chosen; the caller applies it
};

struct ShellState {
    ShellLevel level{ShellLevel::ready};
    std::size_t menu_index{0};
    std::size_t section_index{0};
    std::size_t field_index{0};
    std::size_t value_index{0};
};

struct ShellResult {
    ShellState state{};
    ShellOutcome outcome{ShellOutcome::ignored};
    NavigationAction action{NavigationAction::back};
    bool emits_action{false};
};

class ShellNavigation {
  public:
    explicit ShellNavigation(std::size_t field_count) noexcept;

    [[nodiscard]] ShellResult dispatch(InputAction action) noexcept;

    // The number of values the current field offers. Supplied by the caller because it
    // depends on the field, and the navigation model deliberately knows nothing about
    // settings.
    void set_value_count(std::size_t count) noexcept;

    // Places the cursor on the value the setting already holds, so opening a field shows
    // the current choice rather than always starting at the first one.
    void select_value(std::size_t index) noexcept;

    // A live session owns the screen, matching the existing rule that Setup is
    // unavailable while one is running.
    void synchronize_session(bool active) noexcept;

    void close() noexcept;

    [[nodiscard]] const ShellState& state() const noexcept;
    [[nodiscard]] MenuItem menu_item() const noexcept;
    [[nodiscard]] bool menu_open() const noexcept;
    [[nodiscard]] bool session_active() const noexcept;

  private:
    [[nodiscard]] std::size_t count_for(ShellLevel level) const noexcept;
    void move(bool forward, ShellResult& result) noexcept;
    void enter(ShellResult& result) noexcept;
    void back(ShellResult& result) noexcept;

    ShellState state_{};
    std::size_t field_count_{1};
    std::size_t value_count_{1};
    bool session_active_{false};
};

[[nodiscard]] const char* shell_level_name(ShellLevel level) noexcept;
[[nodiscard]] const char* shell_outcome_name(ShellOutcome outcome) noexcept;
[[nodiscard]] const char* menu_item_name(MenuItem item) noexcept;
[[nodiscard]] const char* setup_item_name(SetupItem item) noexcept;

static_assert(std::is_trivially_copyable_v<ShellState>);
static_assert(std::is_trivially_copyable_v<ShellResult>);

}  // namespace track_timer::ui

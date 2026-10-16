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

// Track selection sits at the top level, between Mode and Setup: at a circuit it is the
// thing most often changed, and burying it under Setup made it the deepest common task.
// Order is position, and position is a statement about when each item is wanted: opening
// the menu lands on the first one with no swipe at all, and everything else costs at least
// one.
//
// Review comes first because it is wanted at the one moment the driver is definitely
// stopped and definitely reaching for the device - just after a session. Mode, Track and
// Trigger follow, in the order they are decided at the circuit before going out.
// Diagnostics is last because it is the least often wanted.
enum class MenuItem : std::uint8_t {
    review,
    mode,
    track,
    trigger,
    setup,
    diagnostics,
};
inline constexpr std::size_t kMenuItemCount = 6;

// The default only, and no longer the same for every menu: Mode offers four children,
// Trigger and Setup three, and the track list is however many are on the card. Every
// caller sets the count from the array it is about to show, so the two cannot disagree.
inline constexpr std::size_t kSectionItemCount = 3;

// The children of Setup, in the order the carousel offers them. Only the ones the shell
// routes are listed: track selection moved to the top level, and the G-meter and display
// entries are drawn but not yet wired, which is why neither appears here.
enum class SetupItem : std::uint8_t {
    device_settings,
    gate_capture,
    // The 0-X target. It is one value rather than a list of fields, so entering it skips
    // the field level and opens that field's choices directly - Setup should not make
    // somebody walk a list of twelve settings to reach the one the entry is named after.
    acceleration_target,
};

enum class ShellOutcome : std::uint8_t {
    ignored,
    menu_opened,
    moved,
    entered,
    exited,
    refused_session_active,
    mode_selected,   // a Mode was chosen; the caller persists it
    track_selected,  // a track was chosen; the caller loads and applies it
    value_selected,    // a setting value was chosen; the caller applies it
    trigger_selected,  // a session trigger was chosen; the caller persists it
    // Gate capture was chosen under Setup; the caller opens the capture screen. Like the
    // four above, choosing it is the whole interaction as far as the shell is concerned -
    // what follows is driven by the capture screen's own buttons, not by the carousel.
    gate_capture_selected,
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

    // The track list length comes from the card, so the shell is told rather than
    // assuming. Applies to whichever section carousel is currently open.
    void set_section_count(std::size_t count) noexcept;

    // Places the cursor on the value the setting already holds, so opening a field shows
    // the current choice rather than always starting at the first one.
    // Opens a carousel on the item currently in force rather than the first one. Without
    // it a menu cannot tell the driver what is set, only let them change it.
    void select_section(std::size_t index) noexcept;
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
    std::size_t section_count_{kSectionItemCount};
    bool session_active_{false};
};

[[nodiscard]] const char* shell_level_name(ShellLevel level) noexcept;
[[nodiscard]] const char* shell_outcome_name(ShellOutcome outcome) noexcept;
[[nodiscard]] const char* menu_item_name(MenuItem item) noexcept;
[[nodiscard]] const char* setup_item_name(SetupItem item) noexcept;

static_assert(std::is_trivially_copyable_v<ShellState>);
static_assert(std::is_trivially_copyable_v<ShellResult>);

}  // namespace track_timer::ui

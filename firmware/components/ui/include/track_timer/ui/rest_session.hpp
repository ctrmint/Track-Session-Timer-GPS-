#pragma once

#include "track_timer/session/controller.hpp"
#include "track_timer/ui/active_session.hpp"

#include <array>
#include <cstdint>
#include <type_traits>

namespace track_timer::ui {

struct RestSessionViewModel {
    std::array<char, 24> remaining{};
    std::array<char, 32> completion{};
    std::array<char, 48> next_step{};
    StopControlViewModel skip{};
    bool visible{false};
};

class RestSessionController {
  public:
    void update(const session::SessionSnapshot& snapshot, std::uint64_t now_ms) noexcept;
    void press_skip(std::uint64_t now_ms) noexcept;
    void release_skip(std::uint64_t now_ms) noexcept;
    void cancel_skip_hold(std::uint64_t now_ms) noexcept;
    void cancel_skip(std::uint64_t now_ms) noexcept;
    void confirm_skip(std::uint64_t now_ms) noexcept;

    [[nodiscard]] bool consume_skip_request() noexcept;
    [[nodiscard]] const RestSessionViewModel& view_model() const noexcept;

  private:
    void reset_interaction() noexcept;
    void update_timeouts(std::uint64_t now_ms) noexcept;
    void refresh_skip_view() noexcept;

    RestSessionViewModel view_{};
    std::uint64_t last_update_ms_{0};
    std::uint64_t hold_started_ms_{0};
    std::uint64_t confirmation_expires_ms_{0};
    bool skip_request_pending_{false};
};

[[nodiscard]] const char* session_state_name(session::SessionState state) noexcept;
[[nodiscard]] const char* completion_reason_name(session::CompletionReason reason) noexcept;
[[nodiscard]] domain::UiSnapshot apply_session_timing(
    const domain::UiSnapshot& input,
    const session::SessionSnapshot& session_snapshot) noexcept;

static_assert(std::is_trivially_copyable_v<RestSessionViewModel>);

}  // namespace track_timer::ui

#pragma once

#include "track_timer/domain/contracts.hpp"
#include "track_timer/track/storage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

enum class GateCaptureStatus : std::uint8_t {
    closed,
    editing,
    endpoint_captured,
    active_session,
    moving,
    stale_fix,
    unusable_fix,
    poor_accuracy,
    incomplete,
    invalid_geometry,
    overwrite_confirmation,
    saved,
    storage_error,
};

struct GateCaptureViewModel {
    std::array<char, 64> track{};
    std::array<char, 48> selection{};
    std::array<char, 96> fix{};
    std::array<char, 96> preview{};
    std::array<char, 112> status{};
    std::array<char, 24> endpoint_label{};
    std::array<char, 24> save_label{};
    std::uint32_t status_color_rgb{0xFFFFFF};
    bool can_capture{false};
    bool can_save{false};
    bool confirming_overwrite{false};
};

class GateCaptureController {
  public:
    void begin(const track::TrackDefinition& definition,
               track::TrackDefinitionStore* store, bool session_active) noexcept;
    void update_fix(const domain::GnssFix& fix,
                    std::int64_t evaluation_monotonic_us) noexcept;
    void previous_gate() noexcept;
    void next_gate() noexcept;
    void toggle_endpoint() noexcept;
    void capture(bool session_active) noexcept;
    void save(bool confirm_overwrite) noexcept;
    void cancel() noexcept;

    [[nodiscard]] GateCaptureStatus status() const noexcept;
    [[nodiscard]] std::size_t gate_index() const noexcept;
    [[nodiscard]] bool right_endpoint_selected() const noexcept;
    [[nodiscard]] std::size_t captured_endpoint_count() const noexcept;
    [[nodiscard]] const track::TrackDefinition& draft() const noexcept;
    [[nodiscard]] GateCaptureViewModel view_model() const noexcept;

  private:
    [[nodiscard]] track::DirectedGateDefinition* selected_gate() noexcept;
    [[nodiscard]] const track::DirectedGateDefinition* selected_gate() const noexcept;
    [[nodiscard]] bool complete() const noexcept;

    track::TrackDefinition draft_{};
    track::TrackDefinitionStore* store_{nullptr};
    domain::GnssFix latest_fix_{};
    std::int64_t evaluation_monotonic_us_{domain::kUnavailableTime};
    std::array<bool, 8> captured_{};
    std::size_t gate_index_{0};
    bool right_endpoint_{false};
    GateCaptureStatus status_{GateCaptureStatus::closed};
};

[[nodiscard]] const char* gate_capture_status_name(GateCaptureStatus status) noexcept;

}  // namespace track_timer::ui

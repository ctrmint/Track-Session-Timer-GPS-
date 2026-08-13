#pragma once

#include "track_timer/diagnostics/snapshot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::ui {

inline constexpr std::size_t kDiagnosticsPageCount = 4;
inline constexpr std::size_t kDiagnosticsRowsPerPage = 8;

enum class DiagnosticsPage : std::uint8_t {
    system,
    gnss,
    logging,
    peripherals,
};

struct DiagnosticsRow {
    std::array<char, 24> label{};
    std::array<char, 48> value{};
    std::uint32_t color_rgb{0};
};

struct DiagnosticsViewModel {
    std::array<char, 32> title{};
    std::array<char, 48> status{};
    std::uint32_t status_color_rgb{0};
    std::array<char, 24> page{};
    std::array<DiagnosticsRow, kDiagnosticsRowsPerPage> rows{};
    DiagnosticsPage current_page{DiagnosticsPage::system};
    bool previous_enabled{false};
    bool next_enabled{true};
};

class DiagnosticsController {
  public:
    void begin(const diagnostics::DiagnosticsSnapshot& snapshot) noexcept;
    void update(const diagnostics::DiagnosticsSnapshot& snapshot) noexcept;
    void close() noexcept;
    void previous_page() noexcept;
    void next_page() noexcept;
    [[nodiscard]] const DiagnosticsViewModel& view_model() const noexcept;

  private:
    void refresh() noexcept;
    void build_system_page() noexcept;
    void build_gnss_page() noexcept;
    void build_logging_page() noexcept;
    void build_peripherals_page() noexcept;

    diagnostics::DiagnosticsSnapshot snapshot_{};
    DiagnosticsViewModel view_{};
    bool open_{false};
};

[[nodiscard]] const char* diagnostics_page_name(DiagnosticsPage page) noexcept;
[[nodiscard]] const char* diagnostics_overall_name(
    diagnostics::OverallState state) noexcept;

}  // namespace track_timer::ui

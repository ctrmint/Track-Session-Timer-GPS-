#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace track_timer::simulator {

inline constexpr std::size_t kMaximumFixedTextCells = 10;

struct FixedCellText {
    std::array<char, kMaximumFixedTextCells> cells{};
    std::size_t cell_count{0};
    bool overflowed{false};
};

[[nodiscard]] FixedCellText layout_fixed_cell_text(std::string_view text,
                                                   std::size_t cell_count) noexcept;

}  // namespace track_timer::simulator

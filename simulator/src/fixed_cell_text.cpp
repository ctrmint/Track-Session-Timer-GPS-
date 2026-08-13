#include "track_timer/simulator/fixed_cell_text.hpp"

#include <algorithm>

namespace track_timer::simulator {

FixedCellText layout_fixed_cell_text(const std::string_view text,
                                     const std::size_t cell_count) noexcept
{
    FixedCellText result{};
    result.cell_count = std::min(cell_count, kMaximumFixedTextCells);
    if (text.size() > result.cell_count) {
        std::fill_n(result.cells.begin(), result.cell_count, '#');
        result.overflowed = true;
        return result;
    }

    const auto first_cell = result.cell_count - text.size();
    std::copy(text.begin(), text.end(), result.cells.begin() + first_cell);
    return result;
}

}  // namespace track_timer::simulator

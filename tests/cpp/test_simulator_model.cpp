#include "track_timer/board/platform.hpp"
#include "track_timer/simulator/scenario.hpp"
#include "track_timer/ui/presenter.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

class DeterministicClock final : public track_timer::board::MonotonicClock {
  public:
    [[nodiscard]] std::int64_t now_us() const noexcept override
    {
        return now_us_;
    }

    void advance(const std::int64_t elapsed_us) noexcept
    {
        now_us_ += elapsed_us;
    }

  private:
    std::int64_t now_us_{0};
};

bool snapshots_equal(const track_timer::domain::UiSnapshot& left,
                     const track_timer::domain::UiSnapshot& right) noexcept
{
    return left.session_remaining_ms == right.session_remaining_ms &&
           left.current_lap_ms == right.current_lap_ms &&
           left.previous_lap_ms == right.previous_lap_ms && left.best_lap_ms == right.best_lap_ms &&
           left.lap_index == right.lap_index && left.gnss_health == right.gnss_health &&
           left.session_active == right.session_active &&
           left.logging_available == right.logging_available;
}

}  // namespace

int main()
{
    using track_timer::simulator::ScenarioId;
    using track_timer::simulator::ScenarioPlayer;

    DeterministicClock clock;
    clock.advance(40'000);
    assert(clock.now_us() == 40'000);

    ScenarioId parsed{};
    assert(track_timer::simulator::parse_scenario("active", parsed));
    assert(parsed == ScenarioId::active);
    assert(!track_timer::simulator::parse_scenario("unknown", parsed));
    assert(track_timer::simulator::next_scenario(ScenarioId::storage_failure) == ScenarioId::ready);

    ScenarioPlayer first{ScenarioId::active};
    ScenarioPlayer second{ScenarioId::active};
    for (int frame = 0; frame < 25; ++frame) {
        first.advance(40);
        second.advance(40);
    }
    assert(first.elapsed_ms() == 1'000);
    assert(snapshots_equal(first.snapshot(), second.snapshot()));

    const auto active = track_timer::ui::present(first.snapshot());
    assert(std::strcmp(active.lap_label.data(), "LAP 07") == 0);
    assert(std::strcmp(active.current_lap.data(), "1:43.638") == 0);
    assert(std::strcmp(active.previous_lap.data(), "1:43.112") == 0);
    assert(std::strcmp(active.best_lap.data(), "1:42.985") == 0);
    assert(std::strcmp(active.session_remaining.data(), "15:26") == 0);
    assert(std::strcmp(active.gnss_status.data(), "GPS GOOD") == 0);
    assert(std::strcmp(active.logging_status.data(), "LOGGING") == 0);

    const auto gnss_loss = track_timer::ui::present(ScenarioPlayer{ScenarioId::gnss_loss}.snapshot());
    assert(std::strcmp(gnss_loss.gnss_status.data(), "GPS STALE") == 0);

    const auto storage_failure =
        track_timer::ui::present(ScenarioPlayer{ScenarioId::storage_failure}.snapshot());
    assert(std::strcmp(storage_failure.logging_status.data(), "NO LOG") == 0);

    const auto ready = track_timer::ui::present(ScenarioPlayer{ScenarioId::ready}.snapshot());
    assert(std::strcmp(ready.current_lap.data(), "--:--.---") == 0);
    assert(std::strcmp(ready.gnss_status.data(), "GPS SEARCH") == 0);

    auto extreme_snapshot = ScenarioPlayer{ScenarioId::active}.snapshot();
    extreme_snapshot.current_lap_ms = std::numeric_limits<std::int64_t>::min();
    extreme_snapshot.session_remaining_ms = std::numeric_limits<std::int64_t>::min();
    const auto extreme = track_timer::ui::present(extreme_snapshot);
    assert(std::strcmp(extreme.current_lap.data(), "+153722867280912:55.808") == 0);
    assert(std::strcmp(extreme.session_remaining.data(), "+153722867280912:55") == 0);

    std::cout << "Simulator model contracts passed\n";
    return 0;
}

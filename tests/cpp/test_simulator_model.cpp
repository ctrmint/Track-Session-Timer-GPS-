#include "track_timer/board/platform.hpp"
#include "track_timer/simulator/fixed_cell_text.hpp"
#include "track_timer/simulator/scenario.hpp"
#include "track_timer/ui/presenter.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
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

std::uint16_t occupied_cells(const track_timer::simulator::FixedCellText& layout) noexcept
{
    std::uint16_t mask = 0;
    for (std::size_t index = 0; index < layout.cell_count; ++index) {
        if (layout.cells[index] != '\0') {
            mask = static_cast<std::uint16_t>(mask | (1U << index));
        }
    }
    return mask;
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

    const auto narrow_lap = track_timer::simulator::layout_fixed_cell_text("1:11.111", 10);
    const auto wide_lap = track_timer::simulator::layout_fixed_cell_text("8:48.888", 10);
    const auto two_digit_lap = track_timer::simulator::layout_fixed_cell_text("10:41.141", 10);
    const auto unavailable_lap = track_timer::simulator::layout_fixed_cell_text("--:--.---", 10);
    assert(!narrow_lap.overflowed && !wide_lap.overflowed && !two_digit_lap.overflowed);
    assert(occupied_cells(narrow_lap) == occupied_cells(wide_lap));
    assert(narrow_lap.cells[3] == ':' && wide_lap.cells[3] == ':' &&
           two_digit_lap.cells[3] == ':' && unavailable_lap.cells[3] == ':');
    assert(narrow_lap.cells[6] == '.' && wide_lap.cells[6] == '.' &&
           two_digit_lap.cells[6] == '.' && unavailable_lap.cells[6] == '.');

    const auto narrow_session = track_timer::simulator::layout_fixed_cell_text("11:11", 7);
    const auto wide_session = track_timer::simulator::layout_fixed_cell_text("88:48", 7);
    const auto long_session = track_timer::simulator::layout_fixed_cell_text("100:00", 7);
    assert(occupied_cells(narrow_session) == occupied_cells(wide_session));
    assert(narrow_session.cells[4] == ':' && wide_session.cells[4] == ':' &&
           long_session.cells[4] == ':');

    const auto overflow = track_timer::simulator::layout_fixed_cell_text("1234:56.789", 10);
    assert(overflow.overflowed);
    for (std::size_t index = 0; index < overflow.cell_count; ++index) {
        assert(overflow.cells[index] == '#');
    }

    const auto repo_root = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path();
    track_timer::simulator::GnssFixture recorded_fixture{};
    std::string fixture_error;
    assert(track_timer::simulator::load_gnss_fixture(
        (repo_root / "simulator/fixtures/recorded_reference_v1.csv").string(),
        recorded_fixture, fixture_error));
    assert(fixture_error.empty());
    assert(recorded_fixture.format_version == 1);
    assert(recorded_fixture.name == "recorded-reference-v1");
    assert(recorded_fixture.fixes.size() == 12);

    track_timer::simulator::SimulatedClock replay_clock;
    track_timer::simulator::SimulatedGnss replay_20hz{
        replay_clock, recorded_fixture, track_timer::simulator::GnssReplayRate::hz20};
    replay_clock.advance_us(1'000'000);
    replay_20hz.advance();
    const auto replay_diagnostics = replay_20hz.diagnostics();
    assert(replay_diagnostics.scheduled == 20);
    assert(replay_diagnostics.emitted == 20);
    assert(replay_diagnostics.queue.depth == 20);
    track_timer::domain::GnssFix replay_fix{};
    assert(replay_20hz.try_read(replay_fix));
    assert(replay_fix.arrival_monotonic_us == 50'000);
    assert(replay_fix.latitude_deg == recorded_fixture.fixes.front().latitude_deg);

    track_timer::simulator::SimulatedClock overflow_clock;
    track_timer::simulator::SimulatedGnss overflow_gnss{
        overflow_clock, track_timer::simulator::make_synthetic_gnss_fixture(),
        track_timer::simulator::GnssReplayRate::hz25};
    overflow_clock.advance_us(3'000'000);
    overflow_gnss.advance();
    const auto overflow_diagnostics = overflow_gnss.diagnostics();
    assert(overflow_diagnostics.scheduled == 75);
    assert(overflow_diagnostics.emitted == track_timer::domain::queue_capacity::gnss_fixes);
    assert(overflow_diagnostics.queue.high_water_mark ==
           track_timer::domain::queue_capacity::gnss_fixes);
    assert(overflow_diagnostics.queue.dropped == 11);

    track_timer::simulator::SimulatedClock fault_clock;
    track_timer::simulator::SimulatedGnss fault_gnss{
        fault_clock, track_timer::simulator::make_synthetic_gnss_fixture(),
        track_timer::simulator::GnssReplayRate::hz25};
    fault_gnss.set_mode(track_timer::simulator::GnssMode::loss);
    fault_clock.advance_us(80'000);
    fault_gnss.advance();
    assert(fault_gnss.diagnostics().lost == 2);
    fault_gnss.set_mode(track_timer::simulator::GnssMode::stale);
    fault_clock.advance_us(80'000);
    fault_gnss.advance();
    assert(fault_gnss.try_read(replay_fix));
    assert(replay_fix.reject_reason == track_timer::domain::FixRejectReason::stale);
    const auto stale_measurement_time = replay_fix.measurement_time_ns;
    assert(fault_gnss.try_read(replay_fix));
    assert(replay_fix.measurement_time_ns == stale_measurement_time);
    fault_gnss.set_mode(track_timer::simulator::GnssMode::corrupt);
    fault_clock.advance_us(40'000);
    fault_gnss.advance();
    assert(fault_gnss.try_read(replay_fix));
    assert(replay_fix.reject_reason == track_timer::domain::FixRejectReason::invalid_status);
    fault_gnss.set_mode(track_timer::simulator::GnssMode::normal);
    assert(fault_gnss.diagnostics().recoveries == 1);

    track_timer::simulator::SimulatedClock peripheral_clock;
    track_timer::simulator::SimulatedTouch touch{peripheral_clock};
    peripheral_clock.advance_us(12'000);
    assert(touch.inject(300, 225, true));
    track_timer::board::TouchSample touch_sample{};
    assert(touch.try_read(touch_sample));
    assert(touch_sample.monotonic_us == 12'000);
    assert(touch_sample.x == 300 && touch_sample.y == 225 && touch_sample.pressed);

    track_timer::simulator::SimulatedImu imu{peripheral_clock};
    peripheral_clock.advance_us(20'000);
    imu.advance();
    track_timer::board::ImuSample imu_sample{};
    assert(imu.try_read(imu_sample));
    assert(imu_sample.valid);
    assert(imu_sample.acceleration_z_mps2 == 9.80665F);

    track_timer::simulator::SimulatedClock rtc_clock;
    track_timer::simulator::SimulatedRtc rtc{
        rtc_clock, track_timer::board::RtcDateTime{2024, 2, 28, 23, 59, 59, true}};
    rtc_clock.advance_us(2'000'000);
    const auto rtc_value = rtc.now();
    assert(rtc_value.year == 2024 && rtc_value.month == 2 && rtc_value.day == 29);
    assert(rtc_value.hour == 0 && rtc_value.minute == 0 && rtc_value.second == 1);

    track_timer::simulator::SimulatedStorage storage;
    track_timer::domain::LogRecord log_record{};
    log_record.payload_size = 32;
    assert(storage.append(log_record));
    storage.advance(1'000);
    assert(storage.diagnostics().written_records == 1);
    storage.reset();
    storage.set_mode(track_timer::simulator::StorageMode::slow);
    for (std::size_t index = 0; index <= track_timer::domain::queue_capacity::log_records;
         ++index) {
        storage.append(log_record);
    }
    assert(storage.status().health == track_timer::board::StorageHealth::degraded);
    assert(storage.diagnostics().queue.dropped == 1);
    storage.set_mode(track_timer::simulator::StorageMode::missing);
    assert(!storage.append(log_record));
    storage.set_mode(track_timer::simulator::StorageMode::full);
    assert(!storage.append(log_record));
    storage.set_mode(track_timer::simulator::StorageMode::write_failed);
    assert(!storage.append(log_record));
    storage.set_mode(track_timer::simulator::StorageMode::ready);
    assert(storage.diagnostics().recoveries == 1);

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
    assert(active.gnss_health == track_timer::domain::GnssHealth::good);
    assert(std::strcmp(active.logging_status.data(), "LOGGING") == 0);

    const auto gnss_loss = track_timer::ui::present(ScenarioPlayer{ScenarioId::gnss_loss}.snapshot());
    assert(std::strcmp(gnss_loss.gnss_status.data(), "GPS STALE") == 0);
    assert(gnss_loss.gnss_health == track_timer::domain::GnssHealth::stale);

    const auto storage_failure =
        track_timer::ui::present(ScenarioPlayer{ScenarioId::storage_failure}.snapshot());
    assert(std::strcmp(storage_failure.logging_status.data(), "NO LOG") == 0);

    ScenarioPlayer gnss_recovery{ScenarioId::gnss_loss};
    ScenarioPlayer storage_recovery{ScenarioId::storage_failure};
    for (int frame = 0; frame < 100; ++frame) {
        gnss_recovery.advance(40);
        storage_recovery.advance(40);
    }
    gnss_recovery.advance(40);
    assert(gnss_recovery.snapshot().session_remaining_ms == 4 * 60'000 + 46'960);
    assert(gnss_recovery.snapshot().gnss_health == track_timer::domain::GnssHealth::good);
    assert(gnss_recovery.diagnostics().gnss.lost > 0);
    assert(gnss_recovery.diagnostics().gnss.stale > 0);
    assert(gnss_recovery.diagnostics().gnss.corrupt > 0);
    assert(gnss_recovery.diagnostics().gnss.recoveries == 1);
    assert(storage_recovery.snapshot().session_remaining_ms == 8 * 60'000 + 8'000);
    assert(storage_recovery.snapshot().logging_available);
    assert(storage_recovery.diagnostics().storage.recoveries == 1);

    ScenarioPlayer coarse_gnss_recovery{ScenarioId::gnss_loss};
    coarse_gnss_recovery.advance(4'040);
    assert(snapshots_equal(coarse_gnss_recovery.snapshot(), gnss_recovery.snapshot()));
    assert(coarse_gnss_recovery.diagnostics().gnss.lost ==
           gnss_recovery.diagnostics().gnss.lost);
    assert(coarse_gnss_recovery.diagnostics().gnss.stale ==
           gnss_recovery.diagnostics().gnss.stale);
    assert(coarse_gnss_recovery.diagnostics().gnss.corrupt ==
           gnss_recovery.diagnostics().gnss.corrupt);

    const auto ready = track_timer::ui::present(ScenarioPlayer{ScenarioId::ready}.snapshot());
    assert(std::strcmp(ready.current_lap.data(), "--:--.---") == 0);
    assert(std::strcmp(ready.gnss_status.data(), "GPS SEARCH") == 0);
    assert(ready.gnss_health == track_timer::domain::GnssHealth::searching);

    auto extreme_snapshot = ScenarioPlayer{ScenarioId::active}.snapshot();
    extreme_snapshot.current_lap_ms = std::numeric_limits<std::int64_t>::min();
    extreme_snapshot.session_remaining_ms = std::numeric_limits<std::int64_t>::min();
    const auto extreme = track_timer::ui::present(extreme_snapshot);
    assert(std::strcmp(extreme.current_lap.data(), "+153722867280912:55.808") == 0);
    assert(std::strcmp(extreme.session_remaining.data(), "+153722867280912:55") == 0);

    std::cout << "Simulator model contracts passed\n";
    return 0;
}

#include "track_timer/simulator/diagnostics_fixtures.hpp"
#include "track_timer/ui/diagnostics.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

int main()
{
    using namespace track_timer;

    simulator::DeviceDiagnostics device{};
    device.gnss.queue.capacity = domain::queue_capacity::gnss_fixes;
    device.imu_queue.capacity = 64;
    device.touch_queue.capacity = 16;
    board::StorageStatus storage{64U * 1'024U * 1'024U, 0,
                                 board::StorageHealth::ready};
    logger::LoggerMetrics logger{};
    ui::RenderMetrics display{12, 24'000, 2'500, 74'000};

    const auto normal = simulator::make_diagnostics_snapshot(
        simulator::DiagnosticsFixtureId::normal, 3'723'000, device, storage, logger,
        display);
    ui::DiagnosticsController diagnostics{};
    diagnostics.begin(normal);
    auto view = diagnostics.view_model();
    assert(view.current_page == ui::DiagnosticsPage::system);
    assert(!view.previous_enabled);
    assert(view.next_enabled);
    assert(std::strcmp(view.rows[0].label.data(), "FIRMWARE") == 0);
    assert(std::strcmp(view.rows[1].value.data(), "01:02:03") == 0);
    assert(std::strcmp(view.rows[4].value.data(), "NOT SIMULATED") == 0);
    assert(std::strcmp(view.rows[5].value.data(), "SIMULATOR") == 0);

    diagnostics.next_page();
    view = diagnostics.view_model();
    assert(view.current_page == ui::DiagnosticsPage::gnss);
    assert(std::strcmp(view.rows[1].value.data(), "25 Hz") == 0);
    assert(std::strcmp(view.rows[2].value.data(), "3D FIX") == 0);
    assert(std::strcmp(view.rows[3].value.data(), "14") == 0);

    diagnostics.next_page();
    view = diagnostics.view_model();
    assert(view.current_page == ui::DiagnosticsPage::logging);
    assert(std::strcmp(view.rows[0].value.data(), "READY") == 0);
    assert(std::strcmp(view.rows[4].value.data(), "0") == 0);

    diagnostics.next_page();
    view = diagnostics.view_model();
    assert(view.current_page == ui::DiagnosticsPage::peripherals);
    assert(view.previous_enabled);
    assert(!view.next_enabled);
    diagnostics.next_page();
    assert(diagnostics.view_model().current_page == ui::DiagnosticsPage::peripherals);

    const auto degraded = simulator::make_diagnostics_snapshot(
        simulator::DiagnosticsFixtureId::degraded, 4'000, device, storage, logger,
        display);
    diagnostics.update(degraded);
    view = diagnostics.view_model();
    assert(view.current_page == ui::DiagnosticsPage::peripherals);
    assert(std::strstr(view.status.data(), "DEGRADED") != nullptr);
    assert(std::strcmp(view.rows[0].value.data(), "DEGRADED") == 0);

    const auto missing = simulator::make_diagnostics_snapshot(
        simulator::DiagnosticsFixtureId::missing, 5'000, device, storage, logger,
        display);
    diagnostics.begin(missing);
    diagnostics.next_page();
    view = diagnostics.view_model();
    assert(std::strcmp(view.rows[0].value.data(), "UNAVAILABLE") == 0);
    assert(std::strcmp(view.rows[1].value.data(), "UNAVAILABLE") == 0);
    assert(std::strcmp(view.rows[4].value.data(), "UNAVAILABLE") == 0);

    const auto recovered = simulator::make_diagnostics_snapshot(
        simulator::DiagnosticsFixtureId::recovery, 6'000, device, storage, logger,
        display);
    diagnostics.begin(recovered);
    diagnostics.next_page();
    view = diagnostics.view_model();
    assert(std::strstr(view.status.data(), "RECOVERED") != nullptr);
    assert(std::strcmp(view.rows[7].value.data(), "1 / 2") == 0);
    diagnostics.next_page();
    view = diagnostics.view_model();
    assert(std::strcmp(view.rows[5].value.data(), "3") == 0);
    assert(std::strcmp(view.rows[6].value.data(), "2") == 0);

    diagnostics.previous_page();
    assert(diagnostics.view_model().current_page == ui::DiagnosticsPage::gnss);
    diagnostics.close();

    std::cout << "Immutable diagnostics pages and normal/degraded/missing/recovery states passed\n";
    return 0;
}

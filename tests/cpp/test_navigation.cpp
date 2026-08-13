#include "track_timer/ui/navigation.hpp"
#include "track_timer/ui/presenter.hpp"

#include <cassert>
#include <cstring>
#include <iostream>

int main()
{
    using namespace track_timer::ui;

    NavigationController navigation;
    assert(navigation.destination() == Destination::ready);
    assert(navigation.configuration_allowed());

    auto result = navigation.dispatch(NavigationAction::open_setup);
    assert(result.accepted && result.current == Destination::setup);
    result = navigation.dispatch(NavigationAction::back);
    assert(result.accepted && result.current == Destination::ready);

    result = navigation.dispatch(NavigationAction::open_review);
    assert(result.accepted && result.current == Destination::review);
    assert(navigation.dispatch(NavigationAction::back).current == Destination::ready);

    result = navigation.dispatch(NavigationAction::open_diagnostics);
    assert(result.accepted && result.current == Destination::diagnostics);
    assert(navigation.dispatch(NavigationAction::back).current == Destination::ready);

    result = navigation.dispatch(NavigationAction::start_session);
    assert(result.accepted && result.start_requested);
    assert(result.current == Destination::active);
    assert(navigation.session_active());
    assert(!navigation.configuration_allowed());
    result = navigation.dispatch(NavigationAction::open_setup);
    assert(!result.accepted && result.current == Destination::active);
    result = navigation.dispatch(NavigationAction::back);
    assert(!result.accepted && result.current == Destination::active);
    result = navigation.dispatch(NavigationAction::session_ended);
    assert(result.accepted && result.current == Destination::ready);

    navigation.synchronize_session(true);
    assert(navigation.destination() == Destination::active);
    navigation.synchronize_session(false);
    assert(navigation.destination() == Destination::ready);

    ReadySnapshot ready{};
    std::strcpy(ready.selected_track.data(), "Synthetic Test Loop");
    ready.session_duration_minutes = 30;
    ready.rest_duration_minutes = 15;
    ready.gnss_health = track_timer::domain::GnssHealth::searching;
    ready.storage = Readiness::degraded;
    ready.imu = Readiness::unavailable;
    ready.logging_available = false;
    const auto degraded = present_ready(ready);
    assert(std::strcmp(degraded.selected_track.data(), "Synthetic Test Loop") == 0);
    assert(std::strcmp(degraded.session_duration.data(), "30 MIN SESSION") == 0);
    assert(std::strcmp(degraded.rest_duration.data(), "15 MIN REST") == 0);
    assert(std::strcmp(degraded.timing_mode.data(), "TIMER ONLY - GPS UNAVAILABLE") == 0);
    assert(std::strcmp(degraded.storage.text.data(), "STORAGE DEGRADED") == 0);
    assert(std::strcmp(degraded.imu.text.data(), "NO IMU") == 0);
    assert(std::strcmp(degraded.logging.text.data(), "NO LOGGING") == 0);
    assert(degraded.start_enabled);

    ready.gnss_health = track_timer::domain::GnssHealth::good;
    struct TrackTimingExpectation {
        ReadyTrackState state;
        const char* text;
    };
    for (const auto expectation : {
             TrackTimingExpectation{ReadyTrackState::selected, "LAP TIMING READY"},
             TrackTimingExpectation{ReadyTrackState::suggested,
                                    "TIMER ONLY - CONFIRM TRACK"},
             TrackTimingExpectation{ReadyTrackState::ambiguous,
                                    "TIMER ONLY - SELECT TRACK"},
             TrackTimingExpectation{ReadyTrackState::missing,
                                    "TIMER ONLY - TRACK MISSING"},
             TrackTimingExpectation{ReadyTrackState::invalid,
                                    "TIMER ONLY - TRACK DATA"},
             TrackTimingExpectation{ReadyTrackState::none, "TIMER ONLY - NO TRACK"},
         }) {
        ready.track_state = expectation.state;
        assert(std::strcmp(present_ready(ready).timing_mode.data(), expectation.text) == 0);
    }

    ready.session_active = true;
    const auto active = present_ready(ready);
    assert(!active.setup_enabled);

    std::cout << "Ready navigation transitions and degraded timer path passed\n";
    return 0;
}

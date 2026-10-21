#include "track_timer/simulator/file_track_definition_store.hpp"
#include "track_timer/simulator/track_fixtures.hpp"
#include "track_timer/track/capture.hpp"
#include "track_timer/ui/gate_capture.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

class MemoryTrackStore final : public track_timer::track::TrackDefinitionStore {
  public:
    track_timer::track::TrackStoreReadResult read(
        std::string_view, track_timer::track::TrackDefinitionBlob& output) noexcept override
    {
        if (fail_read) {
            return track_timer::track::TrackStoreReadResult::io_error;
        }
        if (!present) {
            return track_timer::track::TrackStoreReadResult::not_found;
        }
        output = blob;
        return track_timer::track::TrackStoreReadResult::loaded;
    }

    bool exists(std::string_view) noexcept override
    {
        return present;
    }

    bool write_atomic(std::string_view,
                      const track_timer::track::TrackDefinitionBlob& value) noexcept override
    {
        ++write_count;
        if (fail_write) {
            return false;
        }
        blob = value;
        present = true;
        return true;
    }

    track_timer::track::TrackDefinitionBlob blob{};
    bool present{false};
    bool fail_write{false};
    bool fail_read{false};
    int write_count{0};
};

track_timer::domain::GnssFix fix_at(
    const track_timer::track::GeographicPoint point, const std::uint32_t sequence)
{
    track_timer::domain::GnssFix fix{};
    fix.measurement_time_ns = static_cast<std::int64_t>(sequence) * 1'000'000'000;
    fix.arrival_monotonic_us = static_cast<std::int64_t>(sequence) * 1'000'000;
    fix.latitude_deg = point.latitude_deg;
    fix.longitude_deg = point.longitude_deg;
    fix.speed_mps = 0.1F;
    fix.horizontal_accuracy_m = 0.8F;
    fix.sequence_number = sequence;
    fix.fix_type = track_timer::domain::FixType::fix_3d;
    fix.reject_reason = track_timer::domain::FixRejectReason::none;
    fix.accepted_for_timing = true;
    return fix;
}

void capture_all(track_timer::ui::GateCaptureController& controller,
                 const track_timer::track::TrackDefinition& source)
{
    const std::array<track_timer::track::DirectedGateDefinition,
                     track_timer::ui::kGateCaptureGateCount>
        gates{source.gates.start, source.gates.finish};
    std::uint32_t sequence = 10;
    for (std::size_t gate_index = 0; gate_index < gates.size(); ++gate_index) {
        auto left = fix_at(gates[gate_index].left, sequence++);
        controller.update_fix(left, left.arrival_monotonic_us + 10'000);
        controller.capture(false);
        controller.toggle_endpoint();
        auto right = fix_at(gates[gate_index].right, sequence++);
        controller.update_fix(right, right.arrival_monotonic_us + 10'000);
        controller.capture(false);
        controller.toggle_endpoint();
        if (gate_index + 1 < gates.size()) {
            controller.next_gate();
        }
    }
}

}  // namespace

int main()
{
    using namespace track_timer;
    auto fixture = simulator::make_track_fixture(simulator::TrackFixtureId::suggested);
    const auto source = fixture.definitions[0];
    MemoryTrackStore store{};
    ui::GateCaptureController controller{};

    controller.begin(source, &store, false);
    assert(controller.status() == ui::GateCaptureStatus::editing);
    controller.save(false);
    assert(controller.status() == ui::GateCaptureStatus::incomplete);
    assert(store.write_count == 0);

    auto good = fix_at(source.gates.start.left, 1);
    controller.update_fix(good, good.arrival_monotonic_us);
    controller.capture(true);
    assert(controller.status() == ui::GateCaptureStatus::active_session);
    auto moving = good;
    moving.speed_mps = 0.6F;
    controller.update_fix(moving, moving.arrival_monotonic_us);
    controller.capture(false);
    assert(controller.status() == ui::GateCaptureStatus::moving);
    controller.update_fix(good, good.arrival_monotonic_us +
                                    track::kMaximumCaptureFixAgeUs + 1);
    controller.capture(false);
    assert(controller.status() == ui::GateCaptureStatus::stale_fix);
    auto unusable = good;
    unusable.accepted_for_timing = false;
    controller.update_fix(unusable, unusable.arrival_monotonic_us);
    controller.capture(false);
    assert(controller.status() == ui::GateCaptureStatus::unusable_fix);
    auto poor = good;
    poor.horizontal_accuracy_m = 5.1F;
    controller.update_fix(poor, poor.arrival_monotonic_us);
    controller.capture(false);
    assert(controller.status() == ui::GateCaptureStatus::poor_accuracy);

    // What the screen tells the driver to do has to match what the controller will accept.
    // It did not: the status asked for eight endpoints across four gates while save
    // unlocked at four across two, because the pit gates went (#227) and this text did not
    // (#204). Asserted against the stated count rather than a number typed in here, so the
    // next change to the shape of a circuit cannot pass this test with stale instructions.
    controller.begin(source, &store, false);
    auto empty = controller.view_model();
    assert(!empty.can_save);
    controller.save(false);
    assert(controller.status() == ui::GateCaptureStatus::incomplete);
    empty = controller.view_model();
    assert(std::strstr(empty.status.data(), "START AND FINISH") != nullptr);
    assert(std::strstr(empty.status.data(), "FOUR GATES") == nullptr);
    assert(std::strstr(empty.selection.data(), "0/4") != nullptr);

    controller.begin(source, &store, false);
    auto first = fix_at(source.gates.start.left, 7);
    controller.update_fix(first, first.arrival_monotonic_us + 10'000);
    controller.capture(false);
    assert(controller.status() == ui::GateCaptureStatus::endpoint_captured);
    const auto after_one = controller.view_model();
    assert(std::strstr(after_one.status.data(), "1 OF 4 DONE") != nullptr);
    assert(std::strstr(after_one.status.data(), "8") == nullptr);

    controller.begin(source, &store, false);
    capture_all(controller, source);
    assert(controller.captured_endpoint_count() == ui::kGateCaptureEndpointCount);
    auto model = controller.view_model();
    assert(model.can_save);
    assert(std::strstr(model.preview.data(), "LINE READY") != nullptr);
    assert(std::strstr(model.selection.data(), "4/4") != nullptr);
    controller.save(false);
    assert(controller.status() == ui::GateCaptureStatus::saved);
    assert(controller.draft().revision == source.revision + 1);
    assert(controller.draft().provenance.geometry_status ==
           track::TrackGeometryStatus::device_captured);
    assert(track::track_timing_ready(controller.draft()));

    controller.begin(source, &store, false);
    assert(controller.draft().revision == source.revision + 1);
    capture_all(controller, source);
    controller.save(false);
    assert(controller.status() == ui::GateCaptureStatus::overwrite_confirmation);
    assert(store.write_count == 1);
    controller.save(true);
    assert(controller.status() == ui::GateCaptureStatus::saved);
    assert(store.write_count == 2);

    controller.begin(source, &store, false);
    capture_all(controller, source);
    store.fail_write = true;
    controller.save(false);
    assert(controller.status() == ui::GateCaptureStatus::overwrite_confirmation);
    controller.save(true);
    assert(controller.status() == ui::GateCaptureStatus::storage_error);
    store.fail_write = false;

    // Every endpoint taken at the same spot: both lines are a point, which validation
    // refuses as degenerate. The refusal has to reach the panel as the thing to go and do
    // about it - the three ways a captured line fails have three different remedies, and
    // "GATE GEOMETRY FAILED SAFETY VALIDATION" names none of them to somebody standing at
    // the circuit with the engine off (#204).
    controller.begin(source, &store, false);
    const auto writes_before_invalid = store.write_count;
    for (std::size_t endpoint = 0; endpoint < ui::kGateCaptureEndpointCount; ++endpoint) {
        controller.update_fix(good, good.arrival_monotonic_us);
        controller.capture(false);
        controller.toggle_endpoint();
        if (endpoint % 2 == 1 && endpoint + 1 < ui::kGateCaptureEndpointCount) {
            controller.next_gate();
        }
    }
    controller.save(true);
    assert(controller.status() == ui::GateCaptureStatus::invalid_geometry);
    assert(controller.refusal().result == track::TrackLoadResult::degenerate_start_gate);
    const auto refused = controller.view_model();
    assert(std::strstr(refused.status.data(), "LINE TOO SHORT") != nullptr);
    assert(std::strstr(refused.status.data(), "SAFETY VALIDATION") == nullptr);
    assert(store.write_count == writes_before_invalid);

    // The refusal a bench test produces, and the reason this is checkable away from a
    // circuit at all: a line captured in the garden is a valid line in the wrong place.
    // Both ends of both gates are shifted a few kilometres north, keeping each line its
    // original length, so nothing is degenerate and the geofence is what refuses it.
    {
        auto distant = source;
        constexpr double kNorthDeg = 0.05;
        distant.gates.start.left.latitude_deg += kNorthDeg;
        distant.gates.start.right.latitude_deg += kNorthDeg;
        distant.gates.finish.left.latitude_deg += kNorthDeg;
        distant.gates.finish.right.latitude_deg += kNorthDeg;
        controller.begin(source, &store, false);
        capture_all(controller, distant);
        const auto writes_before = store.write_count;
        controller.save(true);
        assert(controller.status() == ui::GateCaptureStatus::invalid_geometry);
        assert(controller.refusal().result ==
               track::TrackLoadResult::geometry_outside_geofence);
        const auto outside = controller.view_model();
        assert(std::strstr(outside.status.data(), "OUTSIDE THIS CIRCUIT'S GEOFENCE") !=
               nullptr);
        assert(store.write_count == writes_before);
    }

    // And the reason does not outlive the attempt it belongs to.
    controller.begin(source, &store, false);
    assert(controller.refusal().result == track::TrackLoadResult::empty);
    controller.cancel();
    assert(controller.status() == ui::GateCaptureStatus::closed);

    track::TrackDefinitionBlob serialized{};
    assert(track::serialize_track_definition(source, serialized) ==
           track::TrackSerializeResult::serialized);
    track::TrackDefinition round_trip{};
    assert(track::load_track_definition(
               {serialized.bytes.data(), serialized.size}, round_trip).result ==
           track::TrackLoadResult::loaded);
    assert(std::strcmp(round_trip.track_id.data(), source.track_id.data()) == 0);

    const auto directory = std::filesystem::temp_directory_path() /
                           "track-timer-gate-capture-test";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    simulator::FileTrackDefinitionStore file_store{directory};
    assert(file_store.write_atomic(source.track_id.data(), serialized));
    track::TrackDefinitionBlob file_blob{};
    assert(file_store.read(source.track_id.data(), file_blob) ==
           track::TrackStoreReadResult::loaded);
    const auto main_path = directory / "synthetic_test_loop.json";
    const auto backup_path = directory / "synthetic_test_loop.json.bak";
    std::filesystem::rename(main_path, backup_path);
    {
        std::ofstream interrupted{directory / "synthetic_test_loop.json.tmp"};
        interrupted << "{interrupted";
    }
    assert(file_store.read(source.track_id.data(), file_blob) ==
           track::TrackStoreReadResult::loaded);
    assert(std::filesystem::exists(main_path));
    std::filesystem::remove_all(directory, error);

    for (const auto status : {ui::GateCaptureStatus::closed,
                              ui::GateCaptureStatus::editing,
                              ui::GateCaptureStatus::endpoint_captured,
                              ui::GateCaptureStatus::active_session,
                              ui::GateCaptureStatus::moving,
                              ui::GateCaptureStatus::stale_fix,
                              ui::GateCaptureStatus::unusable_fix,
                              ui::GateCaptureStatus::poor_accuracy,
                              ui::GateCaptureStatus::incomplete,
                              ui::GateCaptureStatus::invalid_geometry,
                              ui::GateCaptureStatus::overwrite_confirmation,
                              ui::GateCaptureStatus::saved,
                              ui::GateCaptureStatus::storage_error}) {
        assert(std::strlen(ui::gate_capture_status_name(status)) > 0);
    }

    std::cout << "Two-gate capture, the instructions matching it, named geometry "
                 "refusals, safety rejection, atomic persistence, and recovery passed\n";
    return 0;
}

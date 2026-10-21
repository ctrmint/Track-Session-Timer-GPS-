#include "track_timer/ui/gate_capture.hpp"

#include "track_timer/track/capture.hpp"
#include "track_timer/track/matching.hpp"
#include "track_timer/ui/foundation.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

constexpr std::array<const char*, kGateCaptureGateCount> kGateNames{"START", "FINISH"};

const char* status_text(const GateCaptureStatus status) noexcept
{
    switch (status) {
    case GateCaptureStatus::closed:
        return "GATE CAPTURE CLOSED";
    case GateCaptureStatus::editing:
        return "PARK SAFELY AT THE PHYSICAL ENDPOINT, THEN CAPTURE";
    case GateCaptureStatus::endpoint_captured:
        // Written with the live count in write_status; this is the fallback.
        return "ENDPOINT CAPTURED";
    case GateCaptureStatus::active_session:
        return "CAPTURE BLOCKED - STOP THE ACTIVE SESSION FIRST";
    case GateCaptureStatus::moving:
        return "CAPTURE BLOCKED - VEHICLE MUST BE STATIONARY";
    case GateCaptureStatus::stale_fix:
        return "CAPTURE BLOCKED - WAIT FOR A FRESH GPS FIX";
    case GateCaptureStatus::unusable_fix:
        return "CAPTURE BLOCKED - GPS FIX IS NOT USABLE";
    case GateCaptureStatus::poor_accuracy:
        return "CAPTURE BLOCKED - GPS ACCURACY MUST BE 5 M OR BETTER";
    case GateCaptureStatus::incomplete:
        // The gates are named rather than counted: a count goes stale when the shape of a
        // circuit changes, as this text did, and "START AND FINISH" is also the clearer
        // instruction to somebody holding the device at the line.
        return "SAVE BLOCKED - CAPTURE BOTH ENDS OF THE START AND FINISH LINES";
    case GateCaptureStatus::invalid_geometry:
        return "SAVE BLOCKED - GATE GEOMETRY FAILED SAFETY VALIDATION";
    case GateCaptureStatus::overwrite_confirmation:
        return "EXISTING JSON FOUND - PRESS CONFIRM OVERWRITE TO REPLACE IT";
    case GateCaptureStatus::saved:
        return "SAVED AND RELOADED - START AND FINISH LINES ARE READY";
    case GateCaptureStatus::storage_error:
        return "SAVE FAILED - PREVIOUS VALID JSON REMAINS IN PLACE";
    }
    return "GATE CAPTURE";
}

// Why validation refused the captured geometry, in the terms of the thing to go and do
// about it. Three results are worth translating because they are the three a capture can
// actually produce; anything else is reported as the validator named it rather than
// guessed at.
void write_refusal(std::array<char, 144>& status,
                   const track::TrackLoadReport& refusal) noexcept
{
    switch (refusal.result) {
    case track::TrackLoadResult::degenerate_start_gate:
    case track::TrackLoadResult::degenerate_finish_gate:
        std::snprintf(status.data(), status.size(),
                      "SAVE BLOCKED - LINE TOO SHORT: THE TWO ENDS MUST BE AT LEAST 1 M "
                      "APART");
        return;
    case track::TrackLoadResult::geometry_outside_geofence:
        std::snprintf(status.data(), status.size(),
                      "SAVE BLOCKED - AN ENDPOINT IS OUTSIDE THIS CIRCUIT'S GEOFENCE");
        return;
    case track::TrackLoadResult::invalid_value:
        // Three checks share this result for a captured line: the projection failing
        // because the endpoints are nowhere near the circuit, a line over a kilometre
        // long, and a line within ten degrees of the direction of travel. From here there
        // is no telling which of them refused it, so all three are named rather than one
        // being picked and presented as the cause.
        std::snprintf(status.data(), status.size(),
                      "SAVE BLOCKED - %s REFUSED. CHECK ITS LENGTH, ITS ANGLE TO THE "
                      "DIRECTION OF TRAVEL, AND THAT IT IS AT THIS CIRCUIT",
                      track::track_definition_field_name(refusal.error_field));
        return;
    default:
        break;
    }
    std::snprintf(status.data(), status.size(), "SAVE BLOCKED - GEOMETRY REFUSED: %s",
                  track::track_load_result_name(refusal.result));
}

// The status line. Counted states are formatted from kGateCaptureEndpointCount rather than
// from a number typed into a string, which is how the eight-endpoint instruction outlived
// the eight endpoints.
void write_status(std::array<char, 144>& status, const GateCaptureStatus state,
                  const std::size_t captured, const track::TrackLoadReport& refusal) noexcept
{
    if (state == GateCaptureStatus::endpoint_captured) {
        std::snprintf(status.data(), status.size(),
                      "ENDPOINT CAPTURED - %zu OF %zu DONE", captured,
                      kGateCaptureEndpointCount);
        return;
    }
    if (state == GateCaptureStatus::invalid_geometry) {
        write_refusal(status, refusal);
        return;
    }
    std::snprintf(status.data(), status.size(), "%s", status_text(state));
}

GateCaptureStatus from_endpoint_result(const track::EndpointCaptureResult result) noexcept
{
    switch (result) {
    case track::EndpointCaptureResult::captured:
        return GateCaptureStatus::endpoint_captured;
    case track::EndpointCaptureResult::active_session:
        return GateCaptureStatus::active_session;
    case track::EndpointCaptureResult::moving:
        return GateCaptureStatus::moving;
    case track::EndpointCaptureResult::stale_fix:
        return GateCaptureStatus::stale_fix;
    case track::EndpointCaptureResult::unusable_fix:
        return GateCaptureStatus::unusable_fix;
    case track::EndpointCaptureResult::poor_accuracy:
        return GateCaptureStatus::poor_accuracy;
    }
    return GateCaptureStatus::unusable_fix;
}

}  // namespace

void GateCaptureController::begin(const track::TrackDefinition& definition,
                                  track::TrackDefinitionStore* store,
                                  const bool session_active) noexcept
{
    draft_ = definition;
    store_ = store;
    if (store_ != nullptr) {
        track::TrackDefinitionBlob stored_blob{};
        track::TrackDefinition stored_definition{};
        if (store_->read(definition.track_id.data(), stored_blob) ==
                track::TrackStoreReadResult::loaded &&
            track::load_track_definition(
                {stored_blob.bytes.data(), stored_blob.size}, stored_definition).result ==
                track::TrackLoadResult::loaded &&
            std::strcmp(stored_definition.track_id.data(), definition.track_id.data()) == 0) {
            draft_ = stored_definition;
        }
    }
    latest_fix_ = {};
    evaluation_monotonic_us_ = domain::kUnavailableTime;
    refusal_ = {};
    captured_.fill(false);
    gate_index_ = 0;
    right_endpoint_ = false;
    status_ = session_active ? GateCaptureStatus::active_session
                             : GateCaptureStatus::editing;
}

void GateCaptureController::update_fix(const domain::GnssFix& fix,
                                       const std::int64_t evaluation_monotonic_us) noexcept
{
    latest_fix_ = fix;
    evaluation_monotonic_us_ = evaluation_monotonic_us;
}

void GateCaptureController::previous_gate() noexcept
{
    if (status_ == GateCaptureStatus::closed) {
        return;
    }
    gate_index_ = gate_index_ == 0 ? kGateCaptureGateCount - 1 : gate_index_ - 1;
    status_ = GateCaptureStatus::editing;
}

void GateCaptureController::next_gate() noexcept
{
    if (status_ == GateCaptureStatus::closed) {
        return;
    }
    gate_index_ = (gate_index_ + 1) % kGateCaptureGateCount;
    status_ = GateCaptureStatus::editing;
}

void GateCaptureController::toggle_endpoint() noexcept
{
    if (status_ == GateCaptureStatus::closed) {
        return;
    }
    right_endpoint_ = !right_endpoint_;
    status_ = GateCaptureStatus::editing;
}

void GateCaptureController::capture(const bool session_active) noexcept
{
    auto* gate = selected_gate();
    if (gate == nullptr) {
        return;
    }
    track::GeographicPoint point{};
    const auto result = track::capture_stationary_endpoint(
        latest_fix_, evaluation_monotonic_us_, session_active, point);
    status_ = from_endpoint_result(result);
    if (result != track::EndpointCaptureResult::captured) {
        return;
    }
    if (right_endpoint_) {
        gate->right = point;
    }
    else {
        gate->left = point;
    }
    captured_[gate_index_ * 2 + (right_endpoint_ ? 1U : 0U)] = true;
}

void GateCaptureController::save(const bool confirm_overwrite) noexcept
{
    if (!complete()) {
        status_ = GateCaptureStatus::incomplete;
        return;
    }
    if (store_ == nullptr) {
        status_ = GateCaptureStatus::storage_error;
        return;
    }
    if (store_->exists(draft_.track_id.data()) && !confirm_overwrite) {
        status_ = GateCaptureStatus::overwrite_confirmation;
        return;
    }
    auto candidate = draft_;
    if (candidate.revision < UINT32_MAX) {
        ++candidate.revision;
    }
    candidate.provenance.geometry_status =
        track::TrackGeometryStatus::device_captured;
    track::TrackDefinitionBlob blob{};
    refusal_ = {};
    const auto serialized =
        track::serialize_track_definition(candidate, blob, &refusal_);
    if (serialized != track::TrackSerializeResult::serialized) {
        // The round trip leaves its report; a blob that would not fit at all leaves none,
        // so the one result that says so is set here rather than reported as an empty
        // definition the validator never saw.
        if (serialized == track::TrackSerializeResult::capacity_exceeded) {
            refusal_ = {track::TrackLoadResult::capacity_exceeded, 0,
                        track::TrackDefinitionField::none};
        }
        status_ = GateCaptureStatus::invalid_geometry;
        return;
    }
    if (!store_->write_atomic(candidate.track_id.data(), blob)) {
        status_ = GateCaptureStatus::storage_error;
        return;
    }
    track::TrackDefinitionBlob reloaded_blob{};
    track::TrackDefinition reloaded{};
    if (store_->read(candidate.track_id.data(), reloaded_blob) !=
            track::TrackStoreReadResult::loaded ||
        track::load_track_definition(
            {reloaded_blob.bytes.data(), reloaded_blob.size}, reloaded).result !=
            track::TrackLoadResult::loaded) {
        status_ = GateCaptureStatus::storage_error;
        return;
    }
    draft_ = reloaded;
    status_ = GateCaptureStatus::saved;
}

void GateCaptureController::cancel() noexcept
{
    captured_.fill(false);
    status_ = GateCaptureStatus::closed;
}

GateCaptureStatus GateCaptureController::status() const noexcept
{
    return status_;
}

std::size_t GateCaptureController::gate_index() const noexcept
{
    return gate_index_;
}

bool GateCaptureController::right_endpoint_selected() const noexcept
{
    return right_endpoint_;
}

std::size_t GateCaptureController::captured_endpoint_count() const noexcept
{
    return static_cast<std::size_t>(
        std::count(captured_.begin(), captured_.end(), true));
}

const track::TrackDefinition& GateCaptureController::draft() const noexcept
{
    return draft_;
}

const track::TrackLoadReport& GateCaptureController::refusal() const noexcept
{
    return refusal_;
}

GateCaptureViewModel GateCaptureController::view_model() const noexcept
{
    GateCaptureViewModel model{};
    std::snprintf(model.track.data(), model.track.size(), "%.44s | REV %u",
                  draft_.name.data(), static_cast<unsigned>(draft_.revision));
    std::snprintf(model.selection.data(), model.selection.size(), "%s | %s | %zu/%zu",
                  kGateNames[gate_index_], right_endpoint_ ? "RIGHT" : "LEFT",
                  captured_endpoint_count(), kGateCaptureEndpointCount);
    std::snprintf(model.endpoint_label.data(), model.endpoint_label.size(), "%s ENDPOINT",
                  right_endpoint_ ? "RIGHT" : "LEFT");
    std::snprintf(model.save_label.data(), model.save_label.size(), "%s",
                  status_ == GateCaptureStatus::overwrite_confirmation
                      ? "CONFIRM OVERWRITE"
                      : "SAVE JSON");
    if (latest_fix_.arrival_monotonic_us == domain::kUnavailableTime ||
        evaluation_monotonic_us_ == domain::kUnavailableTime) {
        std::snprintf(model.fix.data(), model.fix.size(), "GPS FIX: UNAVAILABLE");
    }
    else {
        const auto age_ms = evaluation_monotonic_us_ >= latest_fix_.arrival_monotonic_us
                                ? (evaluation_monotonic_us_ -
                                   latest_fix_.arrival_monotonic_us) /
                                      1'000
                                : 0;
        std::snprintf(model.fix.data(), model.fix.size(),
                      "%.7f, %.7f | +/- %.1f m | AGE %lld ms",
                      latest_fix_.latitude_deg, latest_fix_.longitude_deg,
                      static_cast<double>(latest_fix_.horizontal_accuracy_m),
                      static_cast<long long>(age_ms));
    }
    const auto* gate = selected_gate();
    const auto left_done = captured_[gate_index_ * 2];
    const auto right_done = captured_[gate_index_ * 2 + 1];
    if (gate != nullptr && left_done && right_done) {
        std::snprintf(model.preview.data(), model.preview.size(),
                      "LINE READY | %.1f m | HEADING %.0f deg",
                      track::geographic_distance_m(gate->left, gate->right),
                      gate->direction_heading_deg);
    }
    else {
        std::snprintf(model.preview.data(), model.preview.size(),
                      "LINE PREVIEW: LEFT %s | RIGHT %s",
                      left_done ? "OK" : "MISSING", right_done ? "OK" : "MISSING");
    }
    write_status(model.status, status_, captured_endpoint_count(), refusal_);
    model.status_color_rgb = status_ == GateCaptureStatus::saved ||
                                     status_ == GateCaptureStatus::endpoint_captured
                                 ? color::positive_bright
                             : status_ == GateCaptureStatus::editing
                                 ? color::text_secondary
                                 : color::caution_bright;
    model.can_capture = status_ != GateCaptureStatus::closed &&
                        status_ != GateCaptureStatus::saved;
    model.can_save = complete();
    model.confirming_overwrite =
        status_ == GateCaptureStatus::overwrite_confirmation;
    return model;
}

track::DirectedGateDefinition* GateCaptureController::selected_gate() noexcept
{
    switch (gate_index_) {
    case 0:
        return &draft_.gates.start;
    case 1:
        return &draft_.gates.finish;
    }
    return nullptr;
}

const track::DirectedGateDefinition* GateCaptureController::selected_gate() const noexcept
{
    return const_cast<GateCaptureController*>(this)->selected_gate();
}

bool GateCaptureController::complete() const noexcept
{
    return std::all_of(captured_.begin(), captured_.end(), [](const bool value) {
        return value;
    });
}

const char* gate_capture_status_name(const GateCaptureStatus status) noexcept
{
    switch (status) {
    case GateCaptureStatus::closed:
        return "closed";
    case GateCaptureStatus::editing:
        return "editing";
    case GateCaptureStatus::endpoint_captured:
        return "endpoint-captured";
    case GateCaptureStatus::active_session:
        return "active-session";
    case GateCaptureStatus::moving:
        return "moving";
    case GateCaptureStatus::stale_fix:
        return "stale-fix";
    case GateCaptureStatus::unusable_fix:
        return "unusable-fix";
    case GateCaptureStatus::poor_accuracy:
        return "poor-accuracy";
    case GateCaptureStatus::incomplete:
        return "incomplete";
    case GateCaptureStatus::invalid_geometry:
        return "invalid-geometry";
    case GateCaptureStatus::overwrite_confirmation:
        return "overwrite-confirmation";
    case GateCaptureStatus::saved:
        return "saved";
    case GateCaptureStatus::storage_error:
        return "storage-error";
    }
    return "closed";
}

}  // namespace track_timer::ui

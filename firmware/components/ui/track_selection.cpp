#include "track_timer/ui/track_selection.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cstdio>
#include <cstring>

namespace track_timer::ui {
namespace {

const char* status_text(const TrackSelectionStatus status) noexcept
{
    switch (status) {
    case TrackSelectionStatus::closed:
        return "TRACK SELECTION CLOSED";
    case TrackSelectionStatus::browsing:
        return "MANUAL CHOICE - CONFIRM SELECT OR USE TIMER ONLY";
    case TrackSelectionStatus::suggested:
        return "ONE NEARBY TRACK FOUND - CONFIRM BEFORE LAP TIMING";
    case TrackSelectionStatus::ambiguous:
        return "MULTIPLE NEARBY TRACKS - CHOOSE THE CORRECT DEFINITION";
    case TrackSelectionStatus::no_nearby_track:
        return "NO NEARBY TRACK - BROWSE MANUALLY OR USE TIMER ONLY";
    case TrackSelectionStatus::location_unavailable:
        return "GPS LOCATION UNAVAILABLE - MANUAL SELECTION REMAINS AVAILABLE";
    case TrackSelectionStatus::selected_track_missing:
        return "SAVED TRACK IS MISSING - SELECT AGAIN OR USE TIMER ONLY";
    case TrackSelectionStatus::invalid_catalog:
        return "TRACK DATA INVALID - LAP TIMING DISABLED; TIMER REMAINS SAFE";
    case TrackSelectionStatus::saved:
        return "TRACK SAVED - START/FINISH DEFINITION READY";
    case TrackSelectionStatus::timer_only:
        return "TRACK CLEARED - SESSION TIMER ONLY";
    case TrackSelectionStatus::storage_error:
        return "SAVE FAILED - PREVIOUS TRACK SELECTION IS UNCHANGED";
    case TrackSelectionStatus::locked_active:
        return "TRACK CHANGES LOCKED WHILE A SESSION IS ACTIVE";
    case TrackSelectionStatus::capture_information:
        return "UNKNOWN-TRACK CAPTURE IS A SEPARATE SAFE WORKFLOW IN #37";
    }
    return "TRACK SELECTION";
}

bool same_id(const std::array<char, settings::kTrackIdentifierCapacity>& selected,
             const track::TrackDefinition& definition) noexcept
{
    return std::strncmp(selected.data(), definition.track_id.data(), selected.size()) == 0;
}

}  // namespace

void TrackSelectionController::begin(const track::TrackCatalogView catalog,
                                     const track::TrackMatchResult& match,
                                     const settings::DeviceSettings& current,
                                     const bool session_active) noexcept
{
    catalog_ = catalog;
    match_ = match;
    browse_index_ = catalog.count == 0 ? track::kNoTrackIndex : 0;
    if (session_active) {
        status_ = TrackSelectionStatus::locked_active;
        return;
    }
    set_initial_status(match, current);
}

void TrackSelectionController::previous() noexcept
{
    if (!catalog_usable()) {
        return;
    }
    browse_index_ = browse_index_ == 0 ? catalog_.count - 1 : browse_index_ - 1;
    status_ = TrackSelectionStatus::browsing;
}

void TrackSelectionController::next() noexcept
{
    if (!catalog_usable()) {
        return;
    }
    browse_index_ = (browse_index_ + 1) % catalog_.count;
    status_ = TrackSelectionStatus::browsing;
}

settings::SettingsApplyResult TrackSelectionController::select(
    settings::SettingsManager& manager, const bool session_active) noexcept
{
    if (session_active) {
        status_ = TrackSelectionStatus::locked_active;
        return settings::SettingsApplyResult::invalid_settings;
    }
    if (!catalog_usable()) {
        status_ = TrackSelectionStatus::invalid_catalog;
        return settings::SettingsApplyResult::invalid_settings;
    }
    auto updated = manager.current();
    updated.selected_track_id.fill('\0');
    std::snprintf(updated.selected_track_id.data(), updated.selected_track_id.size(), "%s",
                  catalog_.definitions[browse_index_].track_id.data());
    const auto result = manager.apply(updated, false);
    status_ = result == settings::SettingsApplyResult::applied
                  ? TrackSelectionStatus::saved
              : result == settings::SettingsApplyResult::storage_error
                  ? TrackSelectionStatus::storage_error
                  : TrackSelectionStatus::invalid_catalog;
    return result;
}

settings::SettingsApplyResult TrackSelectionController::use_timer_only(
    settings::SettingsManager& manager, const bool session_active) noexcept
{
    if (session_active) {
        status_ = TrackSelectionStatus::locked_active;
        return settings::SettingsApplyResult::invalid_settings;
    }
    auto updated = manager.current();
    updated.selected_track_id.fill('\0');
    const auto result = manager.apply(updated, false);
    status_ = result == settings::SettingsApplyResult::applied
                  ? TrackSelectionStatus::timer_only
              : result == settings::SettingsApplyResult::storage_error
                  ? TrackSelectionStatus::storage_error
                  : TrackSelectionStatus::invalid_catalog;
    return result;
}

void TrackSelectionController::show_capture_information() noexcept
{
    if (status_ != TrackSelectionStatus::locked_active) {
        status_ = TrackSelectionStatus::capture_information;
    }
}

void TrackSelectionController::cancel() noexcept
{
    status_ = TrackSelectionStatus::closed;
}

std::size_t TrackSelectionController::browse_index() const noexcept
{
    return browse_index_;
}

TrackSelectionStatus TrackSelectionController::status() const noexcept
{
    return status_;
}

TrackSelectionViewModel TrackSelectionController::view_model() const noexcept
{
    TrackSelectionViewModel model{};
    const bool usable = catalog_usable();
    if (usable) {
        const auto& definition = catalog_.definitions[browse_index_];
        std::snprintf(model.position.data(), model.position.size(), "TRACK %u OF %u",
                      static_cast<unsigned>(browse_index_ + 1),
                      static_cast<unsigned>(catalog_.count));
        std::snprintf(model.track_name.data(), model.track_name.size(), "%s",
                      definition.name.data());
        std::array<char, 17> hash{};
        track::format_definition_hash(definition.definition_hash, hash);
        std::snprintf(model.definition.data(), model.definition.size(),
                      "START/FINISH READY | SCHEMA v%u | %.8s",
                      static_cast<unsigned>(definition.schema_version), hash.data());
    }
    else {
        std::snprintf(model.position.data(), model.position.size(), "NO VALID TRACKS");
        std::snprintf(model.track_name.data(), model.track_name.size(), "TIMER ONLY AVAILABLE");
        std::snprintf(model.definition.data(), model.definition.size(),
                      "NO START/FINISH DEFINITION");
    }
    std::snprintf(model.status.data(), model.status.size(), "%s", status_text(status_));
    std::snprintf(model.select_label.data(), model.select_label.size(), "%s",
                  status_ == TrackSelectionStatus::suggested ? "CONFIRM TRACK" : "SELECT TRACK");
    model.status_color_rgb = status_ == TrackSelectionStatus::saved
                                 ? color::positive_bright
                             : status_ == TrackSelectionStatus::invalid_catalog ||
                                       status_ == TrackSelectionStatus::selected_track_missing ||
                                       status_ == TrackSelectionStatus::storage_error ||
                                       status_ == TrackSelectionStatus::locked_active
                                 ? color::critical_bright
                                 : color::caution_bright;
    model.can_browse = usable && status_ != TrackSelectionStatus::locked_active;
    model.can_select = model.can_browse;
    model.can_use_timer_only = status_ != TrackSelectionStatus::locked_active;
    return model;
}

bool TrackSelectionController::catalog_usable() const noexcept
{
    return catalog_.definitions != nullptr && catalog_.count > 0 &&
           catalog_.count <= track::kMaximumCatalogTracks &&
           match_.state != track::TrackMatchState::invalid_catalog &&
           browse_index_ < catalog_.count;
}

void TrackSelectionController::set_initial_status(
    const track::TrackMatchResult& match,
    const settings::DeviceSettings& current) noexcept
{
    switch (match.state) {
    case track::TrackMatchState::selected:
        browse_index_ = match.selected_index;
        status_ = TrackSelectionStatus::saved;
        break;
    case track::TrackMatchState::suggested:
        browse_index_ = match.selected_index;
        status_ = TrackSelectionStatus::suggested;
        break;
    case track::TrackMatchState::ambiguous:
        browse_index_ = match.candidate_count > 0 ? match.candidate_indices[0] : browse_index_;
        status_ = TrackSelectionStatus::ambiguous;
        break;
    case track::TrackMatchState::no_match:
        status_ = TrackSelectionStatus::no_nearby_track;
        break;
    case track::TrackMatchState::location_unavailable:
        status_ = TrackSelectionStatus::location_unavailable;
        break;
    case track::TrackMatchState::selected_track_missing:
        status_ = TrackSelectionStatus::selected_track_missing;
        break;
    case track::TrackMatchState::invalid_catalog:
        browse_index_ = track::kNoTrackIndex;
        status_ = TrackSelectionStatus::invalid_catalog;
        break;
    }
    if (catalog_usable() && current.selected_track_id[0] != '\0') {
        for (std::size_t index = 0; index < catalog_.count; ++index) {
            if (same_id(current.selected_track_id, catalog_.definitions[index])) {
                browse_index_ = index;
                break;
            }
        }
    }
}

}  // namespace track_timer::ui

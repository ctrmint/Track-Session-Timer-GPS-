#include "track_timer/ui/navigation.hpp"

namespace track_timer::ui {

NavigationResult NavigationController::dispatch(const NavigationAction action) noexcept
{
    NavigationResult result{destination_, destination_, false, false};

    if (action == NavigationAction::session_ended) {
        session_active_ = false;
        destination_ = Destination::ready;
        result.current = destination_;
        result.accepted = true;
        return result;
    }

    if (session_active_) {
        return result;
    }

    switch (action) {
    case NavigationAction::start_session:
        if (destination_ == Destination::ready) {
            session_active_ = true;
            destination_ = Destination::active;
            result.accepted = true;
            result.start_requested = true;
        }
        break;
    case NavigationAction::open_setup:
        if (destination_ == Destination::ready) {
            destination_ = Destination::setup;
            result.accepted = true;
        }
        break;
    case NavigationAction::open_review:
        if (destination_ == Destination::ready) {
            destination_ = Destination::review;
            result.accepted = true;
        }
        break;
    case NavigationAction::open_diagnostics:
        if (destination_ == Destination::ready) {
            destination_ = Destination::diagnostics;
            result.accepted = true;
        }
        break;
    case NavigationAction::back:
        if (destination_ == Destination::setup || destination_ == Destination::review ||
            destination_ == Destination::diagnostics) {
            destination_ = Destination::ready;
            result.accepted = true;
        }
        break;
    case NavigationAction::session_ended:
        break;
    case NavigationAction::rest_started:
        if (destination_ == Destination::ready || destination_ == Destination::review) {
            destination_ = Destination::rest;
            result.accepted = true;
        }
        break;
    }

    result.current = destination_;
    return result;
}

void NavigationController::synchronize_session(const bool active) noexcept
{
    session_active_ = active;
    if (active) {
        destination_ = Destination::active;
    }
    else if (destination_ == Destination::active) {
        destination_ = Destination::ready;
    }
}

Destination NavigationController::destination() const noexcept
{
    return destination_;
}

bool NavigationController::session_active() const noexcept
{
    return session_active_;
}

bool NavigationController::configuration_allowed() const noexcept
{
    return !session_active_;
}

const char* destination_name(const Destination destination) noexcept
{
    switch (destination) {
    case Destination::ready:
        return "ready";
    case Destination::active:
        return "active";
    case Destination::setup:
        return "setup";
    case Destination::review:
        return "review";
    case Destination::diagnostics:
        return "diagnostics";
    case Destination::rest:
        return "rest";
    }
    return "ready";
}

}  // namespace track_timer::ui

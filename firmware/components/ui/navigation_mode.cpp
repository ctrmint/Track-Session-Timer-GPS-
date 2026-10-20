#include "track_timer/ui/navigation_mode.hpp"

#include <cstdio>

namespace track_timer::ui {

NavigationViewModel navigation_view(const NavigationSnapshot& snapshot) noexcept
{
    NavigationViewModel model{};
    model.speed = speed_only_view(snapshot.gnss);
    model.time = clock_view(snapshot.clock);
    model.heading = compass_view(snapshot.gnss);

    // Only when no bytes have ever arrived. Every other combination has the readouts
    // disagreeing about what they know, and then each has to speak for itself: a parked
    // car with a good fix has a speed and a time and no heading, and collapsing that into
    // one line would lose the only part of it the driver cannot guess.
    //
    // The three valid flags are tested as well as the receiver flag, rather than trusted
    // to follow from it. They come from three independent view builders, and a readout
    // that somehow had a value here would be hidden by the single line.
    if (!snapshot.gnss.receiver_present && !model.speed.speed_valid &&
        !model.time.digits_valid && !model.heading.heading_valid) {
        model.has_shared_fault = true;
        model.shared_fault_rgb = model.speed.status_rgb;
        std::snprintf(model.shared_fault.data(), model.shared_fault.size(), "%s",
                      model.speed.status.data());
    }
    return model;
}

}  // namespace track_timer::ui

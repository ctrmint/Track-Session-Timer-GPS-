#include "track_timer/ui/acceleration.hpp"

#include "track_timer/ui/foundation.hpp"

#include <cmath>
#include <cstdio>

namespace track_timer::ui {
namespace {

constexpr const char* kNoValue = "--.--";
constexpr float kNanosecondsPerSecond = 1'000'000'000.0F;

void write_text(char* const destination, const std::size_t capacity,
                const char* const text) noexcept
{
    std::snprintf(destination, capacity, "%s", text);
}

[[nodiscard]] float mph_to_mps(const std::uint8_t mph) noexcept
{
    return static_cast<float>(mph) / kMetresPerSecondToMph;
}

}  // namespace

std::int64_t extrapolated_zero_ns(const std::int64_t first_ns, const float first_mps,
                                  const std::int64_t second_ns,
                                  const float second_mps) noexcept
{
    const auto rise = second_mps - first_mps;
    // Not accelerating between these two, so there is nothing to extrapolate along. The
    // first sample is the best estimate available rather than a division by zero.
    if (!(rise > 0.0F) || second_ns <= first_ns) {
        return first_ns;
    }
    const auto run_ns = static_cast<float>(second_ns - first_ns);
    const auto back_ns = first_mps * run_ns / rise;
    return first_ns - static_cast<std::int64_t>(std::lround(back_ns));
}

std::int64_t interpolated_crossing_ns(const std::int64_t before_ns, const float before_mps,
                                      const std::int64_t after_ns, const float after_mps,
                                      const float target_mps) noexcept
{
    const auto rise = after_mps - before_mps;
    if (!(rise > 0.0F) || after_ns <= before_ns) {
        return after_ns;
    }
    const auto fraction = (target_mps - before_mps) / rise;
    const auto span_ns = static_cast<float>(after_ns - before_ns);
    return before_ns + static_cast<std::int64_t>(std::lround(fraction * span_ns));
}

void AccelerationRun::reset() noexcept
{
    start_ns_ = domain::kUnavailableTime;
    finish_ns_ = domain::kUnavailableTime;
    has_previous_ = false;
    state_ = AccelerationState::armed;
}

void AccelerationRun::update(const AccelerationSnapshot& snapshot) noexcept
{
    const auto receiver = gps_only_state(snapshot.gnss);
    if (receiver != GpsOnlyState::fixed) {
        state_ = receiver == GpsOnlyState::no_receiver ? AccelerationState::no_receiver
                                                       : AccelerationState::no_fix;
        has_previous_ = false;
        return;
    }
    const auto& fix = snapshot.gnss.fix;
    if (!std::isfinite(fix.speed_accuracy_mps) || fix.speed_accuracy_mps <= 0.0F ||
        fix.speed_accuracy_mps > kAccelerationMaximumSpeedAccuracyMps) {
        // Measuring against this would report the receiver's uncertainty rather than the
        // car's acceleration, so no run starts and one in progress is abandoned.
        state_ = state_ == AccelerationState::running ? AccelerationState::abandoned
                                                      : AccelerationState::no_speed;
        has_previous_ = false;
        return;
    }

    // A finished or abandoned run holds its number until the car comes back to rest, which
    // is how it re-arms without being told. A tap does the same thing sooner.
    if (state_ == AccelerationState::complete || state_ == AccelerationState::abandoned) {
        if (fix.speed_mps < kAccelerationRestMps) {
            reset();
        }
        return;
    }

    const auto now_ns = fix.measurement_time_ns;
    const auto speed = fix.speed_mps;

    if (state_ == AccelerationState::running && last_fix_arrival_us_ != domain::kUnavailableTime &&
        snapshot.now_monotonic_us != domain::kUnavailableTime &&
        snapshot.now_monotonic_us - last_fix_arrival_us_ > kAccelerationStaleUs) {
        // The fixes stopped. The number would otherwise sit still, which is exactly what
        // arriving at the target looks like.
        state_ = AccelerationState::abandoned;
        return;
    }

    if (speed < kAccelerationRestMps) {
        // Stationary: armed, and any partial run is discarded rather than carried into the
        // next launch.
        start_ns_ = domain::kUnavailableTime;
        finish_ns_ = domain::kUnavailableTime;
        state_ = AccelerationState::armed;
    }
    else if (state_ == AccelerationState::armed && has_previous_) {
        // First motion. The start is where speed would have been zero, not where it was
        // first seen to be non-zero.
        start_ns_ = extrapolated_zero_ns(last_fix_ns_, last_speed_mps_, now_ns, speed);
        state_ = AccelerationState::running;
    }

    if (state_ == AccelerationState::running && has_previous_) {
        const auto target_mps = mph_to_mps(snapshot.target_mph);
        if (last_speed_mps_ < target_mps && speed >= target_mps) {
            finish_ns_ =
                interpolated_crossing_ns(last_fix_ns_, last_speed_mps_, now_ns, speed,
                                         target_mps);
            state_ = AccelerationState::complete;
        }
    }

    last_fix_ns_ = now_ns;
    last_speed_mps_ = speed;
    last_fix_arrival_us_ = fix.arrival_monotonic_us;
    has_previous_ = true;
}

AccelerationState AccelerationRun::state() const noexcept { return state_; }

float AccelerationRun::elapsed_seconds() const noexcept
{
    if (start_ns_ == domain::kUnavailableTime) {
        return 0.0F;
    }
    const auto end_ns = finish_ns_ != domain::kUnavailableTime ? finish_ns_ : last_fix_ns_;
    if (end_ns <= start_ns_) {
        return 0.0F;
    }
    return static_cast<float>(end_ns - start_ns_) / kNanosecondsPerSecond;
}

AccelerationViewModel AccelerationRun::view_model(
    const AccelerationSnapshot& snapshot) const noexcept
{
    AccelerationViewModel model{};
    model.state = state_;
    std::snprintf(model.target.data(), model.target.size(), "0-%u MPH",
                  static_cast<unsigned>(snapshot.target_mph));

    switch (state_) {
    case AccelerationState::no_receiver:
        write_text(model.time.data(), model.time.size(), kNoValue);
        write_text(model.status.data(), model.status.size(), "NO RECEIVER");
        model.status_rgb = color::critical_bright;
        model.time_rgb = color::critical_bright;
        return model;
    case AccelerationState::no_fix:
        write_text(model.time.data(), model.time.size(), kNoValue);
        write_text(model.status.data(), model.status.size(), "NO FIX");
        model.status_rgb = color::caution_bright;
        model.time_rgb = color::caution_bright;
        return model;
    case AccelerationState::no_speed:
        write_text(model.time.data(), model.time.size(), kNoValue);
        // Its own words: the fix is fine and the position is good, and sending somebody to
        // look at satellites would be the wrong problem.
        write_text(model.status.data(), model.status.size(), "NO SPEED");
        model.status_rgb = color::caution_bright;
        model.time_rgb = color::caution_bright;
        return model;
    case AccelerationState::abandoned:
        // The number it reached, so the run is not simply erased - but never green, and
        // said in words, because a frozen time looks exactly like a finished one.
        write_text(model.status.data(), model.status.size(), "LOST FIX");
        model.status_rgb = color::critical_bright;
        model.time_rgb = color::critical_bright;
        break;
    case AccelerationState::armed:
        write_text(model.status.data(), model.status.size(), "READY");
        model.status_rgb = color::text_secondary;
        model.time_rgb = color::text_primary;
        break;
    case AccelerationState::running:
        write_text(model.status.data(), model.status.size(), "RUN");
        model.status_rgb = color::text_secondary;
        model.time_rgb = color::text_primary;
        break;
    case AccelerationState::complete:
        write_text(model.status.data(), model.status.size(), "DONE");
        model.status_rgb = color::positive_bright;
        model.time_rgb = color::positive_bright;
        break;
    }

    auto seconds = elapsed_seconds();
    if (!(seconds >= 0.0F)) {
        seconds = 0.0F;
    }
    if (seconds > kAccelerationMaximumSeconds) {
        seconds = kAccelerationMaximumSeconds;
    }
    model.time_valid = true;
    // Two decimals, which is what the interpolation earns: without it every reading would
    // be quantised to the 40 ms between fixes and the second decimal would be decoration.
    std::snprintf(model.time.data(), model.time.size(), "%.2f",
                  static_cast<double>(seconds));
    return model;
}

const char* acceleration_state_name(const AccelerationState state) noexcept
{
    switch (state) {
    case AccelerationState::no_receiver:
        return "no-receiver";
    case AccelerationState::no_fix:
        return "no-fix";
    case AccelerationState::no_speed:
        return "no-speed";
    case AccelerationState::armed:
        return "armed";
    case AccelerationState::running:
        return "running";
    case AccelerationState::complete:
        return "complete";
    case AccelerationState::abandoned:
        return "abandoned";
    }
    return "unknown";
}

}  // namespace track_timer::ui

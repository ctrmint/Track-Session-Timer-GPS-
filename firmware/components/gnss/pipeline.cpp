#include "track_timer/gnss/pipeline.hpp"

#include <cmath>

namespace track_timer::gnss {
namespace {

// One DDC read returns at most the count the receiver declared, and a UART FIFO hands over
// whatever has arrived. 256 bytes is comfortably more than either produces in one 40 ms
// epoch, where a NAV-PVT frame is 100 bytes.
constexpr std::size_t kReadBufferBytes = 256;

// Bounded work per poll. At 25 Hz a poll should find about 100 bytes; 2 KB is twenty
// epochs, so a backlog is drained promptly without the pipeline ever being able to hold the
// task indefinitely. Whatever is left over is read on the next poll, and the gap counter
// records anything the receiver dropped in the meantime.
constexpr std::size_t kMaximumReadsPerPoll = 8;

// How quickly the cadence estimate follows a change. Slow enough that one jittery interval
// does not move it, fast enough to settle within a second or so.
constexpr double kCadenceSmoothing = 0.1;

constexpr double kNanosecondsPerSecond = 1.0e9;

}  // namespace

bool FixQueue::push(const domain::GnssFix& fix) noexcept
{
    if (count_ == fixes_.size()) {
        return false;
    }
    fixes_[(head_ + count_) % fixes_.size()] = fix;
    ++count_;
    return true;
}

bool FixQueue::pop(domain::GnssFix& output) noexcept
{
    if (count_ == 0) {
        return false;
    }
    output = fixes_[head_];
    head_ = (head_ + 1) % fixes_.size();
    --count_;
    return true;
}

std::size_t FixQueue::size() const noexcept { return count_; }

bool FixQueue::empty() const noexcept { return count_ == 0; }

void FixQueue::clear() noexcept
{
    head_ = 0;
    count_ = 0;
}

GnssPipeline::GnssPipeline(const GnssPipelineConfig& config) noexcept
    : config_(config), validator_(config.quality), status_(config.quality)
{
}

// Gaps are measured on the receiver's own clock, never on arrival cadence. Arrival says
// when this system got round to looking, which on a polled bus is a statement about the
// CPU and not about the receiver. The measurement interval is the only evidence of what
// the receiver actually produced, and it is the same evidence whichever transport carried
// it - which is why this still catches a receiver that reset or a rate setting that
// silently reverted, on UART, where nothing is being polled at all.
void GnssPipeline::note_cadence(const domain::GnssFix& accepted) noexcept
{
    const auto expected_ns = expected_interval_ns(config_.rate);
    if (last_accepted_measurement_ns_ != domain::kUnavailableTime) {
        const auto interval_ns = accepted.measurement_time_ns - last_accepted_measurement_ns_;
        const auto gap_threshold_ns = static_cast<std::int64_t>(
            static_cast<double>(expected_ns) * static_cast<double>(config_.gap_interval_multiple));
        if (interval_ns > gap_threshold_ns) {
            ++metrics_.gap_events;
            // How many epochs the gap accounts for. Rounding to nearest means a slightly
            // long interval reports the one fix it lost rather than two.
            const auto epochs = static_cast<std::int64_t>(
                std::llround(static_cast<double>(interval_ns) / static_cast<double>(expected_ns)));
            if (epochs > 1) {
                metrics_.missed_fixes += static_cast<std::uint32_t>(epochs - 1);
            }
        }
        const auto interval = static_cast<double>(interval_ns);
        mean_interval_ns_ = has_cadence_
                                ? mean_interval_ns_ + kCadenceSmoothing * (interval - mean_interval_ns_)
                                : interval;
        has_cadence_ = true;
        metrics_.observed_rate_hz =
            mean_interval_ns_ > 0.0
                ? static_cast<float>(kNanosecondsPerSecond / mean_interval_ns_)
                : 0.0F;
    }
    last_accepted_measurement_ns_ = accepted.measurement_time_ns;
}

void GnssPipeline::ingest(const std::uint8_t byte, const std::int64_t now_monotonic_us) noexcept
{
    if (!parser_.consume(byte)) {
        return;
    }
    const auto message = parser_.message();
    domain::GnssFix decoded{};
    if (!decode_nav_pvt(message, decoded)) {
        // A valid frame that is not the one we asked for. Counted rather than ignored: a
        // receiver sending something else is a configuration that did not take (#16).
        ++metrics_.unsupported_messages;
        return;
    }

    const auto judged = validator_.evaluate(decoded, now_monotonic_us);
    status_.observe(judged);

    if (!judged.accepted_for_timing) {
        ++metrics_.fixes_rejected;
        return;
    }
    ++metrics_.fixes_accepted;
    note_cadence(judged);

    if (!queue_.push(judged)) {
        // Accepted, then lost because nobody took it. A different fault from every other
        // kind of loss here, and the only one that is this system's own fault.
        ++metrics_.queue_drops;
    }
    if (queue_.size() > metrics_.queue_high_water) {
        metrics_.queue_high_water = static_cast<std::uint32_t>(queue_.size());
    }
}

std::size_t GnssPipeline::poll(GnssTransport& transport,
                               const std::int64_t now_monotonic_us) noexcept
{
    ++metrics_.polls;
    // How late this poll was. On a polled bus this is the number that explains a gap: the
    // receiver's buffer holds about 410 ms of 25 Hz traffic, so a poll interval approaching
    // that is a loss waiting to happen, and one past it has already caused one.
    if (last_poll_us_ != domain::kUnavailableTime) {
        const auto interval_us = now_monotonic_us - last_poll_us_;
        if (interval_us > metrics_.maximum_poll_interval_us) {
            metrics_.maximum_poll_interval_us = interval_us;
        }
    }
    last_poll_us_ = now_monotonic_us;

    metrics_.transport_healthy = transport.healthy();

    const auto accepted_before = metrics_.fixes_accepted;
    std::array<std::uint8_t, kReadBufferBytes> buffer{};
    for (std::size_t read_index = 0; read_index < kMaximumReadsPerPoll; ++read_index) {
        const auto count = transport.read(buffer.data(), buffer.size());
        if (count == 0) {
            break;
        }
        metrics_.bytes_read += static_cast<std::uint32_t>(count);
        status_.note_traffic(now_monotonic_us);
        for (std::size_t index = 0; index < count && index < buffer.size(); ++index) {
            ingest(buffer[index], now_monotonic_us);
        }
        if (count < buffer.size()) {
            break;
        }
    }

    // Parser counters are cumulative, so the pipeline's view is their delta. Mirroring them
    // rather than recomputing keeps one definition of what a checksum error is.
    const auto& parser_counters = parser_.counters();
    metrics_.frames += parser_counters.frames - previous_parser_counters_.frames;
    metrics_.checksum_errors +=
        parser_counters.checksum_errors - previous_parser_counters_.checksum_errors;
    metrics_.oversized_frames +=
        parser_counters.oversized_frames - previous_parser_counters_.oversized_frames;
    metrics_.discarded_bytes +=
        parser_counters.discarded_bytes - previous_parser_counters_.discarded_bytes;
    previous_parser_counters_ = parser_counters;

    return metrics_.fixes_accepted - accepted_before;
}

FixQueue& GnssPipeline::queue() noexcept { return queue_; }

const FixQueue& GnssPipeline::queue() const noexcept { return queue_; }

const GnssPipelineMetrics& GnssPipeline::metrics() const noexcept { return metrics_; }

const FixValidator& GnssPipeline::validator() const noexcept { return validator_; }

domain::GnssHealth GnssPipeline::health(const std::int64_t now_monotonic_us) const noexcept
{
    return status_.health(now_monotonic_us);
}

void GnssPipeline::reset() noexcept
{
    parser_.reset();
    validator_.reset();
    status_.reset();
    queue_.clear();
    metrics_ = {};
    previous_parser_counters_ = {};
    last_poll_us_ = domain::kUnavailableTime;
    last_accepted_measurement_ns_ = domain::kUnavailableTime;
    mean_interval_ns_ = 0.0;
    has_cadence_ = false;
}

}  // namespace track_timer::gnss

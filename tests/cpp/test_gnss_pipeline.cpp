#include "track_timer/gnss/pipeline.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace track_timer;

void put_u32(std::vector<std::uint8_t>& out, const std::size_t offset,
             const std::uint32_t value)
{
    out[offset] = static_cast<std::uint8_t>(value & 0xFFU);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void put_i32(std::vector<std::uint8_t>& out, const std::size_t offset,
             const std::int32_t value)
{
    put_u32(out, offset, static_cast<std::uint32_t>(value));
}

std::vector<std::uint8_t> frame(const std::uint8_t message_class, const std::uint8_t id,
                                const std::vector<std::uint8_t>& payload)
{
    const auto length = static_cast<std::uint16_t>(payload.size());
    std::vector<std::uint8_t> body{message_class, id,
                                   static_cast<std::uint8_t>(length & 0xFFU),
                                   static_cast<std::uint8_t>((length >> 8U) & 0xFFU)};
    body.insert(body.end(), payload.begin(), payload.end());

    std::uint8_t ck_a = 0;
    std::uint8_t ck_b = 0;
    for (const auto byte : body) {
        ck_a = static_cast<std::uint8_t>(ck_a + byte);
        ck_b = static_cast<std::uint8_t>(ck_b + ck_a);
    }

    std::vector<std::uint8_t> out{gnss::kSyncChar1, gnss::kSyncChar2};
    out.insert(out.end(), body.begin(), body.end());
    out.push_back(ck_a);
    out.push_back(ck_b);
    return out;
}

constexpr std::int32_t kBaseLatitudeE7 = 520'619'000;
constexpr std::int32_t kBaseLongitudeE7 = -10'238'000;
// 2.5 m of latitude, which is how far a car at 62.6 m/s travels in one 40 ms epoch.
constexpr std::int32_t kLatitudeStepE7 = 225;

std::vector<std::uint8_t> nav_pvt_payload(const std::uint32_t itow_ms,
                                          const std::int32_t latitude_e7)
{
    std::vector<std::uint8_t> payload(gnss::kNavPvtLength, 0);
    put_u32(payload, 0, itow_ms);
    payload[11] = 0x07;  // validDate | validTime | fullyResolved
    payload[20] = 3;     // 3D fix
    payload[21] = 0x01;  // gnssFixOK
    payload[23] = 14;    // numSV
    put_i32(payload, 24, kBaseLongitudeE7);
    put_i32(payload, 28, latitude_e7);
    put_i32(payload, 32, 120'500);
    put_u32(payload, 40, 1'500);     // hAcc 1.5 m
    put_i32(payload, 60, 62'600);    // 62.6 m/s, about 140 mph
    put_i32(payload, 64, 0);         // due north, so the latitude march is consistent
    put_u32(payload, 68, 50);        // sAcc 0.05 m/s
    put_u32(payload, 72, 50'000);    // headAcc 0.5 deg
    return payload;
}

// A receiver with a buffer that can actually overrun.
//
// The finite buffer is the whole point. A fake that queued without limit could never
// reproduce the failure this pipeline exists to catch: on a polled bus, a CPU that stops
// asking does not slow the receiver down, it loses what the receiver produced meanwhile,
// and nothing anywhere reports an error.
class FakeReceiver : public gnss::GnssTransport {
  public:
    explicit FakeReceiver(const std::size_t buffer_capacity = 1024) noexcept
        : capacity_(buffer_capacity)
    {
    }

    // One epoch of the receiver's own time, whether or not anyone is listening.
    void produce_epoch() noexcept
    {
        itow_ms_ += kEpochMs;
        latitude_e7_ += kLatitudeStepE7;
        const auto bytes = frame(gnss::kClassNav, gnss::kIdNavPvt,
                                 nav_pvt_payload(itow_ms_, latitude_e7_));
        if (buffer_.size() + bytes.size() > capacity_) {
            ++overruns_;
            return;
        }
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }

    // The receiver's clock and position advance, but nothing is emitted. This is what an
    // overrun looks like from the outside: the epoch happened, and we never saw it.
    void skip_epoch() noexcept
    {
        itow_ms_ += kEpochMs;
        latitude_e7_ += kLatitudeStepE7;
    }

    void produce_raw(const std::vector<std::uint8_t>& bytes) noexcept
    {
        if (buffer_.size() + bytes.size() > capacity_) {
            ++overruns_;
            return;
        }
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }

    std::size_t read(std::uint8_t* const output, const std::size_t capacity) noexcept override
    {
        const auto count = std::min(capacity, buffer_.size());
        std::memcpy(output, buffer_.data(), count);
        buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(count));
        return count;
    }

    [[nodiscard]] bool healthy() const noexcept override { return healthy_; }

    void set_healthy(const bool value) noexcept { healthy_ = value; }
    [[nodiscard]] std::uint32_t overruns() const noexcept { return overruns_; }
    [[nodiscard]] std::size_t pending() const noexcept { return buffer_.size(); }

    static constexpr std::uint32_t kEpochMs = 40;

  private:
    std::vector<std::uint8_t> buffer_{};
    std::size_t capacity_;
    std::uint32_t itow_ms_{400'000};
    std::int32_t latitude_e7_{kBaseLatitudeE7};
    std::uint32_t overruns_{0};
    bool healthy_{true};
};

constexpr std::int64_t kEpochUs = 40'000;

void a_clean_stream_reaches_the_queue()
{
    FakeReceiver receiver{};
    gnss::GnssPipeline pipeline{};

    std::int64_t now = 1'000'000;
    for (int epoch = 0; epoch < 10; ++epoch) {
        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);
    }

    const auto& metrics = pipeline.metrics();
    assert(metrics.fixes_accepted == 10);
    assert(metrics.fixes_rejected == 0);
    assert(metrics.gap_events == 0);
    assert(metrics.missed_fixes == 0);
    assert(metrics.queue_drops == 0);
    assert(metrics.checksum_errors == 0);
    assert(pipeline.queue().size() == 10);
    assert(pipeline.health(now) == domain::GnssHealth::good);
}

// The heart of the design, in two halves. Gaps are read from the receiver's own clock, so
// an epoch the receiver never sent is caught even when the bytes arrive perfectly evenly,
// and a poll that ran late is NOT reported as loss when the receiver missed nothing.
void gaps_are_measured_on_the_receiver_clock_not_on_arrival()
{
    // Delivered late is not lost. Two epochs sat in the receiver's buffer and both arrived
    // on one poll: the stream is continuous, so there is no gap to report.
    {
        FakeReceiver receiver{};
        gnss::GnssPipeline pipeline{};

        receiver.produce_epoch();
        (void)pipeline.poll(receiver, 1'000'000);

        receiver.produce_epoch();
        receiver.produce_epoch();
        (void)pipeline.poll(receiver, 1'000'000 + 3 * kEpochUs);

        const auto& metrics = pipeline.metrics();
        assert(metrics.fixes_accepted == 3);
        assert(metrics.gap_events == 0);
        assert(metrics.missed_fixes == 0);
    }

    // Epochs the receiver produced that never reached us. Arrival is perfectly regular
    // throughout, so only the receiver's own clock can reveal this.
    {
        FakeReceiver receiver{};
        gnss::GnssPipeline pipeline{};
        std::int64_t now = 1'000'000;

        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);

        receiver.skip_epoch();
        receiver.skip_epoch();
        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);

        const auto& metrics = pipeline.metrics();
        assert(metrics.fixes_accepted == 2);
        assert(metrics.gap_events == 1);
        assert(metrics.missed_fixes == 2);
        // Every byte the transport handed over parsed cleanly. The loss happened upstream
        // of anything the parser or the bus could have noticed.
        assert(metrics.checksum_errors == 0);
        assert(metrics.discarded_bytes == 0);
    }

    // A poll that ran very late, with nothing actually lost, must not be reported as loss.
    {
        FakeReceiver receiver{};
        gnss::GnssPipeline pipeline{};

        receiver.produce_epoch();
        (void)pipeline.poll(receiver, 1'000'000);

        receiver.produce_epoch();
        receiver.produce_epoch();
        (void)pipeline.poll(receiver, 1'500'000);

        const auto& metrics = pipeline.metrics();
        assert(metrics.fixes_accepted == 3);
        assert(metrics.gap_events == 0);
        // The lateness is still recorded, as the thing that explains a future gap.
        assert(metrics.maximum_poll_interval_us == 500'000);
    }
}

// The failure this pipeline exists to catch, reproduced end to end: the CPU stops asking,
// the receiver's buffer overruns, epochs are lost, and nothing anywhere reports an error.
// A detector never seen to fire is not evidence of anything.
void a_starved_poll_loses_fixes_and_the_counter_trips()
{
    FakeReceiver receiver{1024};  // about ten frames
    gnss::GnssPipeline pipeline{};

    std::int64_t now = 1'000'000;
    for (int epoch = 0; epoch < 5; ++epoch) {
        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);
    }
    assert(pipeline.metrics().gap_events == 0);

    // The UI task takes the core for a second. The receiver keeps producing regardless,
    // its buffer fills, and everything after that is gone.
    for (int epoch = 0; epoch < 25; ++epoch) {
        receiver.produce_epoch();
        now += kEpochUs;
    }
    const auto lost = receiver.overruns();
    assert(lost > 0);  // the fake really did lose data, so the test is testing something

    // Draining the backlog shows nothing wrong yet, and this is a property of gap
    // detection worth stating rather than discovering later: a hole is only visible once
    // its far edge arrives. Everything buffered is continuous, so until the stream
    // resumes, loss reads as silence - which is why health and the gap counter are
    // separate instruments rather than one.
    (void)pipeline.poll(receiver, now);
    assert(pipeline.metrics().gap_events == 0);

    // The stream resumes, and the discontinuity becomes visible.
    receiver.produce_epoch();
    now += kEpochUs;
    (void)pipeline.poll(receiver, now);

    const auto& metrics = pipeline.metrics();
    assert(metrics.gap_events == 1);
    assert(metrics.missed_fixes == lost);
    // The cause is recorded beside the symptom, so a trace says why and not only what.
    assert(metrics.maximum_poll_interval_us >= 25 * kEpochUs);
    // Nothing else noticed: the bus was healthy, every byte parsed, no checksum failed.
    // That is precisely why this counter has to exist.
    assert(metrics.transport_healthy);
    assert(metrics.checksum_errors == 0);
    assert(metrics.discarded_bytes == 0);
}

// A bounded queue turns a slow consumer into a counted drop now, instead of a memory fault
// later and somewhere else.
void the_queue_is_bounded_and_drops_are_counted()
{
    FakeReceiver receiver{64 * 1024};
    gnss::GnssPipeline pipeline{};

    std::int64_t now = 1'000'000;
    const auto capacity = gnss::FixQueue::capacity();
    for (std::size_t epoch = 0; epoch < capacity + 10; ++epoch) {
        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);
    }

    const auto& metrics = pipeline.metrics();
    assert(pipeline.queue().size() == capacity);
    assert(metrics.queue_high_water == capacity);
    assert(metrics.queue_drops == 10);
    assert(metrics.fixes_accepted == capacity + 10);

    // Two seconds of headroom at 25 Hz is the requirement the capacity exists to meet.
    assert(capacity >= 50);
}

void draining_the_queue_preserves_order()
{
    FakeReceiver receiver{64 * 1024};
    gnss::GnssPipeline pipeline{};

    std::int64_t now = 1'000'000;
    for (int epoch = 0; epoch < 8; ++epoch) {
        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);
    }

    domain::GnssFix fix{};
    std::int64_t previous = -1;
    std::uint32_t drained = 0;
    while (pipeline.queue().pop(fix)) {
        assert(fix.measurement_time_ns > previous);
        previous = fix.measurement_time_ns;
        ++drained;
    }
    assert(drained == 8);
    assert(pipeline.queue().empty());
}

void the_observed_rate_follows_the_receiver()
{
    FakeReceiver receiver{64 * 1024};
    gnss::GnssPipeline pipeline{};

    std::int64_t now = 1'000'000;
    for (int epoch = 0; epoch < 60; ++epoch) {
        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);
    }
    assert(std::fabs(pipeline.metrics().observed_rate_hz - 25.0F) < 0.5F);
}

void a_refused_fix_is_counted_and_never_queued()
{
    FakeReceiver receiver{64 * 1024};
    gnss::GnssPipeline pipeline{};

    auto payload = nav_pvt_payload(400'040, kBaseLatitudeE7);
    payload[21] = 0x00;  // gnssFixOK clear
    receiver.produce_raw(frame(gnss::kClassNav, gnss::kIdNavPvt, payload));

    (void)pipeline.poll(receiver, 1'000'000);

    const auto& metrics = pipeline.metrics();
    assert(metrics.frames == 1);
    assert(metrics.fixes_rejected == 1);
    assert(metrics.fixes_accepted == 0);
    assert(pipeline.queue().empty());
    // Present and talking, but not yet usable: not the same as absent.
    assert(pipeline.health(1'000'000) == domain::GnssHealth::searching);
}

// A receiver still emitting NMEA, or sending a message nobody asked for, is a configuration
// that did not take (#16) rather than a fault. Both are counted, neither stalls the stream.
void noise_and_unexpected_messages_are_counted_not_fatal()
{
    FakeReceiver receiver{64 * 1024};
    gnss::GnssPipeline pipeline{};

    const std::string nmea = "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M*47\r\n";
    receiver.produce_raw({nmea.begin(), nmea.end()});
    receiver.produce_raw(frame(gnss::kClassNav, 0x02, std::vector<std::uint8_t>(20, 0)));
    receiver.produce_epoch();

    (void)pipeline.poll(receiver, 1'000'000);

    const auto& metrics = pipeline.metrics();
    assert(metrics.discarded_bytes == nmea.size());
    assert(metrics.unsupported_messages == 1);
    assert(metrics.fixes_accepted == 1);
}

// A bus that cannot be read at all is a different fault from a receiver with nothing to
// say, and the two must not arrive as one.
void a_failing_bus_is_reported_separately_from_a_silent_receiver()
{
    FakeReceiver receiver{};
    gnss::GnssPipeline pipeline{};

    (void)pipeline.poll(receiver, 1'000'000);
    assert(pipeline.metrics().transport_healthy);
    assert(pipeline.health(1'000'000) == domain::GnssHealth::unavailable);

    receiver.set_healthy(false);
    (void)pipeline.poll(receiver, 1'040'000);
    assert(!pipeline.metrics().transport_healthy);
}

// A pipeline that could spin until a busy receiver fell silent would starve the task it
// shares a core with, which is the very fault it exists to detect.
void work_per_poll_is_bounded()
{
    FakeReceiver receiver{512 * 1024};
    gnss::GnssPipeline pipeline{};

    for (int epoch = 0; epoch < 200; ++epoch) {
        receiver.produce_epoch();
    }
    const auto backlog = receiver.pending();

    (void)pipeline.poll(receiver, 1'000'000);
    assert(pipeline.metrics().bytes_read < backlog);
    assert(receiver.pending() > 0);

    // The rest is drained over subsequent polls rather than lost.
    for (int poll = 0; poll < 100 && receiver.pending() > 0; ++poll) {
        (void)pipeline.poll(receiver, 1'000'000 + kEpochUs * (poll + 1));
    }
    assert(receiver.pending() == 0);
    assert(pipeline.metrics().fixes_accepted == 200);
}

void a_reset_clears_the_account()
{
    FakeReceiver receiver{64 * 1024};
    gnss::GnssPipeline pipeline{};
    receiver.produce_epoch();
    (void)pipeline.poll(receiver, 1'000'000);
    assert(pipeline.metrics().fixes_accepted == 1);

    pipeline.reset();
    assert(pipeline.metrics().fixes_accepted == 0);
    assert(pipeline.metrics().polls == 0);
    assert(pipeline.queue().empty());
    assert(pipeline.health(1'000'000) == domain::GnssHealth::unavailable);
}

void the_configured_rate_sets_what_counts_as_a_gap()
{
    assert(gnss::expected_interval_ns(gnss::FixRate::hz25) == 40'000'000);
    assert(gnss::expected_interval_ns(gnss::FixRate::hz20) == 50'000'000);

    // At 20 Hz a 40 ms step is early rather than late, and must not read as a gap.
    gnss::GnssPipelineConfig config{};
    config.rate = gnss::FixRate::hz20;
    gnss::GnssPipeline pipeline{config};
    FakeReceiver receiver{64 * 1024};

    std::int64_t now = 1'000'000;
    for (int epoch = 0; epoch < 10; ++epoch) {
        receiver.produce_epoch();
        now += kEpochUs;
        (void)pipeline.poll(receiver, now);
    }
    assert(pipeline.metrics().gap_events == 0);
}

}  // namespace

int main()
{
    a_clean_stream_reaches_the_queue();
    gaps_are_measured_on_the_receiver_clock_not_on_arrival();
    a_starved_poll_loses_fixes_and_the_counter_trips();
    the_queue_is_bounded_and_drops_are_counted();
    draining_the_queue_preserves_order();
    the_observed_rate_follows_the_receiver();
    a_refused_fix_is_counted_and_never_queued();
    noise_and_unexpected_messages_are_counted_not_fatal();
    a_failing_bus_is_reported_separately_from_a_silent_receiver();
    work_per_poll_is_bounded();
    a_reset_clears_the_account();
    the_configured_rate_sets_what_counts_as_a_gap();

    std::cout << "GNSS pipeline: bounded queue and bounded work per poll, loss counted by "
                 "cause, and a starved poll proven to trip the gap counter\n";
    return 0;
}

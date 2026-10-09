#pragma once

#include "track_timer/domain/contracts.hpp"
#include "track_timer/gnss/fix_validation.hpp"
#include "track_timer/gnss/transport.hpp"
#include "track_timer/gnss/ubx.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace track_timer::gnss {

// Bytes in, judged fixes out, and an honest account of everything lost on the way.
//
// The account is the point. A lost fix has no symptom of its own: the device keeps
// running, the screen keeps updating, and the only trace is a lap time that does not
// repeat. On a polled transport there is not even a watchdog - this device's UI task has
// starved twice, on the carousel image transform (#133) and the glyph cache (#141), and
// both times the task watchdog turned it into a reboot that announced itself. A starved
// GNSS poll announces nothing.
//
// So every way a fix can go missing is counted separately, because they have different
// causes and different fixes: bytes the transport never handed over, frames the parser
// threw away, fixes the quality gate refused, epochs the receiver never sent, and fixes
// the consumer was too slow to take.

// The cadence the receiver has been configured for (#16). Gap detection needs it: a lost
// epoch is only visible as a measurement interval longer than the one the receiver was
// told to produce.
enum class FixRate : std::uint8_t {
    hz20,
    hz25,
};

[[nodiscard]] constexpr std::int64_t expected_interval_ns(const FixRate rate) noexcept
{
    return rate == FixRate::hz20 ? 50'000'000 : 40'000'000;
}

struct GnssPipelineConfig {
    FixRate rate{FixRate::hz25};
    FixQualityPolicy quality{};
    // A measurement interval longer than this multiple of the expected one is a gap. 1.5
    // sits halfway between one interval and two: ordinary jitter cannot be mistaken for
    // loss, and a single lost epoch cannot hide inside it.
    float gap_interval_multiple{1.5F};
};

struct GnssPipelineMetrics {
    // The transport and the parser
    std::uint32_t polls{0};
    std::uint32_t bytes_read{0};
    std::uint32_t frames{0};
    std::uint32_t checksum_errors{0};
    std::uint32_t oversized_frames{0};
    std::uint32_t discarded_bytes{0};
    std::uint32_t unsupported_messages{0};

    // The quality gate
    std::uint32_t fixes_accepted{0};
    std::uint32_t fixes_rejected{0};

    // Loss, counted by cause
    std::uint32_t gap_events{0};    // how many times the stream skipped
    std::uint32_t missed_fixes{0};  // how many epochs those gaps account for
    std::uint32_t queue_drops{0};   // accepted, then dropped because nobody took it

    // Headroom and cadence
    std::uint32_t queue_high_water{0};
    std::int64_t maximum_poll_interval_us{0};
    float observed_rate_hz{0.0F};
    bool transport_healthy{true};
};

// A bounded ring of fixes between the pipeline and whatever consumes them.
//
// Bounded on purpose: an unbounded queue turns a slow consumer into a memory fault much
// later and somewhere else, where a bounded one turns it into a counted drop at the moment
// it happens. domain::queue_capacity::gnss_fixes is 64, which a static_assert there holds
// at two seconds or more of headroom.
class FixQueue {
  public:
    [[nodiscard]] bool push(const domain::GnssFix& fix) noexcept;
    [[nodiscard]] bool pop(domain::GnssFix& output) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] static constexpr std::size_t capacity() noexcept
    {
        return domain::queue_capacity::gnss_fixes;
    }

    void clear() noexcept;

  private:
    std::array<domain::GnssFix, domain::queue_capacity::gnss_fixes> fixes_{};
    std::size_t head_{0};
    std::size_t count_{0};
};

class GnssPipeline {
  public:
    GnssPipeline() noexcept = default;
    explicit GnssPipeline(const GnssPipelineConfig& config) noexcept;

    // Drains whatever the transport has and turns it into judged fixes, returning how many
    // were accepted this call. Work per call is bounded: a pipeline that could spin until
    // a busy receiver fell silent would starve the task it shares a core with, which is the
    // very fault it exists to detect.
    std::size_t poll(GnssTransport& transport, std::int64_t now_monotonic_us) noexcept;

    [[nodiscard]] FixQueue& queue() noexcept;
    [[nodiscard]] const FixQueue& queue() const noexcept;
    [[nodiscard]] const GnssPipelineMetrics& metrics() const noexcept;
    [[nodiscard]] const FixValidator& validator() const noexcept;
    [[nodiscard]] domain::GnssHealth health(std::int64_t now_monotonic_us) const noexcept;

    void reset() noexcept;

  private:
    void ingest(std::uint8_t byte, std::int64_t now_monotonic_us) noexcept;
    void note_cadence(const domain::GnssFix& accepted) noexcept;

    GnssPipelineConfig config_{};
    UbxParser parser_{};
    FixValidator validator_{};
    ReceiverStatus status_{};
    FixQueue queue_{};
    GnssPipelineMetrics metrics_{};
    UbxParserCounters previous_parser_counters_{};
    std::int64_t last_poll_us_{domain::kUnavailableTime};
    std::int64_t last_accepted_measurement_ns_{domain::kUnavailableTime};
    double mean_interval_ns_{0.0};
    bool has_cadence_{false};
};

}  // namespace track_timer::gnss

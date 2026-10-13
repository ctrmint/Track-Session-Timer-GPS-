#pragma once

#include "track_timer/gnss/transport.hpp"

#include <cstddef>
#include <cstdint>

namespace track_timer::gnss {

// Telling the receiver what to emit, and confirming it agreed.
//
// A NEO-M9N in its factory default emits NMEA, which this timer does not parse. So the
// device reads a receiver that is working perfectly and understands none of it: bytes
// arrive, no frame passes checksum, and nothing anywhere reports an error (#16).
//
// The confirmation is the part worth being careful about. A configuration that silently did
// not take is the failure most likely to pass on the bench and reappear in the car, because
// the bench run happens minutes after the settings were written and the car runs after a
// power cycle. So nothing here infers success from fixes arriving later: the receiver either
// acknowledged the frame or it did not, and both are reported.

// How long the receiver keeps a setting.
//
// RAM alone is lost on the next power cycle. Writing all three means the configuration
// survives being unplugged, which is the difference between a device that works once and a
// device that works in a vehicle.
struct ConfigurationLayers {
    bool ram{true};
    bool battery_backed{true};
    bool flash{true};
};

struct GnssConfigurationRequest {
    // Milliseconds between measurements. 50 is 20 Hz, which ADR-005 and #16 both want
    // proven before 25 Hz is attempted.
    std::uint16_t measurement_period_ms{50};
    ConfigurationLayers layers{};
    // Leaving NMEA enabled costs bus bandwidth on a bus shared with the display's touch
    // controller, the IMU and the RTC, and every byte of it has to be discarded by the
    // parser - where it would land in discarded_bytes and make a healthy link look noisy.
    bool silence_nmea{true};
};

enum class ConfigurationOutcome : std::uint8_t {
    // The transport cannot write at all, so there is nothing to attempt. A replayed fixture
    // is the honest case for this.
    not_attempted,
    // The bus refused the frame. The receiver never saw it, so this is wiring rather than
    // configuration.
    write_failed,
    // The frame was sent and nothing came back. Distinct from a refusal, because a refusal
    // proves the receiver is listening and this does not.
    unacknowledged,
    // The receiver understood the frame and refused its contents: a wrong key, or a value
    // out of range. The opposite diagnosis from silence, and the opposite fix.
    rejected,
    acknowledged,
};

struct GnssConfigurationReport {
    ConfigurationOutcome outcome{ConfigurationOutcome::not_attempted};
    std::uint32_t frames_seen{0};
    std::uint32_t acks{0};
    std::uint32_t naks{0};
    std::uint32_t bytes_read{0};
    // True when the receiver answered the form that prefixes the frame with the DDC stream
    // register rather than the raw form.
    //
    // Which form a u-blox receiver expects for an incoming message is not settled by the
    // documentation reachable for this part: the register map is specified for reads, and
    // implementations differ. Rather than pick one and hope, both are tried and the one the
    // receiver acknowledges is recorded - so the answer comes from the hardware, and a
    // future part that wants the other form still configures.
    bool stream_prefixed{false};
};

// Waiting between writing a frame and looking for its acknowledgement.
//
// The caller's, because the two callers are not alike: a host test has a fake that answers
// immediately and must not sleep, while the device has to yield so the frame is actually on
// the wire and the reply has arrived. Null means do not wait.
using WaitForReply = void (*)(void* context);

// Which copy of a setting to read. These are layer *numbers* for CFG-VALGET, not the
// bitmask CFG-VALSET writes with, and the difference is easy to miss: VALSET takes a mask
// because it writes several at once, VALGET takes one number because a value can differ
// between layers and a read has to say which it means.
enum class ConfigurationLayer : std::uint8_t {
    ram = 0,
    battery_backed = 1,
    flash = 2,
    factory_default = 7,
};

// What the receiver says it has stored, read back rather than inferred.
//
// This is what settles whether a configuration survives being unplugged. Observing it any
// other way needs the log from a power-on boot, and that is the one boot this hardware
// cannot capture: the USB device is inside the chip, so cutting power destroys the port a
// listener would be holding (#181). Asking the receiver what is in its flash needs no power
// cycle and no listener, and is a stronger claim than an acknowledgement - an ACK says the
// write was accepted, this says the value is there.
struct StoredConfiguration {
    bool answered{false};  // false means the receiver did not reply at all
    bool nav_pvt_enabled{false};
    bool nmea_silenced{false};
    std::uint16_t measurement_period_ms{0};
    std::uint32_t keys_returned{0};
};

// Reads the settings this firmware writes, from one layer.
[[nodiscard]] StoredConfiguration read_stored_configuration(GnssTransport& transport,
                                                            ConfigurationLayer layer,
                                                            WaitForReply wait = nullptr,
                                                            void* wait_context = nullptr) noexcept;

// What the receiver was already emitting before anything was written to it.
//
// This is the only way to tell whether a stored configuration survived. The firmware
// configures the receiver on every boot, which makes a device that persisted its settings
// and one that reverted to defaults behave identically from the first fix onwards - so
// "written to flash and acknowledged" cannot be upgraded to "observed to persist" without
// looking before writing.
//
// It also matters beyond verification: a receiver that loses power independently of the
// board comes back in its factory default, and nothing can handle a reconnect it cannot
// detect.
struct ReceiverArrivalReport {
    std::uint32_t bytes{0};
    std::uint32_t frames{0};
    std::uint32_t nav_pvt{0};
    // True when the receiver said nothing at all in the window, which is neither of the
    // interesting answers: a receiver still starting up has not yet revealed its
    // configuration, and reporting that as "arrived in defaults" would be a guess.
    [[nodiscard]] bool silent() const noexcept { return bytes == 0; }
    // NAV-PVT before anything was written means the receiver did not need configuring.
    //
    // Not by itself proof that a configuration persisted: across a chip reset the receiver
    // never lost power, so a RAM-only setting would look identical. It is evidence only
    // across a real power cycle, whose log this hardware cannot capture (#181), which is
    // what read_stored_configuration is for.
    [[nodiscard]] bool already_configured() const noexcept { return nav_pvt > 0; }
};

// Listens to the receiver without writing to it.
[[nodiscard]] ReceiverArrivalReport observe_receiver(GnssTransport& transport,
                                                     std::uint32_t polls,
                                                     WaitForReply wait = nullptr,
                                                     void* wait_context = nullptr) noexcept;

// Long enough for a receiver emitting at 1 Hz - the factory NMEA default - to say something,
// since a shorter window would report a healthy default-configured receiver as silent.
inline constexpr std::uint32_t kArrivalPolls = 120;

// Configures the receiver and reports whether it agreed. Never blocks indefinitely: the
// acknowledgement search is bounded, so a receiver that says nothing costs a known delay
// once at startup rather than a stalled boot.
[[nodiscard]] GnssConfigurationReport configure_receiver(GnssTransport& transport,
                                                         const GnssConfigurationRequest& request,
                                                         WaitForReply wait = nullptr,
                                                         void* wait_context = nullptr) noexcept;

// Builds the CFG-VALSET payload for a request, returning its length or 0 if it would not
// fit. Exposed so a test can assert the bytes rather than only the behaviour: a wrong key
// or a value written at the wrong width is invisible in a fake that acknowledges anything.
[[nodiscard]] std::size_t build_valset_payload(const GnssConfigurationRequest& request,
                                               std::uint8_t* output,
                                               std::size_t capacity) noexcept;

// Enough for the five settings written, each a four-byte key and a one or two byte value,
// after the four-byte header.
inline constexpr std::size_t kValsetPayloadCapacity = 64;

// How many read attempts to spend looking for an acknowledgement, and the gap the caller is
// expected to leave between them. 20 at 10 ms is 200 ms per form, which is generous for a
// receiver that answers in tens of milliseconds and still bounded at boot.
inline constexpr std::uint32_t kAcknowledgementPolls = 20;
inline constexpr std::uint32_t kAcknowledgementPollIntervalMs = 10;

}  // namespace track_timer::gnss

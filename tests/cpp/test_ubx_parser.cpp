#include "track_timer/gnss/ubx.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

using namespace track_timer;

void put_u32(std::vector<std::uint8_t>& out, const std::size_t offset,
             const std::uint32_t value)
{
    out[offset + 0] = static_cast<std::uint8_t>(value & 0xFFU);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void put_i32(std::vector<std::uint8_t>& out, const std::size_t offset,
             const std::int32_t value)
{
    put_u32(out, offset, static_cast<std::uint32_t>(value));
}

// Wraps a payload in UBX framing with a correct checksum, so a test only has to describe the
// corruption it is interested in.
std::vector<std::uint8_t> frame(const std::uint8_t message_class, const std::uint8_t id,
                                const std::vector<std::uint8_t>& payload,
                                const std::uint16_t declared_length_override = 0xFFFF)
{
    const auto length = declared_length_override == 0xFFFF
                            ? static_cast<std::uint16_t>(payload.size())
                            : declared_length_override;
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

// A realistic fix: a car at 140 mph, which is the condition the transport decision was sized
// against.
std::vector<std::uint8_t> nav_pvt_payload()
{
    std::vector<std::uint8_t> payload(gnss::kNavPvtLength, 0);
    put_u32(payload, 0, 123'456'789);        // iTOW ms
    payload[11] = 0x07;                      // validDate | validTime | fullyResolved
    put_i32(payload, 16, -500'000);          // nano, -0.5 ms
    payload[20] = 3;                         // 3D fix
    payload[21] = 0x01;                      // gnssFixOK
    payload[23] = 14;                        // numSV
    put_i32(payload, 24, -10'238'000);       // lon -1.0238 deg
    put_i32(payload, 28, 520'619'000);       // lat 52.0619 deg
    put_i32(payload, 32, 120'500);           // height 120.5 m
    put_u32(payload, 40, 1'500);             // hAcc 1.5 m
    put_i32(payload, 60, 62'600);            // gSpeed 62.6 m/s, about 140 mph
    put_i32(payload, 64, 12'345'000);        // headMot 123.45 deg
    put_u32(payload, 68, 50);                // sAcc 0.05 m/s
    put_u32(payload, 72, 50'000);            // headAcc 0.5 deg
    return payload;
}

bool near(const double actual, const double expected, const double tolerance)
{
    return std::fabs(actual - expected) <= tolerance;
}

// Feeds a stream in fixed-size chunks, collecting every decoded fix. The chunk size is what
// varies between transports, and the parser must not care.
std::vector<domain::GnssFix> run(gnss::UbxParser& parser,
                                 const std::vector<std::uint8_t>& stream,
                                 const std::size_t chunk)
{
    std::vector<domain::GnssFix> fixes;
    for (std::size_t index = 0; index < stream.size(); index += chunk) {
        const auto end = std::min(index + chunk, stream.size());
        for (std::size_t at = index; at < end; ++at) {
            if (parser.consume(stream[at])) {
                domain::GnssFix fix{};
                if (gnss::decode_nav_pvt(parser.message(), fix)) {
                    fixes.push_back(fix);
                }
            }
        }
    }
    return fixes;
}

void a_valid_fix_is_framed_and_decoded()
{
    gnss::UbxParser parser;
    const auto stream = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    const auto fixes = run(parser, stream, stream.size());

    assert(fixes.size() == 1);
    const auto& fix = fixes.front();
    // iTOW in ns plus the signed sub-millisecond correction.
    assert(fix.measurement_time_ns == 123'456'789LL * 1'000'000 - 500'000);
    assert(near(fix.latitude_deg, 52.0619, 1e-7));
    assert(near(fix.longitude_deg, -1.0238, 1e-7));
    assert(near(fix.height_m, 120.5, 0.001));
    assert(near(fix.speed_mps, 62.6, 0.001));
    assert(near(fix.heading_deg, 123.45, 0.001));
    assert(near(fix.horizontal_accuracy_m, 1.5, 0.001));
    assert(near(fix.speed_accuracy_mps, 0.05, 0.001));
    assert(near(fix.heading_accuracy_deg, 0.5, 0.001));
    assert(fix.num_satellites == 14);
    assert(fix.fix_type == domain::FixType::fix_3d);
    // Both flag bytes survive intact for the quality rules to judge later.
    assert((fix.valid_flags & 0xFFU) == 0x07U);
    assert(((fix.valid_flags >> 8U) & 0xFFU) == 0x01U);

    assert(parser.counters().frames == 1);
    assert(parser.counters().checksum_errors == 0);
    assert(parser.counters().discarded_bytes == 0);
}

// The reason the parser is byte-at-a-time: a UART FIFO hands over whatever has arrived, while
// a DDC read returns exactly the count the receiver declared. Neither may change the outcome.
void chunking_cannot_change_the_result()
{
    const auto stream = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    std::int64_t reference = 0;

    for (const std::size_t chunk : {std::size_t{1}, std::size_t{2}, std::size_t{7},
                                    std::size_t{31}, stream.size(), stream.size() * 2}) {
        gnss::UbxParser parser;
        const auto fixes = run(parser, stream, chunk);
        assert(fixes.size() == 1);
        if (reference == 0) {
            reference = fixes.front().measurement_time_ns;
        }
        assert(fixes.front().measurement_time_ns == reference);
        assert(parser.counters().frames == 1);
    }
}

// The acceptance criterion: malformed data must not stall recovery of what follows.
void a_corrupt_frame_does_not_stall_the_next_one()
{
    auto corrupt = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    corrupt[20] ^= 0x5A;  // flip a payload byte so the checksum fails

    auto stream = corrupt;
    const auto good = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    stream.insert(stream.end(), good.begin(), good.end());

    gnss::UbxParser parser;
    const auto fixes = run(parser, stream, 1);
    assert(fixes.size() == 1);
    assert(parser.counters().checksum_errors == 1);
    assert(parser.counters().frames == 1);
}

// A length field that lies must be refused rather than buffered, and must not consume the
// frames behind it.
void an_oversized_length_is_refused_and_recovery_continues()
{
    auto lying = frame(gnss::kClassNav, 0x02, {}, 0xFFFE);
    auto stream = lying;
    const auto good = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    stream.insert(stream.end(), good.begin(), good.end());

    gnss::UbxParser parser;
    const auto fixes = run(parser, stream, 1);
    assert(parser.counters().oversized_frames == 1);
    assert(fixes.size() == 1);
    assert(parser.counters().frames == 1);
}

// Classic framing bug: a stray sync byte before the real pair. "B5 B5 62" must still frame.
void a_repeated_sync_byte_still_frames()
{
    const auto good = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    std::vector<std::uint8_t> stream{gnss::kSyncChar1};
    stream.insert(stream.end(), good.begin(), good.end());

    gnss::UbxParser parser;
    const auto fixes = run(parser, stream, 1);
    assert(fixes.size() == 1);
}

// A receiver emitting NMEA we did not ask for looks exactly like this, and the count is how
// that becomes visible rather than mysterious.
void bytes_between_frames_are_counted()
{
    std::vector<std::uint8_t> stream{'$', 'G', 'N', 'G', 'G', 'A', ',', '1', '2', '3', '\r', '\n'};
    const auto good = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    stream.insert(stream.end(), good.begin(), good.end());

    gnss::UbxParser parser;
    const auto fixes = run(parser, stream, 1);
    assert(fixes.size() == 1);
    assert(parser.counters().discarded_bytes == 12);
}

// Garbage containing a false sync pair and a plausible length may cost the frame it swallows,
// but must not prevent the ones after it.
void a_false_sync_in_garbage_recovers()
{
    std::vector<std::uint8_t> stream{gnss::kSyncChar1, gnss::kSyncChar2, 0x01, 0x07,
                                     0x5C, 0x00};  // declares 92 bytes of nonsense
    for (int index = 0; index < 92; ++index) {
        stream.push_back(static_cast<std::uint8_t>(index));
    }
    stream.push_back(0x00);  // wrong checksum
    stream.push_back(0x00);

    const auto good = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    stream.insert(stream.end(), good.begin(), good.end());

    gnss::UbxParser parser;
    const auto fixes = run(parser, stream, 1);
    assert(parser.counters().checksum_errors >= 1);
    assert(fixes.size() == 1);
}

// A frame cut short, as a transport interrupted mid-message leaves it.
//
// This costs the frame immediately behind it, and that is a deliberate limit rather than an
// oversight. The truncated frame still declares 92 bytes, so the parser consumes the next
// frame's header as payload before its checksum fails. Recovering it would mean rescanning the
// buffered bytes for a sync pair and replaying them, which complicates the byte-at-a-time
// contract that makes this parser indifferent to how a transport chunks its reads - and that
// indifference is worth more than one frame at 25 Hz, where a frame is 40 ms.
//
// The cost is bounded at one frame and it is visible: #18's fix-sequence gap counter is what
// turns it from a silent loss into a reported one.
void a_truncated_frame_costs_one_frame_and_then_recovers()
{
    auto truncated = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    truncated.resize(truncated.size() / 2);

    auto stream = truncated;
    const auto good = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    for (int repeat = 0; repeat < 2; ++repeat) {
        stream.insert(stream.end(), good.begin(), good.end());
    }

    gnss::UbxParser parser;
    const auto fixes = run(parser, stream, 1);
    // One of the two swallowed, the other recovered. What matters is that recovery happens at
    // all, which is the acceptance criterion: malformed data cannot stall what follows.
    assert(fixes.size() == 1);
    assert(parser.counters().frames == 1);
    assert(parser.counters().checksum_errors == 1);

    // And the parser is in a clean state afterwards, not wedged.
    const auto after = run(parser, good, 1);
    assert(after.size() == 1);
    assert(parser.counters().frames == 2);
}

// Zero-length frames are legal UBX and must not be mistaken for a parser fault.
void a_zero_length_frame_is_accepted()
{
    gnss::UbxParser parser;
    const auto stream = frame(0x05, 0x01, {});  // ACK-shaped, empty payload
    const auto fixes = run(parser, stream, 1);
    assert(fixes.empty());  // not a NAV-PVT
    assert(parser.counters().frames == 1);
    assert(parser.counters().checksum_errors == 0);
}

// Decoding judges the message, not the fix: wrong class, id or length is refused outright.
void decoding_refuses_anything_that_is_not_a_nav_pvt()
{
    gnss::UbxParser parser;
    domain::GnssFix fix{};

    const auto wrong_id = frame(gnss::kClassNav, 0x35, nav_pvt_payload());
    (void)run(parser, wrong_id, 1);
    assert(!gnss::decode_nav_pvt(parser.message(), fix));

    std::vector<std::uint8_t> short_payload(40, 0);
    const auto wrong_length = frame(gnss::kClassNav, gnss::kIdNavPvt, short_payload);
    (void)run(parser, wrong_length, 1);
    assert(!gnss::decode_nav_pvt(parser.message(), fix));

    const gnss::UbxMessage empty{};
    assert(!gnss::decode_nav_pvt(empty, fix));
}

// An unexpected fix type must not be cast blindly into the enum.
void an_unknown_fix_type_reads_as_no_fix()
{
    auto payload = nav_pvt_payload();
    payload[20] = 99;

    gnss::UbxParser parser;
    const auto stream = frame(gnss::kClassNav, gnss::kIdNavPvt, payload);
    const auto fixes = run(parser, stream, 1);
    assert(fixes.size() == 1);
    assert(fixes.front().fix_type == domain::FixType::no_fix);
}

// A long run must not grow anything: the parser's storage is fixed and its counters saturate.
void sustained_traffic_is_bounded()
{
    gnss::UbxParser parser;
    const auto good = frame(gnss::kClassNav, gnss::kIdNavPvt, nav_pvt_payload());
    for (int repeat = 0; repeat < 500; ++repeat) {
        (void)run(parser, good, 1);
    }
    assert(parser.counters().frames == 500);
    assert(parser.counters().checksum_errors == 0);
    static_assert(sizeof(gnss::UbxParser) <= 256, "parser storage must stay bounded");
}

}  // namespace

int main()
{
    a_valid_fix_is_framed_and_decoded();
    chunking_cannot_change_the_result();
    a_corrupt_frame_does_not_stall_the_next_one();
    an_oversized_length_is_refused_and_recovery_continues();
    a_repeated_sync_byte_still_frames();
    bytes_between_frames_are_counted();
    a_false_sync_in_garbage_recovers();
    a_truncated_frame_costs_one_frame_and_then_recovers();
    a_zero_length_frame_is_accepted();
    decoding_refuses_anything_that_is_not_a_nav_pvt();
    an_unknown_fix_type_reads_as_no_fix();
    sustained_traffic_is_bounded();

    std::cout << "UBX parser: framing, checksums, resynchronisation, bounded storage, "
                 "chunk independence and NAV-PVT decoding passed\n";
    return 0;
}

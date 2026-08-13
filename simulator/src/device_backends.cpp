#include "track_timer/simulator/device_backends.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace track_timer::simulator {
namespace {

constexpr std::string_view kFixtureMarker{"# track-session-timer-gnss-fixture-v1"};
constexpr std::string_view kFixtureNamePrefix{"# name="};
constexpr std::string_view kFixtureHeader{
    "measurement_time_ns,latitude_deg,longitude_deg,height_m,speed_mps,heading_deg,"
    "horizontal_accuracy_m,num_satellites"};
constexpr std::int64_t kImuPeriodUs = 10'000;
constexpr std::int64_t kReadyWriteLatencyUs = 1'000;
constexpr std::int64_t kSlowWriteLatencyUs = 250'000;
constexpr std::uint64_t kStorageCapacityBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;

domain::GnssFix make_fix(const std::uint32_t sequence, const std::int64_t measurement_time_ns,
                         const double latitude_deg, const double longitude_deg,
                         const float heading_deg) noexcept
{
    domain::GnssFix fix{};
    fix.measurement_time_ns = measurement_time_ns;
    fix.arrival_monotonic_us = measurement_time_ns / 1'000;
    fix.latitude_deg = latitude_deg;
    fix.longitude_deg = longitude_deg;
    fix.height_m = 118.0F;
    fix.speed_mps = 31.0F;
    fix.heading_deg = heading_deg;
    fix.horizontal_accuracy_m = 0.45F;
    fix.speed_accuracy_mps = 0.08F;
    fix.heading_accuracy_deg = 0.5F;
    fix.sequence_number = sequence;
    fix.valid_flags = 1;
    fix.num_satellites = 18;
    fix.fix_type = domain::FixType::fix_3d;
    fix.reject_reason = domain::FixRejectReason::none;
    fix.accepted_for_timing = true;
    return fix;
}

void strip_carriage_return(std::string& line)
{
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

std::vector<std::string> split_csv(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream stream{line};
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }
    return fields;
}

bool parse_int64(const std::string& value, std::int64_t& parsed) noexcept
{
    try {
        std::size_t consumed = 0;
        parsed = std::stoll(value, &consumed, 10);
        return consumed == value.size();
    }
    catch (...) {
        return false;
    }
}

bool parse_uint16(const std::string& value, std::uint16_t& parsed) noexcept
{
    try {
        std::size_t consumed = 0;
        const auto candidate = std::stoul(value, &consumed, 10);
        if (consumed != value.size() || candidate > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }
        parsed = static_cast<std::uint16_t>(candidate);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool parse_double(const std::string& value, double& parsed) noexcept
{
    try {
        std::size_t consumed = 0;
        parsed = std::stod(value, &consumed);
        return consumed == value.size() && std::isfinite(parsed);
    }
    catch (...) {
        return false;
    }
}

bool is_leap_year(const std::int32_t year) noexcept
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

std::uint8_t days_in_month(const std::int32_t year, const std::uint8_t month) noexcept
{
    constexpr std::array<std::uint8_t, 12> kDays{31, 28, 31, 30, 31, 30,
                                                 31, 31, 30, 31, 30, 31};
    if (month == 0 || month > kDays.size()) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return kDays[month - 1];
}

bool valid_rtc(const board::RtcDateTime& value) noexcept
{
    return value.valid && value.year >= 1970 && value.month >= 1 && value.month <= 12 &&
           value.day >= 1 && value.day <= days_in_month(value.year, value.month) &&
           value.hour <= 23 && value.minute <= 59 && value.second <= 59;
}

std::int64_t replay_period_us(const GnssReplayRate rate) noexcept
{
    return 1'000'000 / static_cast<std::uint8_t>(rate);
}

}  // namespace

GnssFixture make_synthetic_gnss_fixture()
{
    GnssFixture fixture{};
    fixture.name = "synthetic-loop-v1";
    constexpr std::int64_t kPeriodNs = 40'000'000;
    fixture.fixes.reserve(8);
    fixture.fixes.push_back(make_fix(0, 0 * kPeriodNs, 52.00000, -1.00030, 90.0F));
    fixture.fixes.push_back(make_fix(1, 1 * kPeriodNs, 52.00001, -1.00022, 90.0F));
    fixture.fixes.push_back(make_fix(2, 2 * kPeriodNs, 52.00002, -1.00014, 90.0F));
    fixture.fixes.push_back(make_fix(3, 3 * kPeriodNs, 52.00003, -1.00006, 90.0F));
    fixture.fixes.push_back(make_fix(4, 4 * kPeriodNs, 52.00004, -0.99998, 90.0F));
    fixture.fixes.push_back(make_fix(5, 5 * kPeriodNs, 52.00003, -0.99990, 100.0F));
    fixture.fixes.push_back(make_fix(6, 6 * kPeriodNs, 52.00001, -0.99984, 115.0F));
    fixture.fixes.push_back(make_fix(7, 7 * kPeriodNs, 51.99998, -0.99980, 135.0F));
    return fixture;
}

bool load_gnss_fixture(const std::string& path, GnssFixture& fixture, std::string& error)
{
    std::ifstream input{path};
    if (!input) {
        error = "unable to open GNSS fixture: " + path;
        return false;
    }

    std::string marker;
    std::string name_line;
    std::string header;
    if (!std::getline(input, marker) || !std::getline(input, name_line) ||
        !std::getline(input, header)) {
        error = "GNSS fixture must contain a version marker, name, and header";
        return false;
    }
    strip_carriage_return(marker);
    strip_carriage_return(name_line);
    strip_carriage_return(header);
    if (marker != kFixtureMarker) {
        error = "unsupported GNSS fixture version marker";
        return false;
    }
    if (name_line.rfind(kFixtureNamePrefix.data(), 0) != 0 ||
        name_line.size() == kFixtureNamePrefix.size()) {
        error = "GNSS fixture name is missing";
        return false;
    }
    if (header != kFixtureHeader) {
        error = "GNSS fixture header does not match version 1";
        return false;
    }

    GnssFixture candidate{};
    candidate.name = name_line.substr(kFixtureNamePrefix.size());
    std::string line;
    std::int64_t previous_time_ns = domain::kUnavailableTime;
    std::size_t line_number = 3;
    while (std::getline(input, line)) {
        ++line_number;
        strip_carriage_return(line);
        if (line.empty()) {
            continue;
        }
        const auto fields = split_csv(line);
        if (fields.size() != 8) {
            error = "GNSS fixture line " + std::to_string(line_number) + " must have 8 fields";
            return false;
        }

        std::int64_t measurement_time_ns = 0;
        double latitude_deg = 0.0;
        double longitude_deg = 0.0;
        double height_m = 0.0;
        double speed_mps = 0.0;
        double heading_deg = 0.0;
        double horizontal_accuracy_m = 0.0;
        std::uint16_t satellites = 0;
        if (!parse_int64(fields[0], measurement_time_ns) ||
            !parse_double(fields[1], latitude_deg) || !parse_double(fields[2], longitude_deg) ||
            !parse_double(fields[3], height_m) || !parse_double(fields[4], speed_mps) ||
            !parse_double(fields[5], heading_deg) ||
            !parse_double(fields[6], horizontal_accuracy_m) ||
            !parse_uint16(fields[7], satellites)) {
            error = "GNSS fixture line " + std::to_string(line_number) +
                    " contains an invalid number";
            return false;
        }
        if ((previous_time_ns != domain::kUnavailableTime &&
             measurement_time_ns <= previous_time_ns) ||
            latitude_deg < -90.0 || latitude_deg > 90.0 || longitude_deg < -180.0 ||
            longitude_deg > 180.0 || speed_mps < 0.0 || heading_deg < 0.0 ||
            heading_deg >= 360.0 || horizontal_accuracy_m < 0.0 || satellites > 64) {
            error = "GNSS fixture line " + std::to_string(line_number) +
                    " is outside the version 1 constraints";
            return false;
        }

        auto fix = make_fix(static_cast<std::uint32_t>(candidate.fixes.size()),
                            measurement_time_ns, latitude_deg, longitude_deg,
                            static_cast<float>(heading_deg));
        fix.height_m = static_cast<float>(height_m);
        fix.speed_mps = static_cast<float>(speed_mps);
        fix.horizontal_accuracy_m = static_cast<float>(horizontal_accuracy_m);
        fix.num_satellites = satellites;
        candidate.fixes.push_back(fix);
        previous_time_ns = measurement_time_ns;
    }

    if (candidate.fixes.size() < 2) {
        error = "GNSS fixture must contain at least two fixes";
        return false;
    }
    fixture = std::move(candidate);
    error.clear();
    return true;
}

const char* gnss_mode_name(const GnssMode mode) noexcept
{
    switch (mode) {
    case GnssMode::normal:
        return "normal";
    case GnssMode::loss:
        return "loss";
    case GnssMode::stale:
        return "stale";
    case GnssMode::corrupt:
        return "corrupt";
    }
    return "loss";
}

std::int64_t SimulatedClock::now_us() const noexcept
{
    return now_us_;
}

void SimulatedClock::advance_us(const std::int64_t elapsed_us) noexcept
{
    if (elapsed_us <= 0) {
        return;
    }
    if (elapsed_us > std::numeric_limits<std::int64_t>::max() - now_us_) {
        now_us_ = std::numeric_limits<std::int64_t>::max();
        return;
    }
    now_us_ += elapsed_us;
}

void SimulatedClock::reset() noexcept
{
    now_us_ = 0;
}

SimulatedGnss::SimulatedGnss(SimulatedClock& clock, GnssFixture fixture,
                             const GnssReplayRate rate)
    : clock_(clock), fixture_(std::move(fixture)), rate_(rate)
{
    if (fixture_.fixes.empty()) {
        fixture_ = make_synthetic_gnss_fixture();
    }
    reset();
}

void SimulatedGnss::advance() noexcept
{
    const auto period_us = replay_period_us(rate_);
    while (next_emit_us_ <= clock_.now_us()) {
        const auto sequence = scheduled_++;
        if (mode_ == GnssMode::loss) {
            ++lost_;
            next_emit_us_ += period_us;
            continue;
        }

        auto fix = fixture_.fixes[sequence % fixture_.fixes.size()];
        fix.sequence_number = static_cast<std::uint32_t>(sequence);
        fix.arrival_monotonic_us = next_emit_us_;
        const auto base_time_ns = fixture_.fixes.front().measurement_time_ns;
        fix.measurement_time_ns = base_time_ns + static_cast<std::int64_t>(sequence) * period_us * 1'000;
        if (mode_ == GnssMode::stale) {
            if (last_measurement_time_ns_ == domain::kUnavailableTime) {
                last_measurement_time_ns_ = fix.measurement_time_ns;
            }
            fix.measurement_time_ns = last_measurement_time_ns_;
            fix.reject_reason = domain::FixRejectReason::stale;
            fix.accepted_for_timing = false;
            ++stale_;
        }
        else if (mode_ == GnssMode::corrupt) {
            fix.latitude_deg = 999.0;
            fix.valid_flags = 0;
            fix.reject_reason = domain::FixRejectReason::invalid_status;
            fix.accepted_for_timing = false;
            ++corrupt_;
        }
        else {
            last_measurement_time_ns_ = fix.measurement_time_ns;
        }
        if (queue_.push(fix)) {
            ++emitted_;
        }
        next_emit_us_ += period_us;
    }
}

void SimulatedGnss::reset() noexcept
{
    mode_ = GnssMode::normal;
    queue_.reset();
    next_emit_us_ = clock_.now_us() + replay_period_us(rate_);
    last_measurement_time_ns_ = domain::kUnavailableTime;
    scheduled_ = 0;
    emitted_ = 0;
    lost_ = 0;
    stale_ = 0;
    corrupt_ = 0;
    recoveries_ = 0;
}

void SimulatedGnss::set_mode(const GnssMode mode) noexcept
{
    if (mode_ != GnssMode::normal && mode == GnssMode::normal) {
        ++recoveries_;
    }
    mode_ = mode;
}

bool SimulatedGnss::try_read(domain::GnssFix& fix) noexcept
{
    return queue_.pop(fix);
}

GnssMode SimulatedGnss::mode() const noexcept
{
    return mode_;
}

GnssReplayRate SimulatedGnss::rate() const noexcept
{
    return rate_;
}

const std::string& SimulatedGnss::fixture_name() const noexcept
{
    return fixture_.name;
}

GnssDiagnostics SimulatedGnss::diagnostics() const noexcept
{
    return GnssDiagnostics{scheduled_, emitted_, lost_, stale_, corrupt_, recoveries_,
                           queue_.metrics()};
}

SimulatedTouch::SimulatedTouch(SimulatedClock& clock) noexcept : clock_(clock) {}

bool SimulatedTouch::inject(const std::int16_t x, const std::int16_t y,
                            const bool pressed) noexcept
{
    return queue_.push(board::TouchSample{clock_.now_us(), x, y, pressed});
}

bool SimulatedTouch::try_read(board::TouchSample& sample) noexcept
{
    return queue_.pop(sample);
}

void SimulatedTouch::reset() noexcept
{
    queue_.reset();
}

QueueMetrics SimulatedTouch::metrics() const noexcept
{
    return queue_.metrics();
}

SimulatedImu::SimulatedImu(SimulatedClock& clock) noexcept : clock_(clock)
{
    reset();
}

void SimulatedImu::advance() noexcept
{
    while (next_emit_us_ <= clock_.now_us()) {
        if (available_) {
            board::ImuSample sample{};
            sample.monotonic_us = next_emit_us_;
            const auto phase = static_cast<float>(emitted_ % 20) / 20.0F;
            sample.acceleration_x_mps2 = phase * 0.6F;
            sample.acceleration_y_mps2 = (0.5F - phase) * 0.4F;
            sample.acceleration_z_mps2 = 9.80665F;
            sample.angular_rate_z_dps = 8.0F + phase * 4.0F;
            sample.valid = true;
            queue_.push(sample);
            ++emitted_;
        }
        next_emit_us_ += kImuPeriodUs;
    }
}

void SimulatedImu::reset() noexcept
{
    queue_.reset();
    next_emit_us_ = clock_.now_us() + kImuPeriodUs;
    emitted_ = 0;
    available_ = true;
}

void SimulatedImu::set_available(const bool available) noexcept
{
    available_ = available;
}

bool SimulatedImu::try_read(board::ImuSample& sample) noexcept
{
    return queue_.pop(sample);
}

QueueMetrics SimulatedImu::metrics() const noexcept
{
    return queue_.metrics();
}

std::uint64_t SimulatedImu::emitted() const noexcept
{
    return emitted_;
}

SimulatedRtc::SimulatedRtc(SimulatedClock& clock, const board::RtcDateTime epoch) noexcept
    : clock_(clock), epoch_(epoch)
{
}

board::RtcDateTime SimulatedRtc::now() const noexcept
{
    if (!valid_rtc(epoch_)) {
        return {};
    }

    auto value = epoch_;
    auto additional_seconds = clock_.now_us() / 1'000'000;
    const auto seconds_of_day = static_cast<std::int64_t>(value.hour) * 3'600 +
                                static_cast<std::int64_t>(value.minute) * 60 + value.second +
                                additional_seconds;
    auto additional_days = seconds_of_day / 86'400;
    auto remaining_seconds = seconds_of_day % 86'400;
    value.hour = static_cast<std::uint8_t>(remaining_seconds / 3'600);
    remaining_seconds %= 3'600;
    value.minute = static_cast<std::uint8_t>(remaining_seconds / 60);
    value.second = static_cast<std::uint8_t>(remaining_seconds % 60);

    while (additional_days > 0) {
        const auto remaining_in_month =
            static_cast<std::int64_t>(days_in_month(value.year, value.month)) - value.day;
        if (additional_days <= remaining_in_month) {
            value.day = static_cast<std::uint8_t>(value.day + additional_days);
            break;
        }
        additional_days -= remaining_in_month + 1;
        value.day = 1;
        if (value.month == 12) {
            value.month = 1;
            ++value.year;
        }
        else {
            ++value.month;
        }
    }
    return value;
}

void SimulatedRtc::set_epoch(const board::RtcDateTime epoch) noexcept
{
    epoch_ = epoch;
}

const char* storage_mode_name(const StorageMode mode) noexcept
{
    switch (mode) {
    case StorageMode::ready:
        return "ready";
    case StorageMode::missing:
        return "missing";
    case StorageMode::full:
        return "full";
    case StorageMode::slow:
        return "slow";
    case StorageMode::write_failed:
        return "write-failed";
    }
    return "missing";
}

bool SimulatedStorage::append(const domain::LogRecord& record) noexcept
{
    if (mode_ != StorageMode::ready && mode_ != StorageMode::slow) {
        ++write_failures_;
        return false;
    }
    if (!queue_.push(record)) {
        ++write_failures_;
        return false;
    }
    ++accepted_records_;
    return true;
}

board::StorageStatus SimulatedStorage::status() const noexcept
{
    board::StorageHealth health = board::StorageHealth::unavailable;
    switch (mode_) {
    case StorageMode::ready:
        health = board::StorageHealth::ready;
        break;
    case StorageMode::missing:
        health = board::StorageHealth::unavailable;
        break;
    case StorageMode::full:
        health = board::StorageHealth::full;
        break;
    case StorageMode::slow:
        health = board::StorageHealth::degraded;
        break;
    case StorageMode::write_failed:
        health = board::StorageHealth::write_failed;
        break;
    }
    const auto available = mode_ == StorageMode::full || mode_ == StorageMode::missing
                               ? 0
                               : kStorageCapacityBytes -
                                     std::min(kStorageCapacityBytes, written_bytes_);
    return board::StorageStatus{available, write_failures_, health};
}

void SimulatedStorage::advance(const std::int64_t elapsed_us) noexcept
{
    if (elapsed_us <= 0 || (mode_ != StorageMode::ready && mode_ != StorageMode::slow)) {
        return;
    }
    const auto latency_us = write_latency_us();
    write_budget_us_ += elapsed_us;
    domain::LogRecord record{};
    while (write_budget_us_ >= latency_us) {
        if (!queue_.pop(record)) {
            write_budget_us_ = std::min(write_budget_us_, latency_us);
            break;
        }
        write_budget_us_ -= latency_us;
        ++written_records_;
        written_bytes_ += record.payload_size;
    }
}

void SimulatedStorage::reset() noexcept
{
    queue_.reset();
    mode_ = StorageMode::ready;
    write_budget_us_ = 0;
    accepted_records_ = 0;
    written_records_ = 0;
    written_bytes_ = 0;
    write_failures_ = 0;
    recoveries_ = 0;
}

void SimulatedStorage::set_mode(const StorageMode mode) noexcept
{
    if (mode_ != StorageMode::ready && mode == StorageMode::ready) {
        ++recoveries_;
    }
    mode_ = mode;
}

StorageMode SimulatedStorage::mode() const noexcept
{
    return mode_;
}

StorageDiagnostics SimulatedStorage::diagnostics() const noexcept
{
    return StorageDiagnostics{accepted_records_, written_records_, written_bytes_, recoveries_,
                              write_latency_us(), queue_.metrics()};
}

std::int64_t SimulatedStorage::write_latency_us() const noexcept
{
    return mode_ == StorageMode::slow ? kSlowWriteLatencyUs : kReadyWriteLatencyUs;
}

SimulatedDevice::SimulatedDevice(GnssFixture fixture, const GnssReplayRate rate)
    : gnss_(clock_, std::move(fixture), rate), touch_(clock_), imu_(clock_),
      rtc_(clock_, board::RtcDateTime{2026, 8, 13, 9, 30, 0, true})
{
}

void SimulatedDevice::advance(const std::int64_t elapsed_us) noexcept
{
    clock_.advance_us(elapsed_us);
    gnss_.advance();
    imu_.advance();
    storage_.advance(elapsed_us);
}

void SimulatedDevice::reset() noexcept
{
    clock_.reset();
    gnss_.reset();
    touch_.reset();
    imu_.reset();
    storage_.reset();
}

DeviceDiagnostics SimulatedDevice::diagnostics() const noexcept
{
    return DeviceDiagnostics{gnss_.diagnostics(), touch_.metrics(), imu_.metrics(),
                             imu_.emitted(), storage_.diagnostics()};
}

SimulatedClock& SimulatedDevice::clock() noexcept
{
    return clock_;
}

SimulatedGnss& SimulatedDevice::gnss() noexcept
{
    return gnss_;
}

SimulatedTouch& SimulatedDevice::touch() noexcept
{
    return touch_;
}

SimulatedImu& SimulatedDevice::imu() noexcept
{
    return imu_;
}

SimulatedRtc& SimulatedDevice::rtc() noexcept
{
    return rtc_;
}

SimulatedStorage& SimulatedDevice::storage() noexcept
{
    return storage_;
}

}  // namespace track_timer::simulator

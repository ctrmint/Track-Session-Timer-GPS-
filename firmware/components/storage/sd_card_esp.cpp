#include "track_timer/storage/sd_card.hpp"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>

namespace track_timer::storage {
namespace {

constexpr auto kSdHost = SPI3_HOST;  // SPI2_HOST carries the AMOLED panel
constexpr gpio_num_t kPinCs = GPIO_NUM_2;
constexpr gpio_num_t kPinSclk = GPIO_NUM_4;
constexpr gpio_num_t kPinMosi = GPIO_NUM_5;
constexpr gpio_num_t kPinMiso = GPIO_NUM_6;

constexpr int kMaxOpenFiles = 5;
constexpr std::size_t kAllocationUnitBytes = 16 * 1024;
constexpr int kSpiClockKhz = 10'000;

sdmmc_card_t* card = nullptr;
bool bus_ready = false;

}  // namespace

MountResult mount(const bool format_if_unreadable) noexcept
{
    if (card != nullptr) {
        return MountResult::already_mounted;
    }

    if (!bus_ready) {
        spi_bus_config_t bus_config{};
        bus_config.mosi_io_num = kPinMosi;
        bus_config.miso_io_num = kPinMiso;
        bus_config.sclk_io_num = kPinSclk;
        bus_config.quadwp_io_num = -1;
        bus_config.quadhd_io_num = -1;
        bus_config.max_transfer_sz = 4000;
        if (spi_bus_initialize(kSdHost, &bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
            return MountResult::bus_failed;
        }
        bus_ready = true;
    }

// The ESP-IDF defaults are C compound literals; they do not initialise every field and
// so trip -Werror when compiled as C++. Hand-filling sdmmc_host_t is worse: it carries
// driver callbacks that would silently rot across IDF versions.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
#pragma GCC diagnostic pop

    host.slot = kSdHost;
    // 20 MHz is the SDSPI default. The card proved marginal at that rate on this
    // board, so probe conservatively; logging throughput is nowhere near this bound.
    host.max_freq_khz = kSpiClockKhz;
    slot_config.gpio_cs = kPinCs;
    slot_config.host_id = kSdHost;

    mount_config.format_if_mount_failed = format_if_unreadable;
    mount_config.max_files = kMaxOpenFiles;
    mount_config.allocation_unit_size = kAllocationUnitBytes;

    const auto result =
        esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slot_config, &mount_config, &card);
    if (result == ESP_OK) {
        return MountResult::mounted;
    }
    card = nullptr;
    if (result == ESP_ERR_TIMEOUT || result == ESP_ERR_NOT_FOUND ||
        result == ESP_ERR_INVALID_RESPONSE) {
        return MountResult::no_card;
    }
    if (result == ESP_FAIL) {
        return MountResult::unreadable_filesystem;
    }
    return MountResult::mount_failed;
}

void unmount() noexcept
{
    if (card != nullptr) {
        (void)esp_vfs_fat_sdcard_unmount(kMountPoint, card);
        card = nullptr;
    }
}

bool mounted() noexcept { return card != nullptr; }

CardInfo info() noexcept
{
    CardInfo result{};
    if (card == nullptr) {
        return result;
    }
    result.mounted = true;
    result.capacity_bytes =
        static_cast<std::uint64_t>(card->csd.capacity) * card->csd.sector_size;
    std::strncpy(result.name, card->cid.name, sizeof(result.name) - 1);

    // The FATFS VFS layer does not implement statvfs, so it silently reports zeroes;
    // esp_vfs_fat_info is the supported way to read usage.
    (void)esp_vfs_fat_info(kMountPoint, &result.total_bytes, &result.free_bytes);

    if (auto* directory = opendir(kMountPoint); directory != nullptr) {
        while (readdir(directory) != nullptr) {
            ++result.entry_count;
        }
        closedir(directory);
    }
    return result;
}

std::size_t list_entries(char names[][64], const std::size_t capacity) noexcept
{
    if (card == nullptr || names == nullptr || capacity == 0) {
        return 0;
    }
    auto* directory = opendir(kMountPoint);
    if (directory == nullptr) {
        return 0;
    }
    std::size_t written = 0;
    while (written < capacity) {
        const auto* entry = readdir(directory);
        if (entry == nullptr) {
            break;
        }
        std::strncpy(names[written], entry->d_name, 63);
        names[written][63] = '\0';
        ++written;
    }
    closedir(directory);
    return written;
}

namespace {

// Recurses, so the per-frame footprint is kept deliberately small: a 280-byte buffer at
// depth 6 overflowed the main task stack once the card held nested pack directories.
constexpr int kMaxWalkDepth = 3;
constexpr std::uint32_t kMaxWalkEntries = 20'000;

void walk(const char* path, int depth, MediaSurvey& survey) noexcept
{
    if (depth > kMaxWalkDepth || survey.file_count >= kMaxWalkEntries) {
        return;
    }
    auto* directory = opendir(path);
    if (directory == nullptr) {
        return;
    }
    while (const auto* entry = readdir(directory)) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        char child[160]{};
        std::snprintf(child, sizeof(child), "%.100s/%.50s", path, entry->d_name);
        struct stat details {};
        if (stat(child, &details) != 0) {
            continue;
        }
        if (S_ISDIR(details.st_mode)) {
            walk(child, depth + 1, survey);
            continue;
        }
        ++survey.file_count;
        survey.total_bytes += static_cast<std::uint64_t>(details.st_size);
        if (survey.sample_count < 5) {
            std::snprintf(survey.samples[survey.sample_count],
                          sizeof(survey.samples[0]), "%.50s (%llu KB)", child,
                          static_cast<unsigned long long>(details.st_size / 1024));
            ++survey.sample_count;
        }
    }
    closedir(directory);
}

}  // namespace

bool survey_media(MediaSurvey& survey) noexcept
{
    survey = {};
    if (card == nullptr) {
        return false;
    }
    walk(kMountPoint, 0, survey);
    return true;
}

FormatResult format() noexcept
{
    if (card == nullptr) {
        return FormatResult::not_mounted;
    }
    return esp_vfs_fat_sdcard_format(kMountPoint, card) == ESP_OK
               ? FormatResult::formatted
               : FormatResult::format_failed;
}

const char* mount_result_name(const MountResult result) noexcept
{
    switch (result) {
    case MountResult::mounted:
        return "mounted";
    case MountResult::already_mounted:
        return "already-mounted";
    case MountResult::bus_failed:
        return "bus-failed";
    case MountResult::no_card:
        return "no-card";
    case MountResult::unreadable_filesystem:
        return "unreadable-filesystem";
    case MountResult::mount_failed:
        return "mount-failed";
    }
    return "unknown";
}

const char* format_result_name(const FormatResult result) noexcept
{
    switch (result) {
    case FormatResult::formatted:
        return "formatted";
    case FormatResult::not_mounted:
        return "not-mounted";
    case FormatResult::format_failed:
        return "format-failed";
    }
    return "unknown";
}

}  // namespace track_timer::storage

#pragma once

#include <cstddef>
#include <cstdint>

// microSD support for the Waveshare ESP32-S3-Touch-AMOLED-2.41-B.
//
// The slot is wired to GPIO2/4/5/6 and is driven in SPI mode on SPI3_HOST, because
// SPI2_HOST already carries the AMOLED panel.

namespace track_timer::storage {

inline constexpr const char* kMountPoint = "/sdcard";
inline constexpr std::size_t kCardLabelCapacity = 16;

enum class MountResult : std::uint8_t {
    mounted,
    already_mounted,
    bus_failed,
    no_card,
    unreadable_filesystem,
    mount_failed,
};

enum class FormatResult : std::uint8_t {
    formatted,
    not_mounted,
    format_failed,
};

struct CardInfo {
    std::uint64_t capacity_bytes{0};
    std::uint64_t total_bytes{0};
    std::uint64_t free_bytes{0};
    std::uint32_t entry_count{0};
    char name[kCardLabelCapacity]{};
    bool mounted{false};
};

// format_if_unreadable is deliberately explicit and defaults to false: mounting must
// never be able to destroy a card's contents as a side effect.
[[nodiscard]] MountResult mount(bool format_if_unreadable = false) noexcept;
void unmount() noexcept;
[[nodiscard]] bool mounted() noexcept;

[[nodiscard]] CardInfo info() noexcept;

// Writes the first `capacity` top-level entry names into `names`, returning how many
// were written. Used to show what is on a card before it is overwritten.
[[nodiscard]] std::size_t list_entries(char names[][64], std::size_t capacity) noexcept;

struct MediaSurvey {
    std::uint32_t file_count{0};
    std::uint64_t total_bytes{0};
    std::size_t sample_count{0};
    char samples[5][80]{};
};

// Walks /DCIM so a card's existing camera footage can be quantified before anything
// is overwritten. Returns false when there is no /DCIM directory.
[[nodiscard]] bool survey_media(MediaSurvey& survey) noexcept;

// Destructive. Erases the card and lays down a fresh FAT filesystem.
[[nodiscard]] FormatResult format() noexcept;

[[nodiscard]] const char* mount_result_name(MountResult result) noexcept;
[[nodiscard]] const char* format_result_name(FormatResult result) noexcept;

}  // namespace track_timer::storage

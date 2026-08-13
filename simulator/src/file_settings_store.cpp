#include "track_timer/simulator/file_settings_store.hpp"

#include <fstream>
#include <system_error>
#include <utility>

namespace track_timer::simulator {
namespace {

bool read_file(const std::filesystem::path& path, settings::SettingsBlob& blob) noexcept
{
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) {
        return false;
    }
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) > blob.bytes.size()) {
        return false;
    }
    blob.size = static_cast<std::size_t>(end);
    input.seekg(0);
    if (blob.size > 0) {
        input.read(reinterpret_cast<char*>(blob.bytes.data()),
                   static_cast<std::streamsize>(blob.size));
    }
    return input.good() || input.eof();
}

}  // namespace

FileSettingsStore::FileSettingsStore(std::filesystem::path path) : path_(std::move(path)) {}

settings::StoreReadResult FileSettingsStore::read(settings::SettingsBlob& blob) noexcept
{
    std::error_code error;
    if (std::filesystem::exists(path_, error)) {
        return !error && read_file(path_, blob) ? settings::StoreReadResult::found
                                                : settings::StoreReadResult::error;
    }
    if (error) {
        return settings::StoreReadResult::error;
    }

    const auto backup_path = path_.string() + ".bak";
    if (!std::filesystem::exists(backup_path, error)) {
        return error ? settings::StoreReadResult::error : settings::StoreReadResult::missing;
    }
    if (error || !read_file(backup_path, blob)) {
        return settings::StoreReadResult::error;
    }
    std::filesystem::rename(backup_path, path_, error);
    return settings::StoreReadResult::found;
}

bool FileSettingsStore::write_atomic(const settings::SettingsBlob& blob) noexcept
{
    if (blob.size == 0 || blob.size > blob.bytes.size()) {
        return false;
    }

    std::error_code error;
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path(), error);
        if (error) {
            return false;
        }
    }

    const std::filesystem::path temporary_path{path_.string() + ".tmp"};
    const std::filesystem::path backup_path{path_.string() + ".bak"};
    std::filesystem::remove(temporary_path, error);
    error.clear();
    {
        std::ofstream output{temporary_path, std::ios::binary | std::ios::trunc};
        if (!output) {
            return false;
        }
        output.write(reinterpret_cast<const char*>(blob.bytes.data()),
                     static_cast<std::streamsize>(blob.size));
        output.flush();
        if (!output) {
            std::filesystem::remove(temporary_path, error);
            return false;
        }
    }

    const bool had_existing = std::filesystem::exists(path_, error) && !error;
    if (had_existing) {
        std::filesystem::remove(backup_path, error);
        error.clear();
        std::filesystem::rename(path_, backup_path, error);
        if (error) {
            std::filesystem::remove(temporary_path, error);
            return false;
        }
    }

    std::filesystem::rename(temporary_path, path_, error);
    if (error) {
        if (had_existing) {
            std::error_code restore_error;
            std::filesystem::rename(backup_path, path_, restore_error);
        }
        std::filesystem::remove(temporary_path, error);
        return false;
    }
    if (had_existing) {
        std::filesystem::remove(backup_path, error);
    }
    return true;
}

const std::filesystem::path& FileSettingsStore::path() const noexcept
{
    return path_;
}

}  // namespace track_timer::simulator

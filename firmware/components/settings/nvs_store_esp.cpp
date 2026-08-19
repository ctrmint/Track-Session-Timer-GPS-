#include "track_timer/settings/nvs_store.hpp"

#include "nvs.h"
#include "nvs_flash.h"

#include <cstring>

namespace track_timer::settings {

bool NvsSettingsStore::begin() noexcept
{
    auto result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // A full or older-format partition is recoverable. Erasing loses stored settings,
        // which fall back to defaults, and that is far better than refusing to boot.
        if (nvs_flash_erase() != ESP_OK) {
            return false;
        }
        result = nvs_flash_init();
    }
    ready_ = result == ESP_OK;
    return ready_;
}

StoreReadResult NvsSettingsStore::read(SettingsBlob& blob) noexcept
{
    blob = {};
    if (!ready_) {
        return StoreReadResult::error;
    }

    nvs_handle_t handle{};
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        // A namespace that has never been written is a first boot, not a fault.
        return StoreReadResult::missing;
    }

    std::size_t length = 0;
    auto result = nvs_get_blob(handle, kNvsSettingsKey, nullptr, &length);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return StoreReadResult::missing;
    }
    if (result != ESP_OK || length == 0 || length > blob.bytes.size()) {
        nvs_close(handle);
        return StoreReadResult::error;
    }

    result = nvs_get_blob(handle, kNvsSettingsKey, blob.bytes.data(), &length);
    nvs_close(handle);
    if (result != ESP_OK) {
        blob = {};
        return StoreReadResult::error;
    }
    blob.size = length;
    return StoreReadResult::found;
}

bool NvsSettingsStore::write_atomic(const SettingsBlob& blob) noexcept
{
    if (!ready_ || blob.size == 0 || blob.size > blob.bytes.size()) {
        return false;
    }

    nvs_handle_t handle{};
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return false;
    }
    // NVS commits the whole blob or none of it, so no partially written settings record
    // can ever be read back.
    const auto written = nvs_set_blob(handle, kNvsSettingsKey, blob.bytes.data(),
                                      blob.size) == ESP_OK;
    const auto committed = written && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return committed;
}

bool NvsSettingsStore::ready() const noexcept { return ready_; }

}  // namespace track_timer::settings

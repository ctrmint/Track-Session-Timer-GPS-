#pragma once

#include "track_timer/track/definition.hpp"

#include <cstdint>
#include <string_view>

namespace track_timer::track {

enum class TrackStoreReadResult : std::uint8_t {
    loaded,
    not_found,
    invalid,
    io_error,
};

class TrackDefinitionStore {
  public:
    virtual ~TrackDefinitionStore() = default;
    [[nodiscard]] virtual TrackStoreReadResult read(std::string_view track_id,
                                                    TrackDefinitionBlob& blob) noexcept = 0;
    [[nodiscard]] virtual bool exists(std::string_view track_id) noexcept = 0;
    [[nodiscard]] virtual bool write_atomic(std::string_view track_id,
                                            const TrackDefinitionBlob& blob) noexcept = 0;
};

}  // namespace track_timer::track

#pragma once

#include "track_timer/logger/async_logger.hpp"

#include <cstdint>

namespace track_timer::logger {

struct LoggerTaskConfiguration {
    std::uint32_t stack_size_bytes{4'096};
    std::uint32_t service_period_ms{10};
    std::uint8_t priority{5};
    std::int8_t core{-1};
};

// Caller-owned for the lifetime of the firmware task. Producers retain only AsyncLogger
// and never receive this context or the StorageBackend.
struct LoggerTaskContext {
    AsyncLogger* logger{nullptr};
    LoggerTaskConfiguration configuration{};
    void* native_handle{nullptr};
};

[[nodiscard]] bool start_logger_task(LoggerTaskContext& context,
                                     AsyncLogger& logger,
                                     LoggerTaskConfiguration configuration = {}) noexcept;

}  // namespace track_timer::logger

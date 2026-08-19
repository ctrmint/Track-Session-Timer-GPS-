#pragma once

#include "driver/i2c_master.h"

namespace track_timer::board {

// The Waveshare board puts the touch controller, the QMI8658 IMU, the PCF85063 RTC and
// the IO expander on one I2C bus (GPIO47/48). The bus is therefore a board resource, not
// something any single driver owns: the first caller creates it and the rest share it.
//
// Returns nullptr if the bus could not be created.
[[nodiscard]] i2c_master_bus_handle_t shared_i2c_bus() noexcept;

}  // namespace track_timer::board

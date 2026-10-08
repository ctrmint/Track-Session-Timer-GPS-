#pragma once

#include <cstddef>
#include <cstdint>

namespace track_timer::gnss {

// Where the receiver's bytes come from, and the only thing the parser depends on.
//
// The transport is staged: I2C for bring-up, because it is the only connection the hardware
// can make without soldering, and UART before the car, because it is pushed by DMA and
// survives the CPU being busy where a polled bus loses fixes silently. See ADR-005.
//
// The parser sits behind this seam so it is written and tested once, and so the move to UART
// is a transport swap rather than a rewrite.
class GnssTransport {
  public:
    virtual ~GnssTransport() = default;

    // Reads whatever is available, up to `capacity`, and returns how many bytes were written.
    // Zero is the ordinary case when nothing has arrived yet, not an error.
    //
    // The two transports fragment differently - a UART FIFO hands over whatever has arrived,
    // while a DDC read returns exactly the count the receiver declared - so no caller may
    // assume anything about where a chunk boundary falls relative to a frame.
    [[nodiscard]] virtual std::size_t read(std::uint8_t* output, std::size_t capacity) noexcept = 0;

    // False when the bus itself is failing, as distinct from the receiver having nothing to
    // say. A transport that cannot be read at all is a different fault from a receiver
    // without a fix, and the two must not be reported as one.
    [[nodiscard]] virtual bool healthy() const noexcept = 0;
};

}  // namespace track_timer::gnss

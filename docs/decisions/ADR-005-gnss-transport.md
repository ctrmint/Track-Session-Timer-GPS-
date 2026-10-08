# ADR-005: Bring the receiver up on I2C, drive the vehicle on UART

**Status:** Accepted for prototype, 2026-10-08

Supersedes the transport element of [ADR-003](ADR-003-gnss.md), which specified "a dedicated
UART" and referenced `GPS-15712`. The receiver is now `GPS-17285` (see
[HARDWARE_BOM.md](../HARDWARE_BOM.md)) and the transport is staged.

## Context

The NEO-M9N breakout offers the module's UBX stream over three interfaces: UART, I2C (u-blox
call it DDC) and native USB. The Waveshare board breaks out **UART** and **I2C** on two
physically identical SH1.0 4-pin sockets.

Only I2C can be connected without soldering. The breakout's UART exists solely as plated
through-holes, while both of its connectors are Qwiic, which is I2C; the two sockets exist for
daisy-chaining one bus, not for offering two.

Bandwidth does not distinguish the two. At 25 Hz, UBX-NAV-PVT is about 100 bytes per fix, so
**2,500 B/s against roughly 44,400 B/s on a 400 kHz bus - 5.6%**. Touch, IMU and RTC traffic is
negligible beside it.

Neither does latency affect accuracy. Section 11 of the design is explicit that crossing times
come from the receiver's own measurement timestamp, so a fix read late still reports where the
car was and when. At 140 mph a 10 ms read delay is 63 cm of travel and costs nothing.

What does distinguish them is **how the bytes arrive**.

- **UART is pushed.** The peripheral fills a hardware FIFO by DMA. The receiver keeps
  streaming while the CPU is occupied elsewhere.
- **I2C is polled.** Nothing arrives unless the CPU asks, at least 25 times a second, for the
  entire session.

A stalled poll overruns the receiver's buffer and **loses fixes silently**. Roughly 410 ms of
stall fills a 1 KB buffer. That margin reads as generous until set against this device's
history: the UI task has starved twice, once on the carousel's image transform and once on the
glyph cache, each time long enough to trip the task watchdog, which fires after seconds.

Those failures announced themselves with a reboot. A starved GNSS poll has no watchdog. It
leaves gaps in the trace, and the symptom is lap times that do not repeat.

The cost is measurable. At 140 mph consecutive fixes are 2.5 m apart; losing one widens the
interpolation span to **5 m over 80 ms**, which is precisely the quantisation the crossing-time
work exists to remove.

## Decision

**Stage the transport behind a seam, and ship the car on UART.**

1. A transport interface yields bytes. The UBX parser consumes bytes and knows nothing of the
   bus, so it is written and tested once.
2. **I2C first**, for bring-up: solderless, and sufficient to prove the receiver, the antenna,
   a first fix and the parser against real traffic.
3. **UART before any vehicle testing**, for starvation tolerance rather than for speed.
4. The pipeline **detects fix-sequence gaps from the receiver's own `iTOW`**, on both
   transports. On I2C that is the only way loss becomes visible at all, and on UART it still
   catches a receiver reset or a configuration that silently reverted.

## Consequences

Positive:

- the receiver can be exercised the day the cables arrive, with nothing soldered
- the parser, the fix model and the timing engine are reached sooner, and are transport-agnostic
- the eventual UART move is a transport swap rather than a rewrite
- gap detection, forced by the weaker transport, is worth having on the stronger one too

Negative:

- two transports to maintain and test rather than one
- the I2C path shares a bus with touch, the IMU, the RTC and the IO expander
- two identical SH1.0 sockets carrying different buses invite a mis-plug, which on mismatched
  pin orders could put 3.3 V onto a data line between two boards
- UART still costs four soldered joints, and that work is deferred rather than avoided

## Notes for implementation

- **I2C:** address `0x42`; bytes-available is a 16-bit count at registers `0xFD`/`0xFE`, and
  the stream is read from `0xFF`.
- **UART:** the module's default is **38400 baud, 8N1**, per the data sheet. The 115200 figure
  in SparkFun's hookup guide is their serial monitor, not the module. 230400 is the target for
  high-rate work, and section 2's rule applies: do not assume the reconfiguration succeeded.
- **The Waveshare UART socket is GPIO43/44, which is UART0 and the default console.** The
  console must move to USB Serial/JTAG first, or log output is transmitted into the receiver
  and the console driver owns the pin the receiver answers on. See issue #9.
- Leave the breakout's `SPI` solder jumper open; closing it disables the UART on those lines.

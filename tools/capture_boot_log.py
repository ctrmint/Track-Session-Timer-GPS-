#!/usr/bin/env python3
"""Capture a cold boot log from the board over its own USB-Serial-JTAG port.

This exists because the obvious approaches do not work and fail in a way that looks like
success. Two traps, both recorded in #181:

Opening the port and waiting captures nothing, because the console output a boot produces
has already been written and discarded by the time a host attaches. The listener has to be
holding the port *before* the reset, and then reset the chip itself.

Resetting it is where the second trap is. On USB-Serial-JTAG, RTS is wired to EN (reset)
and **DTR is wired to GPIO0** (boot select). pyserial asserts both when it opens a port, so
pulsing RTS alone releases reset with GPIO0 still held low: the chip comes up in the ROM
download stub, which prints nothing at all. Zero captured bytes then reads as "the output
was discarded" when it actually means "the application never ran".

A power cycle cannot be captured this way whatever you do, because the USB device is inside
the chip: cutting power destroys the very port the listener is holding. A reset through RTS
is a chip reset (`rst:0x15 USB_UART_CHIP_RESET`) and the handle survives it.

`idf.py monitor` does the equivalent, but refuses to run without a TTY attached, so it
cannot be scripted or run from CI.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
import time

DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_SECONDS = 15.0
# Long enough for EN to be seen as low, short enough not to be a pause anybody notices.
RESET_HOLD_S = 0.15


def capture(port_name: str, seconds: float, destination: Path | None) -> int:
    try:
        import serial
    except ImportError:
        print(
            "pyserial is needed for this tool and is not a dependency of the host checks, "
            "since it is only useful with hardware attached:\n"
            "    .venv/bin/python -m pip install pyserial",
            file=sys.stderr,
        )
        return 2

    port = serial.Serial()
    port.port = port_name
    port.baudrate = 115200
    port.timeout = 0.2
    # Set before opening as well as after: the open itself asserts both lines otherwise,
    # and GPIO0 being low at the moment reset is released is the whole failure mode.
    port.dtr = False
    port.rts = False
    try:
        port.open()
    except Exception as error:  # A missing board is the common case, not a crash.
        print(f"cannot open {port_name}: {error}", file=sys.stderr)
        return 1

    port.dtr = False
    port.rts = True  # EN low: hold the chip in reset.
    time.sleep(RESET_HOLD_S)
    port.dtr = False
    port.rts = False  # Release, with GPIO0 high, so the application boots.

    chunks: list[bytes] = []
    deadline = time.time() + seconds
    try:
        while time.time() < deadline:
            data = port.read(4096)
            if data:
                chunks.append(data)
    except Exception as error:
        # A disappearing port means the board lost power rather than being reset, which
        # this tool cannot capture. Say so rather than reporting an empty log.
        print(f"the port went away mid-capture: {error}", file=sys.stderr)
        print("that is a power cycle, not a reset, and cannot be captured here.",
              file=sys.stderr)
    finally:
        port.close()

    log = b"".join(chunks)
    if destination is not None:
        destination.write_bytes(log)
        print(f"{len(log)} bytes -> {destination}")
    else:
        sys.stdout.write(log.decode("utf-8", errors="replace"))

    if not log:
        print(
            "nothing captured. If the board is attached and powered, it may have come up "
            "in the download stub, which prints nothing: check that DTR is being left "
            "deasserted.",
            file=sys.stderr,
        )
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--seconds", type=float, default=DEFAULT_SECONDS)
    parser.add_argument("--output", type=Path, help="write to a file instead of stdout")
    arguments = parser.parse_args()
    return capture(arguments.port, arguments.seconds, arguments.output)


if __name__ == "__main__":
    raise SystemExit(main())

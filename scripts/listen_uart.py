#!/usr/bin/env python3
"""Display the NUCLEO's ST-LINK virtual COM port in your terminal."""

import argparse
import sys


# ST-LINK/V2-1, V3, V3E and V3 variants that provide a virtual COM port.
STLINK_USB_IDS = {0x374B, 0x374D, 0x374E, 0x374F, 0x3752, 0x3753, 0x3754}


def describe_port(port):
    description = port.description or "Serial port"
    identifier = ""
    if port.vid is not None and port.pid is not None:
        identifier = f" [{port.vid:04x}:{port.pid:04x}]"
    return f"{port.device}: {description}{identifier}"


def is_stlink(port):
    identifiers = " ".join(
        str(getattr(port, name, "") or "")
        for name in ("description", "manufacturer", "product", "interface", "hwid")
    ).upper().replace("-", "").replace(" ", "")
    return port.vid == 0x0483 and (
        port.pid in STLINK_USB_IDS or "STLINK" in identifiers
    )


def choose_port(ports, requested=None):
    """Select one ST-LINK, or the sole serial port; never guess between boards."""
    if requested:
        return requested
    stlinks = [port for port in ports if is_stlink(port)]
    if len(stlinks) == 1:
        return stlinks[0].device
    if not stlinks and len(ports) == 1:
        return ports[0].device
    if not ports:
        raise ValueError(
            "No serial ports found. Connect the board's ST-LINK USB connector "
            "with a data-capable cable, then try again."
        )
    choices = "\n".join(f"  {describe_port(port)}" for port in ports)
    raise ValueError(
        "Multiple serial ports found; select the board with --port:\n" + choices
    )


def open_port(serial, device, baud):
    # Configure control lines before opening to avoid requesting a board reset.
    # Some OS drivers can still briefly assert them when a USB port is opened.
    connection = serial.Serial(
        port=None,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.2,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    )
    connection.dtr = False
    connection.rts = False
    connection.port = device
    try:
        connection.open()
    except BaseException:
        connection.close()
        raise
    return connection


def stream_output(connection, output):
    """Forward bytes unchanged, including CRLF and partial UTF-8 sequences."""
    while True:
        data = connection.read(min(4096, max(1, connection.in_waiting)))
        if data:
            output.write(data)
            output.flush()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial device, e.g. /dev/ttyACM0 or COM3")
    parser.add_argument("--baud", type=int, default=115200, help="baud rate (default: 115200)")
    parser.add_argument("--list-ports", action="store_true", help="list serial ports and exit")
    args = parser.parse_args(argv)
    if args.baud <= 0:
        parser.error("--baud must be positive")

    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print(
            "pyserial is required. Install it with: python3 -m pip install pyserial",
            file=sys.stderr,
        )
        return 1

    try:
        ports = sorted(list_ports.comports(), key=lambda port: port.device)
        if args.list_ports:
            for port in ports:
                print(describe_port(port))
            if not ports:
                print("No serial ports found.")
            return 0
        device = choose_port(ports, args.port)
        connection = open_port(serial, device, args.baud)
        try:
            print(
                f"Listening on {device} at {args.baud} baud (8N1). Ctrl-C to stop.",
                file=sys.stderr,
                flush=True,
            )
            stream_output(connection, sys.stdout.buffer)
        finally:
            connection.close()
    except KeyboardInterrupt:
        print("\nStopped.", file=sys.stderr)
        return 0
    except (serial.SerialException, OSError, ValueError) as error:
        print(f"UART error: {error}", file=sys.stderr)
        print(
            "Check the USB connection and port permissions, and close any other "
            "terminal using this port.",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

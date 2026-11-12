#!/usr/bin/env python3
r"""Display the latest payload JPEG received from the comms board over UART.

Install: python3 -m pip install -r scripts/requirements-images.txt
Run:     python3 scripts/view_images.py --port /dev/ttyACM0
Capture: python3 scripts/view_images.py --port /dev/ttyACM0 --headless \
             --count 3 --timeout 60 --output-dir build/received-images

Use --list-ports to identify the comms board when both boards are attached.
Use --serial-number SERIAL to keep selecting that board after port renumbering.
The default UART configuration is 921600 baud, 8N1. The graphical viewer also
requires tkinter (on Debian/Ubuntu: sudo apt install python3-tk).
"""

import argparse
from dataclasses import dataclass
import io
import math
import os
from pathlib import Path
import queue
import struct
import sys
import threading
import time
import zlib

if __package__:
    from .listen_uart import choose_port, describe_port, open_port
else:
    from listen_uart import choose_port, describe_port, open_port


MAGIC = b"YCPJPG1\0"
HEADER = struct.Struct("<8sIIII")
HEADER_SIZE = HEADER.size
MAX_JPEG_BYTES = 800 * 480 * 2
READ_SIZE = 4096


@dataclass(frozen=True)
class ImageFrame:
    image_id: int
    jpeg: bytes
    crc32: int
    received_at: float


class FrameParser:
    """Recover CRC-checked frames from partial reads, diagnostics and corruption.

    A 24-byte little-endian header contains the eight-byte magic, image ID,
    JPEG length, JPEG CRC32, then CRC32 of the preceding 20 header bytes.
    CRC32 uses the same convention as zlib.crc32. No graphics or serial
    dependencies are needed to use the parser.
    """

    def __init__(self, max_jpeg_bytes=MAX_JPEG_BYTES):
        if max_jpeg_bytes <= 0:
            raise ValueError("maximum JPEG length must be positive")
        self.max_jpeg_bytes = max_jpeg_bytes
        self.buffer = bytearray()
        self.header_errors = 0
        self.crc_errors = 0
        self.truncated_frames = 0
        self.discarded_bytes = 0

    @property
    def errors(self):
        return self.header_errors + self.crc_errors + self.truncated_frames

    @property
    def pending_image(self):
        """Return (image ID, received JPEG bytes, total JPEG bytes), if known."""
        header = self._header_at(0)
        if header is None:
            return None
        image_id, length, _ = header
        return image_id, len(self.buffer) - HEADER_SIZE, length

    def _header_at(self, offset):
        if len(self.buffer) - offset < HEADER_SIZE:
            return None
        magic, image_id, length, crc, header_crc = HEADER.unpack_from(self.buffer, offset)
        if (magic != MAGIC or not 0 < length <= self.max_jpeg_bytes or
                zlib.crc32(self.buffer[offset:offset + 20]) != header_crc):
            return None
        return image_id, length, crc

    def _discard(self, length):
        self.discarded_bytes += length
        del self.buffer[:length]

    def _replacement_frame(self):
        # A reset may interrupt a JPEG. Require an entire CRC-checked later
        # frame before abandoning it, so a magic prefix within JPEG data is safe.
        offset = self.buffer.find(MAGIC, 1)
        while offset >= 0:
            header = self._header_at(offset)
            if header is not None:
                _, length, crc = header
                start = offset + HEADER_SIZE
                end = start + length
                if end <= len(self.buffer) and zlib.crc32(self.buffer[start:end]) == crc:
                    return offset
            offset = self.buffer.find(MAGIC, offset + 1)
        return None

    def _parse(self):
        frames = []
        while True:
            offset = self.buffer.find(MAGIC)
            if offset < 0:
                # Retain only a possible partial magic at the next read boundary.
                self._discard(max(0, len(self.buffer) - len(MAGIC) + 1))
                break
            if offset:
                self._discard(offset)
            if len(self.buffer) < HEADER_SIZE:
                break
            header = self._header_at(0)
            if header is None:
                self.header_errors += 1
                self._discard(1)
                continue
            image_id, length, crc = header
            end = HEADER_SIZE + length
            if len(self.buffer) < end:
                replacement = self._replacement_frame()
                if replacement is not None:
                    self.truncated_frames += 1
                    self._discard(replacement)
                    continue
                break
            jpeg = bytes(self.buffer[HEADER_SIZE:end])
            if zlib.crc32(jpeg) != crc:
                self.crc_errors += 1
                self._discard(1)
                continue
            frames.append(ImageFrame(image_id, jpeg, crc, time.monotonic()))
            del self.buffer[:end]
        return frames

    def feed(self, data):
        """Return all complete valid frames, including repeated image IDs.

        Process even very large caller reads in bounded pieces. Stored input
        never grows beyond one maximum-length frame plus one read-size piece.
        """
        frames = []
        for offset in range(0, len(data), READ_SIZE):
            self.buffer.extend(data[offset:offset + READ_SIZE])
            frames.extend(self._parse())
        return frames


def describe_image_port(port):
    serial_number = getattr(port, "serial_number", None) or "unknown"
    return f"{describe_port(port)} serial={serial_number}"


def select_image_port(ports, requested=None, serial_number=None):
    if serial_number is not None:
        matches = [port for port in ports
                   if str(getattr(port, "serial_number", "") or "").casefold() == serial_number.casefold()]
        if len(matches) == 1:
            return matches[0].device
        choices = "\n".join(f"  {describe_image_port(port)}" for port in ports)
        problem = "No port matches" if not matches else "Multiple ports match"
        raise ValueError(f"{problem} serial number {serial_number}; select with --port:\n{choices}")
    try:
        return choose_port(ports, requested)
    except ValueError as error:
        raise ValueError(f"{error}\nUse --list-ports to see USB serial numbers, "
                         "then select --port or --serial-number.") from error


def open_image_port(serial, device, baud):
    connection = open_port(serial, device, baud)
    try:
        # Set before starting a reader. The lock is released when the port closes.
        if os.name == "posix":
            connection.exclusive = True
    except BaseException:
        connection.close()
        raise
    return connection


class ImageReceiver(threading.Thread):
    """Read UART away from the GUI and retain only the latest display frame."""

    def __init__(self, connection, *, count=None, timeout=None, output_dir=None,
                 on_frame=None):
        super().__init__(name="image-uart", daemon=True)
        self.connection = connection
        self.count = count
        self.timeout = timeout
        self.output_dir = output_dir
        self.on_frame = on_frame
        self.parser = FrameParser()
        self.latest = queue.Queue(maxsize=1)
        self.stop_requested = threading.Event()
        self.done = threading.Event()
        self.frame_count = 0
        self.bytes_received = 0
        self.last_byte_at = None
        self.pending_image = None
        self.error = None

    def run(self):
        started = time.monotonic()
        try:
            while not self.stop_requested.is_set():
                if self.timeout is not None and time.monotonic() - started >= self.timeout:
                    if self.frame_count == 0 or self.count is not None:
                        self.error = "Timed out before receiving the requested images."
                    return
                data = self.connection.read(min(READ_SIZE, max(1, self.connection.in_waiting)))
                if data:
                    self.bytes_received += len(data)
                    self.last_byte_at = time.monotonic()
                frames = self.parser.feed(data)
                self.pending_image = self.parser.pending_image
                for frame in frames:
                    self.frame_count += 1
                    if self.output_dir is not None:
                        filename = f"image_{frame.image_id:010d}_{self.frame_count:06d}.jpg"
                        (self.output_dir / filename).write_bytes(frame.jpeg)
                    if self.on_frame is not None:
                        self.on_frame(frame, self.frame_count)
                    try:
                        self.latest.get_nowait()
                    except queue.Empty:
                        pass
                    self.latest.put_nowait(frame)
                    if self.count is not None and self.frame_count >= self.count:
                        return
        except OSError as error:
            self.error = f"UART or image output error: {error}"
        finally:
            self.done.set()


def receiver_status(receiver, frame=None, *, now=None, decode_errors=0, decode_error=None):
    """Describe silence, unrelated UART data, a partial image, or the last image."""
    now = time.monotonic() if now is None else now
    if frame is not None:
        age = max(0, now - frame.received_at)
        details = f"Image {frame.image_id} | {len(frame.jpeg):,} bytes | image age {age:.1f}s"
    elif receiver.frame_count:
        details = "Waiting for the next image"
    elif receiver.bytes_received:
        details = "Waiting for a valid image header"
    else:
        details = "Waiting for UART data; check the selected comms port"
    if receiver.pending_image is not None:
        image_id, received, total = receiver.pending_image
        progress = f"Receiving image {image_id}: {received:,}/{total:,} bytes ({100 * received / total:.1f}%)"
        details = details + "\n" + progress if frame is not None else progress
    byte_age = (f"last byte {max(0, now - receiver.last_byte_at):.1f}s ago"
                if receiver.last_byte_at is not None else "no data yet")
    details += (f"\nUART {receiver.bytes_received:,} bytes | {byte_age} | "
                f"received {receiver.frame_count} frames | errors {receiver.parser.errors + decode_errors}")
    if receiver.error or decode_error:
        details += "\n" + (receiver.error or decode_error)
    elif receiver.done.is_set():
        details += "\nStopped"
    return details


def show_images(receiver, source):
    import tkinter as tk
    from PIL import Image, ImageTk

    try:
        root = tk.Tk()
    except tk.TclError as error:
        raise RuntimeError(f"Cannot open a display: {error}. Use --headless to capture images.") from error
    root.title("CubeSat / Payload Images")
    root.geometry("1000x700")
    root.minsize(400, 300)
    root.configure(bg="#101620")
    canvas = tk.Canvas(root, bg="#101620", highlightthickness=0)
    canvas.pack(fill=tk.BOTH, expand=True)
    status = tk.StringVar(value="Waiting for a complete image...")
    tk.Label(root, textvariable=status, anchor="w", justify="left", padx=12, pady=10,
             bg="#18212f", fg="#e2eaf5").pack(fill=tk.X)
    original = None
    last_frame = None
    last_size = None
    photo = None
    decode_errors = 0
    decode_error = None

    def refresh():
        nonlocal original, last_frame, last_size, photo, decode_errors, decode_error
        try:
            frame = receiver.latest.get_nowait()
        except queue.Empty:
            frame = None
        if frame is not None:
            try:
                with Image.open(io.BytesIO(frame.jpeg)) as image:
                    if image.format != "JPEG":
                        raise ValueError("payload is not a JPEG")
                    original = image.convert("RGB")
                last_frame = frame
                last_size = None
                decode_error = None
            except (OSError, ValueError) as error:
                decode_errors += 1
                decode_error = f"JPEG decode error: {error}"
        size = (max(1, canvas.winfo_width()), max(1, canvas.winfo_height()))
        if original is not None and size != last_size:
            display = original.copy()
            display.thumbnail(size, Image.Resampling.LANCZOS)
            photo = ImageTk.PhotoImage(display)
            canvas.delete("all")
            canvas.create_image(size[0] // 2, size[1] // 2, image=photo)
            last_size = size
        details = receiver_status(receiver, last_frame, decode_errors=decode_errors,
                                  decode_error=decode_error)
        status.set(f"{source}\n{details}")
        if receiver.done.is_set() and receiver.count is not None and not receiver.error:
            root.after(100, root.destroy)
        else:
            root.after(30, refresh)

    root.bind("<Escape>", lambda event: root.destroy())
    receiver.start()
    root.after(30, refresh)
    try:
        root.mainloop()
    finally:
        receiver.stop_requested.set()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--port", help="comms serial device, e.g. /dev/ttyACM0 or COM3")
    selection.add_argument("--serial-number", help="select the comms board by its USB serial number")
    parser.add_argument("--baud", type=int, default=921600, help="baud rate (default: 921600)")
    parser.add_argument("--list-ports", action="store_true", help="list serial ports and exit")
    parser.add_argument("--headless", action="store_true", help="receive without opening a window")
    parser.add_argument("--count", type=int, metavar="N", help="stop after N valid image frames")
    parser.add_argument("--timeout", type=float, metavar="SECONDS", help="limit total receive time")
    parser.add_argument("--output-dir", type=Path, help="save each received JPEG to this directory")
    args = parser.parse_args(argv)
    if args.baud <= 0:
        parser.error("--baud must be positive")
    if args.count is not None and args.count <= 0:
        parser.error("--count must be positive")
    if args.timeout is not None and (not math.isfinite(args.timeout) or args.timeout <= 0):
        parser.error("--timeout must be finite and positive")

    connection = None
    receiver = None
    try:
        import serial
        from serial.tools import list_ports

        ports = sorted(list_ports.comports(), key=lambda port: port.device)
        if args.list_ports:
            for port in ports:
                print(describe_image_port(port))
            if not ports:
                print("No serial ports found.")
            return 0
        device = select_image_port(ports, args.port, args.serial_number)
        if args.output_dir is not None:
            args.output_dir.mkdir(parents=True, exist_ok=True)
        connection = open_image_port(serial, device, args.baud)
        source = f"{device} / {args.baud} baud / 8N1"
        print(f"Receiving images on {source}. Ctrl-C or close the window to stop.",
              file=sys.stderr, flush=True)

        def report(frame, count):
            print(f"Image {frame.image_id}: {len(frame.jpeg)} bytes, "
                  f"CRC32 {frame.crc32:08x}, frame {count}", flush=True)

        receiver = ImageReceiver(connection, count=args.count, timeout=args.timeout,
                                 output_dir=args.output_dir,
                                 on_frame=report if args.headless else None)
        if args.headless:
            receiver.start()
            next_status = time.monotonic() + 1
            while not receiver.done.wait(0.1):
                now = time.monotonic()
                if now >= next_status:
                    print(receiver_status(receiver, now=now).replace("\n", " | "),
                          file=sys.stderr, flush=True)
                    next_status = now + 1
        else:
            show_images(receiver, source)
        print(f"Received {receiver.frame_count} image frame(s); "
              f"UART {receiver.bytes_received} bytes; "
              f"stream errors: {receiver.parser.errors} "
              f"(header {receiver.parser.header_errors}, CRC {receiver.parser.crc_errors}, "
              f"interrupted {receiver.parser.truncated_frames}).", file=sys.stderr)
        if receiver.error:
            print(receiver.error, file=sys.stderr)
            return 1
        return 0
    except ImportError as error:
        print(f"Missing dependency: {error.name}. Install with: "
              "python3 -m pip install -r scripts/requirements-images.txt; "
              "the graphical viewer also needs tkinter (python3-tk on Debian/Ubuntu).",
              file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Image viewer error: {error}", file=sys.stderr)
        return 1
    finally:
        if receiver is not None:
            receiver.stop_requested.set()
            if receiver.is_alive():
                receiver.join(timeout=1)
        if connection is not None:
            connection.close()


if __name__ == "__main__":
    raise SystemExit(main())

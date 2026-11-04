#!/usr/bin/env python3
"""Refresh-button viewer for one packetized, binary PPM (P6, maxval 255).

Each .dat/.bin file: [2-byte big-endian sequence ID][256 payload bytes].
CRC must already have been checked and removed by GNU Radio.
Use one image/transmission per directory, with sequence IDs starting at zero.
The received PPM header supplies the image dimensions; no source image is needed.
"""

import argparse
import re
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, ttk
from PIL import Image, ImageTk

PAYLOAD_BYTES = 256
ID_BYTES = 2
TOKEN = re.compile(rb"(?:\s+|\#[^\r\n]*(?:\r\n|\r|\n))*([^\s#]+)")


def read_packets(folder: Path) -> tuple[dict[int, bytes], int]:
    """Ignore unfinished/incorrect-sized files; reject conflicting duplicate IDs."""
    if not folder.is_dir():
        raise ValueError("Choose an existing packet directory.")
    packets: dict[int, bytes] = {}
    ignored = 0
    for path in folder.iterdir():
        if path.suffix.lower() not in {".dat", ".bin"} or not path.is_file():
            continue
        try:
            with path.open("rb") as handle:
                raw = handle.read(ID_BYTES + PAYLOAD_BYTES + 1)
        except OSError:
            ignored += 1
            continue
        if len(raw) != ID_BYTES + PAYLOAD_BYTES:
            ignored += 1  # Often a packet file still being written.
            continue
        seq = int.from_bytes(raw[:ID_BYTES], "big")
        payload = raw[ID_BYTES:]
        if seq in packets and packets[seq] != payload:
            raise ValueError(f"Conflicting ID {seq}: use a separate folder for each run/image.")
        packets[seq] = payload
    return packets, ignored


def ppm_header(prefix: bytes) -> tuple[int, int, int]:
    """Parse only received, contiguous header bytes, not zero-filled gaps."""
    fields = []
    pos = 0
    for _ in range(4):
        match = TOKEN.match(prefix, pos)
        if match is None:
            raise ValueError("Waiting for the complete PPM header, starting at packet 0.")
        fields.append(match.group(1))
        pos = match.end()
    if fields[0] != b"P6":
        raise ValueError("This viewer needs binary PPM (P6), not P3, JPEG, or PNG.")
    if any(not field.isdigit() for field in fields[1:]):
        raise ValueError("Invalid PPM header: check packet format and sequence numbering.")
    width, height, maxval = map(int, fields[1:])
    if maxval != 255 or width < 1 or height < 1:
        raise ValueError("Expected a P6 PPM with positive dimensions and maxval 255.")
    if pos >= len(prefix) or prefix[pos] not in b" \t\r\n\v\f":
        raise ValueError("Waiting for the complete PPM header.")
    # P6 specifies ONE whitespace delimiter after maxval. Never strip raster bytes.
    offset = pos + 1
    if offset + width * height * 3 > PAYLOAD_BYTES * (1 << (8 * ID_BYTES)):
        raise ValueError("Image exceeds this demo's sequence-ID range (or header is invalid).")
    return width, height, offset


def assemble(folder: Path) -> tuple[Image.Image, str]:
    packets, ignored = read_packets(folder)
    if 0 not in packets:
        raise ValueError(f"Waiting for packet 0 (PPM header). Ignored files: {ignored}.")

    # Gather the contiguous beginning so comments/headers can span several packets.
    prefix = bytearray()
    seq = 0
    while seq in packets:
        prefix.extend(packets[seq])
        seq += 1
    width, height, offset = ppm_header(bytes(prefix))
    total_bytes = offset + width * height * 3
    expected = (total_bytes + PAYLOAD_BYTES - 1) // PAYLOAD_BYTES
    missing = set(range(expected)) - packets.keys()

    # Place packets by TRANSMITTED ID, never by filename or arrival order.
    data = bytearray(total_bytes)  # Missing data starts at zero.
    for seq, payload in packets.items():
        start = seq * PAYLOAD_BYTES
        end = min(start + PAYLOAD_BYTES, total_bytes)
        if start < end:
            data[start:end] = payload[:end - start]
    pixels = data[offset:]

    # Black out the WHOLE RGB pixel if any of its three bytes were lost.
    # Do this after copying, since a pixel can straddle two packets.
    for seq in missing:
        start = max(0, seq * PAYLOAD_BYTES - offset)
        end = min(len(pixels), (seq + 1) * PAYLOAD_BYTES - offset)
        if start < end:
            start = (start // 3) * 3
            end = min(len(pixels), ((end + 2) // 3) * 3)
            pixels[start:end] = b"\x00" * (end - start)

    image = Image.frombytes("RGB", (width, height), bytes(pixels))
    status = (f"{width} x {height} | Packets: {expected - len(missing)}/{expected}"
              f" | Missing: {len(missing)} | Ignored unfinished/invalid files: {ignored}")
    return image, status


class Viewer(tk.Tk):
    def __init__(self, directory: Path):
        super().__init__()
        self.title("Received packet image viewer")
        self.folder = tk.StringVar(value=str(directory.expanduser().absolute()))
        self.status = tk.StringVar(value="Select a packet directory, then press Refresh.")
        self.photo = None  # Keep a reference so Tk does not lose the displayed image.
        toolbar = ttk.Frame(self, padding=8)
        toolbar.pack(fill="x")
        ttk.Entry(toolbar, textvariable=self.folder, width=65).pack(
            side="left", fill="x", expand=True)
        ttk.Button(toolbar, text="Browse...", command=self.browse).pack(side="left", padx=5)
        ttk.Button(toolbar, text="Refresh", command=self.refresh).pack(side="left")
        self.preview = ttk.Label(self, anchor="center")
        self.preview.pack(fill="both", expand=True, padx=8, pady=8)
        ttk.Label(self, textvariable=self.status, padding=8).pack(fill="x")

    def browse(self) -> None:
        selected = filedialog.askdirectory(parent=self, title="Packet directory")
        if selected:
            self.folder.set(selected)

    def refresh(self) -> None:
        try:
            image, status = assemble(Path(self.folder.get()).expanduser())
            # Fit the whole image on screen. NEAREST keeps missing regions black.
            image.thumbnail((max(100, min(1280, self.winfo_screenwidth() - 100)),
                             max(100, min(800, self.winfo_screenheight() - 220))),
                            Image.Resampling.NEAREST)
            self.photo = ImageTk.PhotoImage(image, master=self)
            self.preview.configure(image=self.photo, text="")
            self.status.set(status)
        except (OSError, ValueError, MemoryError) as exc:
            self.photo = None
            self.preview.configure(image="", text="No image available for this refresh.")
            self.status.set(str(exc) or "Not enough memory to reconstruct this image.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path, default=Path("rx_packets"))
    Viewer(parser.parse_args().directory).mainloop()

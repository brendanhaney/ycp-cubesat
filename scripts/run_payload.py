#!/usr/bin/env python3
"""Load and start the STM32N6 payload in SRAM through ST-LINK."""

import argparse
from pathlib import Path
import re
import struct
import subprocess
import sys


STM32N6_VECTOR_BASE = 0x34180400
STM32N6_SRAM_END = 0x34200000
ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
ELF_PROGRAM_HEADER = struct.Struct("<IIIIIIII")


def startup_vectors(path):
    image = path.read_bytes()
    if len(image) < ELF_HEADER.size:
        raise ValueError("Payload is not a complete ELF image")
    header = ELF_HEADER.unpack_from(image)
    identity, kind, machine, version, entry, table = header[:6]
    if (identity[:7] != b"\x7fELF\x01\x01\x01" or kind != 2 or
            machine != 40 or version != 1):
        raise ValueError("Payload must be a 32-bit little-endian ARM executable")
    stride, count = header[9:11]
    if (stride < ELF_PROGRAM_HEADER.size or not count or
            table + stride * count > len(image)):
        raise ValueError("Payload has an invalid ELF program-header table")

    segments = []
    for index in range(count):
        segment = ELF_PROGRAM_HEADER.unpack_from(image, table + index * stride)
        kind, offset, address, physical, size, memory_size, flags, _ = segment
        if kind != 1:
            continue
        if (physical != address or size > memory_size or offset + size > len(image) or
                not STM32N6_VECTOR_BASE <= address <= address + memory_size <= STM32N6_SRAM_END):
            raise ValueError("Payload has a segment outside STM32N6 payload SRAM")
        segments.append((offset, address, size, flags))

    vectors = None
    for offset, address, size, _ in segments:
        if address <= STM32N6_VECTOR_BASE and STM32N6_VECTOR_BASE + 8 <= address + size:
            vectors = struct.unpack_from("<II", image, offset + STM32N6_VECTOR_BASE - address)
            break
    if vectors is None:
        raise ValueError("Payload has no vector table at the STM32N6 SRAM entry")

    stack, reset = vectors
    if stack & 7 or not STM32N6_VECTOR_BASE < stack <= STM32N6_SRAM_END:
        raise ValueError("Payload has an invalid initial stack pointer")
    if reset != entry or not reset & 1 or not any(
            flags & 1 and address <= (reset & ~1) < address + size
            for _, address, size, flags in segments):
        raise ValueError("Payload has an invalid reset vector or ELF entry point")
    return stack, reset & ~1


def program(command):
    result = subprocess.run(command, check=False, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    print(result.stdout, end="", flush=True)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--programmer", required=True)
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("connection", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    try:
        stack, reset = startup_vectors(args.elf)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    connection = args.connection
    if connection[:1] == ["--"]:
        connection = connection[1:]
    if not connection:
        parser.error("Provide STM32CubeProgrammer connection arguments after --")
    prefix = [args.programmer, "-q", *connection]
    command = [*prefix, "-halt", "-w", str(args.elf),
               "-v", "-coreReg", f"PC=0x{reset:08X}", f"MSP=0x{stack:08X}",
               "-run", "-score"]
    try:
        result = program(command)
        if result.returncode:
            return result.returncode
        if "Core is running" not in result.stdout:
            print("Payload did not start: no running core reported. Power-cycle the board and retry.",
                  file=sys.stderr)
            return 1
        status = program([*prefix, "-score", "-coreReg", "PC"])
    except OSError as error:
        parser.error(str(error))
    if status.returncode:
        return status.returncode
    counter = re.search(r"\bPC\s*=\s*(0x[0-9a-fA-F]+)", status.stdout)
    if ("Core is running" not in status.stdout or counter is None or
            not STM32N6_VECTOR_BASE <= int(counter[1], 16) < STM32N6_SRAM_END):
        print("Payload did not remain running in STM32N6 SRAM.",
              file=sys.stderr)
        return 1
    print("Payload started from SRAM. Run this target again after reset or power loss.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

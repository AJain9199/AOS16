#!/usr/bin/env python3
"""
programmer.py
-------------
Programs an SST39SF010A/020A/040 flash chip via an Arduino running the
companion firmware.

Usage
-----
    python programmer.py COM3 image.hex                  # write + verify
    python programmer.py /dev/ttyUSB0 image.hex --no-verify
    python programmer.py COM3 image.hex --sector-erase   # erase only used sectors
    python programmer.py COM3 --identify                 # print chip IDs and exit
    python programmer.py COM3 --chip-erase               # blank the chip and exit

Dependencies
------------
    pip install pyserial
"""

import argparse
import sys
import time
import struct
from typing import Optional

import serial

from logisim_parser import load as load_hex

# ---------------------------------------------------------------------------
# Protocol constants — must match protocol.h in the firmware
# ---------------------------------------------------------------------------
CMD_PING          = b'P'
CMD_GET_ID        = b'I'
CMD_CHIP_ERASE    = b'E'
CMD_SECTOR_ERASE  = b'S'
CMD_WRITE         = b'W'
CMD_READ          = b'R'

RESP_OK    = ord('K')
RESP_ERR   = ord('F')
RESP_READY = ord('Z')

SECTOR_SIZE  = 0x1000          # 4 KB per datasheet
CHUNK_SIZE   = 64              # bytes per write/read command
BAUD_RATE    = 230400
BOOT_TIMEOUT = 5.0             # seconds to wait for RESP_READY after reset

# ---------------------------------------------------------------------------
# Known device IDs → (description, chip_size_in_bytes)
# ---------------------------------------------------------------------------
DEVICE_IDS: dict[tuple[int, int], tuple[str, int]] = {
    (0xBF, 0xB5): ("SST39SF010A (1 Mbit, 128 KB)", 0x010000),
    (0xBF, 0xB6): ("SST39SF020A (2 Mbit, 256 KB)", 0x020000),
    (0xBF, 0xB7): ("SST39SF040  (4 Mbit, 512 KB)", 0x040000),
}


# ---------------------------------------------------------------------------
# Programmer class
# ---------------------------------------------------------------------------

class ProtocolError(Exception):
    pass


class Programmer:
    def __init__(self, port: str, baud: int = BAUD_RATE):
        self._ser = serial.Serial(port, baud, timeout=10)
        self._wait_for_ready()

    def close(self):
        self._ser.close()

    # ------------------------------------------------------------------
    # Connection
    # ------------------------------------------------------------------

    def _wait_for_ready(self):
        """Block until the Arduino sends RESP_READY or the timeout expires."""
        print("Waiting for Arduino...", end="", flush=True)
        self._ser.timeout = 0.1
        deadline = time.monotonic() + BOOT_TIMEOUT
        buf = b""
        while time.monotonic() < deadline:
            buf += self._ser.read(64)
            if RESP_READY in buf:
                print(" ready.")
                self._ser.timeout = 10
                self._ser.reset_input_buffer()
                return
        # Arduino may not have reset (no DTR line); try a ping instead
        self._ser.timeout = 2
        self._ser.reset_input_buffer()
        self._ser.write(CMD_PING)
        resp = self._ser.read(1)
        if resp and resp[0] == RESP_OK:
            print(" connected (no reset detected).")
            return
        raise TimeoutError(
            "Arduino did not respond.  Check port, baud rate, and that the "
            "correct firmware is uploaded."
        )

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _recv(self, n: int) -> bytes:
        data = self._ser.read(n)
        if len(data) != n:
            raise TimeoutError(f"Expected {n} bytes, got {len(data)}")
        return data

    def _expect_ok(self):
        resp = self._recv(1)[0]
        if resp != RESP_OK:
            raise ProtocolError(f"Expected OK (0x{RESP_OK:02X}), got 0x{resp:02X}")

    @staticmethod
    def _addr_bytes(address: int) -> bytes:
        """Pack a 24-bit address as 3 big-endian bytes."""
        return struct.pack(">I", address)[1:]   # drop the high byte of a 32-bit int

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def ping(self):
        self._ser.write(CMD_PING)
        self._expect_ok()

    def get_id(self) -> tuple[int, int]:
        """Return (manufacturer_id, device_id)."""
        self._ser.write(CMD_GET_ID)
        self._expect_ok()
        data = self._recv(2)
        return data[0], data[1]

    def chip_erase(self):
        """Erase the entire chip.  Blocks until complete (~100 ms)."""
        self._ser.timeout = 5
        self._ser.write(CMD_CHIP_ERASE)
        self._expect_ok()
        self._ser.timeout = 10

    def sector_erase(self, address: int):
        """Erase the 4 KB sector containing *address*."""
        self._ser.timeout = 2
        self._ser.write(CMD_SECTOR_ERASE + self._addr_bytes(address))
        self._expect_ok()
        self._ser.timeout = 10

    def write_chunk(self, address: int, data: bytes | bytearray):
        """Write up to 256 bytes.  The target area must already be erased."""
        assert 1 <= len(data) <= 256, "Chunk must be 1–256 bytes"
        # len=0 on the wire encodes 256
        wire_len = len(data) & 0xFF
        self._ser.write(CMD_WRITE + self._addr_bytes(address) + bytes([wire_len]) + bytes(data))
        self._expect_ok()

    def read_chunk(self, address: int, length: int) -> bytes:
        """Read *length* bytes (1–65535) from the chip."""
        assert 1 <= length <= 0xFFFF
        self._ser.write(CMD_READ + self._addr_bytes(address) + struct.pack(">H", length))
        data = self._recv(length)
        self._expect_ok()
        return data

    # ------------------------------------------------------------------
    # High-level operations
    # ------------------------------------------------------------------

    def erase_sectors_for(self, image: bytearray):
        """Erase only the sectors that contain non-0xFF data."""
        sectors = sorted({
            addr // SECTOR_SIZE
            for addr, byte in enumerate(image)
            if byte != 0xFF
        })
        if not sectors:
            print("Image is all 0xFF — nothing to erase.")
            return
        print(f"Erasing {len(sectors)} sector(s)...")
        for i, sector in enumerate(sectors):
            base = sector * SECTOR_SIZE
            self.sector_erase(base)
            _progress(i + 1, len(sectors), prefix="  Erase")
        print()

    def program(self, image: bytearray):
        """Write *image* to the chip, skipping all-0xFF chunks."""
        total   = len(image)
        written = 0
        addr    = 0

        print(f"Programming {total:,} bytes...")
        while addr < total:
            chunk = image[addr: addr + CHUNK_SIZE]
            if any(b != 0xFF for b in chunk):
                self.write_chunk(addr, chunk)
                written += len(chunk)
            addr += len(chunk)
            _progress(addr, total, prefix="  Write ")
        print(f"\n  {written:,} bytes written ({total - written:,} skipped as 0xFF).")

    def verify(self, image: bytearray) -> bool:
        """Read back and compare every byte.  Returns True if the chip matches."""
        total  = len(image)
        errors = 0
        addr   = 0

        print(f"Verifying {total:,} bytes...")
        while addr < total:
            length   = min(CHUNK_SIZE, total - addr)
            actual   = self.read_chunk(addr, length)
            expected = image[addr: addr + length]
            for i, (e, a) in enumerate(zip(expected, actual)):
                if e != a:
                    print(f"\n  MISMATCH at 0x{addr + i:05X}: "
                          f"expected 0x{e:02X}, got 0x{a:02X}")
                    errors += 1
                    if errors >= 20:
                        print("  Too many errors — aborting verify.")
                        return False
            addr += length
            _progress(addr, total, prefix="  Verify")

        print()
        if errors == 0:
            print("  Verify OK.")
            return True
        print(f"  Verify FAILED — {errors} error(s).")
        return False


# ---------------------------------------------------------------------------
# Utility
# ---------------------------------------------------------------------------

def _progress(done: int, total: int, prefix: str = ""):
    pct   = 100 * done // total
    bar   = "█" * (pct // 5) + "░" * (20 - pct // 5)
    print(f"\r{prefix}  [{bar}] {pct:3d}%  {done:,}/{total:,}", end="", flush=True)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="SST39SF01x/02x/04x flash programmer via Arduino",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("port",      help="Serial port, e.g. COM3 or /dev/ttyUSB0")
    p.add_argument("file",      nargs="?", help="Logisim v3.0 hex image to program")
    p.add_argument("-b", "--baud",   type=int, default=BAUD_RATE, help=f"Baud rate (default {BAUD_RATE})")
    p.add_argument("--size",         type=lambda x: int(x, 0), default=None,
                   help="Force image size in bytes, e.g. 0x20000 for 128 KB")
    p.add_argument("--identify",     action="store_true", help="Read chip IDs and exit")
    p.add_argument("--chip-erase",   action="store_true", help="Erase entire chip and exit")
    p.add_argument("--sector-erase", action="store_true",
                   help="Erase only sectors occupied by the image (default: chip erase)")
    p.add_argument("--no-verify",    action="store_true", help="Skip verify step after programming")
    return p


def main():
    args = build_parser().parse_args()

    prog = Programmer(args.port, args.baud)

    # -- Identify ----------------------------------------------------------
    mfr_id, dev_id = prog.get_id()
    device_info = DEVICE_IDS.get((mfr_id, dev_id))
    if device_info:
        desc, chip_size = device_info
    else:
        desc, chip_size = "Unknown device", None

    print(f"Chip ID: manufacturer=0x{mfr_id:02X}  device=0x{dev_id:02X}  → {desc}")

    if mfr_id == 0x00 and dev_id == 0x00:
        print("ERROR: Got all-zero IDs.  Check wiring and that the chip is powered.")
        prog.close()
        sys.exit(1)

    if args.identify:
        prog.close()
        return

    # -- Chip-erase only ---------------------------------------------------
    if args.chip_erase and args.file is None:
        print("Erasing chip...")
        prog.chip_erase()
        print("Done.")
        prog.close()
        return

    # -- Load image --------------------------------------------------------
    if args.file is None:
        print("ERROR: A hex file is required for programming.")
        prog.close()
        sys.exit(1)

    print(f"Loading {args.file} ...")
    # Load without a size cap so multi-chip images are fully parsed.
    # --size can still force a specific buffer length if needed.
    image = load_hex(args.file, size=args.size)
    print(f"  {len(image):,} bytes loaded.")

    # -- Split image into per-chip slices -----------------------------------
    # Each chip receives a slice of chip_size bytes at the same addresses
    # (0x00000 … chip_size-1).  The user swaps the physical chip between
    # slices so the programmer never needs to know which socket it is.
    if chip_size is None:
        # Unknown chip — treat the whole image as a single slice and hope
        # for the best; the user was already warned above.
        slices = [image]
    else:
        slices = [
            image[start: start + chip_size]
            for start in range(0, len(image), chip_size)
        ]
        # Pad the last slice to a full chip_size with 0xFF so erase/verify
        # comparisons are always against a complete chip image.
        if len(slices[-1]) < chip_size:
            slices[-1] = slices[-1] + bytearray(b"\xff" * (chip_size - len(slices[-1])))

    n_chips = len(slices)
    if n_chips > 1:
        print(f"Image spans {n_chips} chip(s) of {chip_size:,} bytes each.")

    # -- Program each chip --------------------------------------------------
    for chip_index, chunk in enumerate(slices):
        if chip_index > 0:
            print()
            print(f"─" * 60)
            print(f"  Insert chip {chip_index + 1} of {n_chips} and press Enter …")
            input()
            # Re-identify to confirm the new chip is seated and responding
            mfr_id, dev_id = prog.get_id()
            device_info = DEVICE_IDS.get((mfr_id, dev_id))
            new_desc = device_info[0] if device_info else "Unknown device"
            print(f"  Chip ID: 0x{mfr_id:02X} / 0x{dev_id:02X}  → {new_desc}")
            if mfr_id == 0x00 and dev_id == 0x00:
                print("  ERROR: No chip detected.  Aborting.")
                prog.close()
                sys.exit(1)
            print(f"─" * 60)

        print(f"\n[ Chip {chip_index + 1}/{n_chips} ]"
              f"  bytes 0x{chip_index * (chip_size or 0):05X}"
              f" – 0x{chip_index * (chip_size or 0) + len(chunk) - 1:05X}"
              f" of image")

        # Erase
        if args.sector_erase:
            prog.erase_sectors_for(chunk)
        else:
            print("  Erasing chip...")
            prog.chip_erase()
            print("  Chip erased.")

        # Program
        prog.program(chunk)

        # Verify
        if not args.no_verify:
            ok = prog.verify(chunk)
            if not ok:
                prog.close()
                sys.exit(1)

    prog.close()
    print("All done.")


if __name__ == "__main__":
    main()
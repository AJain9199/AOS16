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
    python programmer.py COM3 image.hex --reroute PRGM.csv   # remap data bits
    python programmer.py COM3 --identify                 # print chip IDs and exit
    python programmer.py COM3 --chip-erase               # blank the chip and exit

Dependencies
------------
    pip install pyserial
"""

import argparse
import binascii
import csv
import sys
import time
import struct
from collections import deque
from typing import Optional

import serial

from logisim_parser import parse_words, to_lanes

# ---------------------------------------------------------------------------
# Protocol constants — must match protocol.h in the firmware
# ---------------------------------------------------------------------------
CMD_PING          = b'P'
CMD_GET_ID        = b'I'
CMD_CHIP_ERASE    = b'E'
CMD_SECTOR_ERASE  = b'S'
CMD_WRITE         = b'W'
CMD_READ          = b'R'
CMD_CRC           = b'C'

RESP_OK    = ord('K')
RESP_ERR   = ord('F')
RESP_READY = ord('Z')

SECTOR_SIZE  = 0x1000          # 4 KB per datasheet
CHUNK_SIZE   = 128             # bytes per write command (5 B header + 128 B must fit
                               # the firmware's 256 B RX ring while the previous
                               # chunk is still being programmed)
READ_CHUNK   = 4096            # bytes per read command (fallback verify only)
WRITE_WINDOW = 2               # write commands in flight before waiting for an ack
BAUD_RATE    = 1_000_000       # must match SERIAL_BAUD in protocol.h
BOOT_TIMEOUT = 5.0             # seconds to wait for RESP_READY after reset

# ---------------------------------------------------------------------------
# Known device IDs → (description, chip_size_in_bytes)
# ---------------------------------------------------------------------------
DEVICE_IDS: dict[tuple[int, int], tuple[str, int]] = {
    (0xBF, 0xB5): ("SST39SF010A (1 Mbit, 128 KB)", 0x020000),
    (0xBF, 0xB6): ("SST39SF020A (2 Mbit, 256 KB)", 0x040000),
    (0xBF, 0xB7): ("SST39SF040  (4 Mbit, 512 KB)", 0x080000),
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
        self.send_write(address, data)
        self._expect_ok()

    def send_write(self, address: int, data: bytes | bytearray):
        """Send a write command without waiting for its ack (see _expect_ok)."""
        assert 1 <= len(data) <= 256, "Chunk must be 1–256 bytes"
        # len=0 on the wire encodes 256
        wire_len = len(data) & 0xFF
        self._ser.write(CMD_WRITE + self._addr_bytes(address) + bytes([wire_len]) + bytes(data))

    def crc16(self, address: int, length: int) -> int:
        """CRC-16/XMODEM of *length* bytes computed on the device."""
        self._ser.timeout = 30
        self._ser.write(CMD_CRC + self._addr_bytes(address) + self._addr_bytes(length))
        hi, lo = self._recv(2)
        self._expect_ok()
        self._ser.timeout = 10
        return (hi << 8) | lo

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
        sectors = []
        for sector in range((len(image) + SECTOR_SIZE - 1) // SECTOR_SIZE):
            part = image[sector * SECTOR_SIZE:(sector + 1) * SECTOR_SIZE]
            if part.count(0xFF) != len(part):
                sectors.append(sector)
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
        """Write *image* to the chip, skipping 0xFF runs (chip must be erased).

        Up to WRITE_WINDOW write commands are kept in flight so the USB-serial
        round-trip latency overlaps with programming.
        """
        total   = len(image)
        written = 0
        pending = deque()          # sizes of unacknowledged chunks
        done    = 0
        t0      = time.monotonic()

        print(f"Programming {total:,} bytes...")
        for base in range(0, total, CHUNK_SIZE):
            chunk = image[base: base + CHUNK_SIZE]
            # Trim leading/trailing 0xFF — the chip is already blank there.
            lo = len(chunk) - len(chunk.lstrip(b"\xff"))
            hi = len(chunk.rstrip(b"\xff"))
            if hi > lo:
                self.send_write(base + lo, chunk[lo:hi])
                pending.append(len(chunk))
                written += hi - lo
            else:
                done += len(chunk)
            while len(pending) >= WRITE_WINDOW:
                self._expect_ok()
                done += pending.popleft()
            _progress(done, total, prefix="  Write ")
        while pending:
            self._expect_ok()
            done += pending.popleft()
        _progress(total, total, prefix="  Write ")
        dt = time.monotonic() - t0
        print(f"\n  {written:,} bytes written ({total - written:,} skipped as 0xFF) "
              f"in {dt:.1f} s ({written / dt / 1024 if dt else 0:.1f} KB/s).")

    def verify(self, image: bytearray) -> bool:
        """Verify the chip against *image*.

        Fast path: compare a CRC-16 computed on the device (no data crosses
        the serial link).  On mismatch, read everything back to locate the
        differing bytes.  Returns True if the chip matches.
        """
        total = len(image)
        print(f"Verifying {total:,} bytes...")
        t0 = time.monotonic()
        expected_crc = binascii.crc_hqx(bytes(image), 0)
        actual_crc = self.crc16(0, total)
        if actual_crc == expected_crc:
            print(f"  Verify OK (CRC-16 0x{actual_crc:04X}, {time.monotonic() - t0:.1f} s).")
            return True
        print(f"  CRC mismatch (expected 0x{expected_crc:04X}, got 0x{actual_crc:04X}) "
              f"— reading back to locate errors...")
        return self._verify_readback(image)

    def _verify_readback(self, image: bytearray) -> bool:
        total  = len(image)
        errors = 0
        addr   = 0
        while addr < total:
            length   = min(READ_CHUNK, total - addr)
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
            print("  Verify OK (CRC mismatch was spurious?) — readback matches.")
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
# Bit re-routing
# ---------------------------------------------------------------------------

def load_reroute(path: str, width: int) -> list[tuple[int, int]]:
    """
    Load a re-routing scheme from a CSV with a "SRC, DEST" header.

    Each row means: bit SRC of the word in the hex file is written as bit DEST.
    Bits are numbered from 0 (LSB).  Raises ValueError on out-of-range or
    duplicate destinations / sources.
    """
    pairs: list[tuple[int, int]] = []
    with open(path, newline="") as fh:
        for lineno, row in enumerate(csv.reader(fh), start=1):
            row = [c.strip() for c in row]
            if not row or not any(row):
                continue
            if lineno == 1 and not row[0].lstrip("-").isdigit():
                continue                      # header
            if len(row) < 2:
                raise ValueError(f"{path}:{lineno}: expected 'SRC, DEST'")
            src, dest = int(row[0], 0), int(row[1], 0)
            for name, v in (("SRC", src), ("DEST", dest)):
                if not 0 <= v < width:
                    raise ValueError(
                        f"{path}:{lineno}: {name} bit {v} out of range for "
                        f"{width}-bit words")
            pairs.append((src, dest))

    for idx, label in ((0, "SRC"), (1, "DEST")):
        seen = [p[idx] for p in pairs]
        dups = sorted({b for b in seen if seen.count(b) > 1})
        if dups:
            raise ValueError(f"{path}: duplicate {label} bit(s): {dups}")
    return pairs


def apply_reroute(words: dict[int, int], pairs: list[tuple[int, int]]) -> dict[int, int]:
    """Return a copy of *words* with each (src, dest) bit moved src -> dest.
    Destination bits not named in *pairs* are 0."""
    out: dict[int, int] = {}
    for addr, value in words.items():
        new = 0
        for src, dest in pairs:
            new |= ((value >> src) & 1) << dest
        out[addr] = new
    return out


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
                   help="Force the number of words (= bytes per chip) to program, e.g. 0x20000")
    p.add_argument("--reroute", metavar="CSV",
                   help="CSV of 'SRC, DEST' bit pairs: bit SRC of each word in the hex "
                        "file is written as bit DEST (applied before splitting into chips)")
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
    words, word_bytes = parse_words(args.file)
    if not words:
        print("ERROR: The hex file contains no data.")
        prog.close()
        sys.exit(1)

    # Number of addresses each chip must hold.  Every chip stores one byte
    # per word, so this is the same for all chips.
    depth = args.size if args.size is not None else max(words) + 1
    print(f"  {len(words):,} word(s) of {word_bytes * 8} bits, "
          f"address span 0x0 – 0x{depth - 1:X} ({depth:,} addresses).")

    # -- Check the address span fits on one chip -----------------------------
    if chip_size is None:
        print("WARNING: Unknown chip size — cannot check that the address span fits.")
    elif depth > chip_size:
        print(f"ERROR: The image needs {depth:,} addresses (0x0 – 0x{depth - 1:X}) "
              f"but {desc} only holds {chip_size:,} bytes (0x0 – 0x{chip_size - 1:X}).")
        print("       Nothing was programmed.")
        prog.close()
        sys.exit(1)

    # -- Bit re-routing ------------------------------------------------------
    if args.reroute:
        width = word_bytes * 8
        try:
            pairs = load_reroute(args.reroute, width)
        except (OSError, ValueError) as e:
            print(f"ERROR: {e}")
            prog.close()
            sys.exit(1)
        if len(pairs) != width:
            print(f"WARNING: scheme maps {len(pairs)} of {width} bits; "
                  f"unmapped destination bits will be 0.")
        words = apply_reroute(words, pairs)
        print(f"Applied bit re-routing from {args.reroute} ({len(pairs)} bit(s)).")

    # -- Split image into per-chip byte lanes --------------------------------
    # Each chip stores a single byte of every word, so a word_bytes-wide
    # image needs word_bytes chips.  Chip 1 gets the most significant byte,
    # the last chip the least significant; all are programmed at the same
    # addresses.  The user swaps the physical chip between lanes so the
    # programmer never needs to know which socket it is.
    slices = to_lanes(words, word_bytes, depth)
    n_chips = len(slices)
    if n_chips > 1:
        print(f"Data is {word_bytes * 8} bits wide — needs {n_chips} chips (one byte each).")

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

        hi_bit = (n_chips - chip_index) * 8 - 1
        print(f"\n[ Chip {chip_index + 1}/{n_chips} ]"
              f"  data bits {hi_bit}–{hi_bit - 7},"
              f" addresses 0x0 – 0x{len(chunk) - 1:X}")

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

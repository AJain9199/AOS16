"""
logisim_parser.py
-----------------
Parser for Logisim Evolution's "v3.0 hex words addressed" file format.

Format example
--------------
v3.0 hex words addressed
0: 3c 00 00 00 08 00 00 05
8: 00 00 00 00 00 00 00 00
1000: de ad be ef

Each line starts with a hex word-address (= byte address for x8 memories),
a colon, then space-separated hex byte values.  Lines may be omitted
entirely (gaps are treated as 0xFF, i.e. the erased state).
"""
import math

EXPECTED_HEADER = "v3.0 hex words addressed"


def parse(filepath: str) -> dict[int, int]:
    """
    Parse a Logisim v3.0 hex file.

    Returns a dict mapping byte_address → byte_value for every explicitly
    listed byte.  Gaps (omitted addresses) are NOT included; callers that
    need a flat array should use to_bytearray().

    Word width is detected from the length of the first data token:
      2 hex chars → 8-bit  words (1 byte  each, byte_addr = word_addr)
      4 hex chars → 16-bit words (2 bytes each, big-endian)
      8 hex chars → 32-bit words (4 bytes each, big-endian)

    Raises
    ------
    ValueError  if the file header is not recognised or a token length is
                inconsistent with the detected word width.
    """
    data: dict[int, int] = {}
    word_bytes: int | None = None   # bytes per word, detected from first token

    with open(filepath, "r") as fh:
        header = fh.readline().strip()
        if header != EXPECTED_HEADER:
            raise ValueError(
                f"Unrecognised header: {header!r}\n"
                f"Expected: {EXPECTED_HEADER!r}"
            )

        for lineno, line in enumerate(fh, start=2):
            line = line.strip()
            if not line or line.startswith("#"):
                continue

            colon = line.index(":")
            word_addr = int(line[:colon], 16)

            for word_offset, token in enumerate(line[colon + 1:].split()):
                # Detect word width from the very first token
                if word_bytes is None:
                    token_chars = len(token)
                    word_bytes = math.ceil(token_chars / 2)

                value = int(token, 16)

                # Unpack word into bytes, big-endian, at consecutive byte addresses
                byte_base = (word_addr + word_offset) * word_bytes
                for byte_offset in range(word_bytes):
                    shift = (word_bytes - 1 - byte_offset) * 8
                    data[byte_base + byte_offset] = (value >> shift) & 0xFF

    return data


def to_bytearray(data: dict[int, int], size: int | None = None) -> bytearray:
    """
    Convert an address→byte dict to a flat bytearray.

    Gaps and bytes not present in *data* are filled with 0xFF (erased state).
    If *size* is not given it defaults to max_address + 1.
    """
    if not data:
        return bytearray()

    max_addr = max(data.keys())
    buf_size = size if size is not None else max_addr + 1
    buf = bytearray(b"\xff" * buf_size)

    for addr, value in data.items():
        if addr < buf_size:
            buf[addr] = value

    return buf


def load(filepath: str, size: int | None = None) -> bytearray:
    """Convenience wrapper: parse + convert in one call."""
    return to_bytearray(parse(filepath), size)


# ---------------------------------------------------------------------------
# Quick self-test when run directly
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: logisim_parser.py <file.hex>")
        sys.exit(1)

    result = load(sys.argv[1])
    non_ff = sum(1 for b in result if b != 0xFF)
    print(f"Parsed {len(result)} bytes, {non_ff} non-0xFF bytes.")
    print(f"First 16 bytes: {result[:16].hex(' ')}")
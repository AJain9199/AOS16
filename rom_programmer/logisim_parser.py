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


def parse_words(filepath: str) -> tuple[dict[int, int], int]:
    """
    Parse a Logisim v3.0 hex file without splitting words into bytes.

    Returns (words, word_bytes) where *words* maps word_address → word_value
    for every explicitly listed word and *word_bytes* is the number of bytes
    per word.  Gaps (omitted addresses) are NOT included.

    Word width is detected from the length of the first data token:
      2 hex chars → 8-bit  words (1 byte)
      4 hex chars → 16-bit words (2 bytes)
      8 hex chars → 32-bit words (4 bytes)

    Raises
    ------
    ValueError  if the file header is not recognised.
    """
    words: dict[int, int] = {}
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
                    word_bytes = math.ceil(len(token) / 2)
                words[word_addr + word_offset] = int(token, 16)

    return words, word_bytes or 1


def to_lanes(words: dict[int, int], word_bytes: int,
             depth: int | None = None) -> list[bytearray]:
    """
    Split word data into one byte image per byte lane (one per flash chip).

    Lane 0 holds the most significant byte of every word, lane word_bytes-1
    the least significant.  Every lane is indexed by word address, so each
    chip is programmed at the same addresses the CPU will present to it.

    Gaps are filled with 0xFF (erased state).  *depth* is the length of each
    lane in bytes; it defaults to max_word_address + 1.  Words at or beyond
    *depth* are dropped.
    """
    if depth is None:
        depth = max(words) + 1 if words else 0

    lanes = [bytearray(b"\xff") * depth for _ in range(word_bytes)]
    for addr, value in words.items():
        if addr >= depth:
            continue
        for lane in range(word_bytes):
            shift = (word_bytes - 1 - lane) * 8
            lanes[lane][addr] = (value >> shift) & 0xFF
    return lanes


def parse(filepath: str) -> dict[int, int]:
    """
    Parse a Logisim v3.0 hex file.

    Returns a dict mapping byte_address → byte_value for every explicitly
    listed byte.  Gaps (omitted addresses) are NOT included; callers that
    need a flat array should use to_bytearray().

    Words are unpacked big-endian at consecutive byte addresses; see
    parse_words() for how word width is detected.
    """
    words, word_bytes = parse_words(filepath)
    data: dict[int, int] = {}
    for word_addr, value in words.items():
        byte_base = word_addr * word_bytes
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
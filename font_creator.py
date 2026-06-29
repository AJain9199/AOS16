#!/usr/bin/env python3
"""
8x5 bitmap font creator for AOS16.

Click/drag to paint pixels. Right-click/drag to erase.
Packs 3 rows per uint16 (little-endian):
  bit15=0  bits[14:10]=row_C  bits[9:5]=row_B  bits[4:0]=row_A
  → 3 uint16s = 6 bytes per character

Starts at ASCII 0x20 (space), ends at 0x7E (~).
Loads existing font.bin on startup if present.
Output: font.bin
"""

import os
import struct
import tkinter as tk

FONT_W        = 5
FONT_H        = 8
START_ASCII   = 0x20
END_ASCII     = 0x7E
TOTAL_CHARS   = END_ASCII - START_ASCII + 1
BYTES_PER_CHAR = 6   # 3 × uint16 LE
OUTPUT_FILE   = os.path.join(os.path.dirname(os.path.abspath(__file__)), "font.bin")

CELL          = 72
PREVIEW_CEL   = 8
PAD           = 16

COLOR_SET     = "#1a1a1a"
COLOR_CLR     = "#f5f5f5"
COLOR_GRID    = "#bbbbbb"
COLOR_BG      = "#2b2b2b"
COLOR_PANEL   = "#3c3c3c"
COLOR_ACCENT  = "#4e9ef5"
COLOR_VISITED = "#555555"


def _row_bits(row):
    val = 0
    for bit in row:
        val = (val << 1) | bit
    return val


def pack_char(pixels):
    """pixels: list[list[int]] 8 rows × 5 cols. Returns 6 bytes (3 × uint16 LE).
    word0: bit15=0, bits[14:10]=row2, bits[9:5]=row1,  bits[4:0]=row0
    word1: bit15=0, bits[14:10]=row5, bits[9:5]=row4,  bits[4:0]=row3
    word2: bit15=0, bits[14:10]=0,    bits[9:5]=row7,  bits[4:0]=row6
    """
    rows = [_row_bits(r) for r in pixels]
    word0 = (rows[2] << 10) | (rows[1] << 5) | rows[0]
    word1 = (rows[5] << 10) | (rows[4] << 5) | rows[3]
    word2 =                   (rows[7] << 5)  | rows[6]   # bits[14:10] = 0
    return struct.pack("<3H", word0, word1, word2)


def unpack_char(data):
    """Inverse of pack_char. Returns list of FONT_H row-bit-values (row0 first)."""
    word0, word1, word2 = struct.unpack_from("<3H", data)
    return [
        word0        & 0x1F,   # row0
        (word0 >> 5) & 0x1F,   # row1
        (word0 >>10) & 0x1F,   # row2
        word1        & 0x1F,   # row3
        (word1 >> 5) & 0x1F,   # row4
        (word1 >>10) & 0x1F,   # row5
        word2        & 0x1F,   # row6
        (word2 >> 5) & 0x1F,   # row7
    ]


def bits_to_pixels(rows):
    """Convert list of row-bit-values to 2D pixel array."""
    return [[(row_val >> (FONT_W - 1 - x)) & 1 for x in range(FONT_W)]
            for row_val in rows]


class FontCreator(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("AOS16 Font Creator")
        self.configure(bg=COLOR_BG)
        self.resizable(False, False)

        self.char_index = 0
        self.font_data  = bytearray(TOTAL_CHARS * BYTES_PER_CHAR)
        self.max_index  = 0   # furthest char ever reached
        self._paint_val = None

        self._load_existing_file()

        self.pixels = bits_to_pixels(unpack_char(
            self.font_data[0:BYTES_PER_CHAR]))

        self._build_ui()
        self._update_header()
        self._redraw_grid()
        self._redraw_preview_strip()

        self.bind("<Key-Return>",    lambda e: self._next())
        self.bind("<Key-space>",     lambda e: self._skip())
        self.bind("<Key-z>",         lambda e: self._clear_grid())
        self.bind("<Left>",          lambda e: self._back())
        self.bind("<BackSpace>",     lambda e: self._back())

    def _load_existing_file(self):
        if not os.path.isfile(OUTPUT_FILE):
            return
        size = os.path.getsize(OUTPUT_FILE)
        expected = TOTAL_CHARS * BYTES_PER_CHAR
        with open(OUTPUT_FILE, "rb") as f:
            data = f.read(expected)
        self.font_data[:len(data)] = data
        # find the last non-zero character to set max_index
        for i in range(TOTAL_CHARS - 1, -1, -1):
            if any(self.font_data[i*BYTES_PER_CHAR:(i+1)*BYTES_PER_CHAR]):
                self.max_index = i
                break
        print(f"Loaded {OUTPUT_FILE}  ({size} bytes, resuming from char 0x{START_ASCII:02X})")

    # ── UI construction ───────────────────────────────────────────────────────

    def _build_ui(self):
        left = tk.Frame(self, bg=COLOR_BG, padx=PAD, pady=PAD)
        left.grid(row=0, column=0, sticky="nsew")

        self.header_var = tk.StringVar()
        tk.Label(left, textvariable=self.header_var,
                 font=("Courier", 18, "bold"), fg=COLOR_ACCENT, bg=COLOR_BG)\
            .grid(row=0, column=0, columnspan=2, pady=(0, PAD//2))

        self.progress_var = tk.StringVar()
        tk.Label(left, textvariable=self.progress_var,
                 font=("Courier", 10), fg="#888888", bg=COLOR_BG)\
            .grid(row=1, column=0, columnspan=2, pady=(0, PAD))

        cw = FONT_W * CELL
        ch = FONT_H * CELL
        self.canvas = tk.Canvas(left, width=cw, height=ch,
                                bg=COLOR_CLR, highlightthickness=0, cursor="crosshair")
        self.canvas.grid(row=2, column=0, columnspan=2)

        self.canvas.bind("<Button-1>",        self._mouse_down_left)
        self.canvas.bind("<B1-Motion>",       self._mouse_drag_left)
        self.canvas.bind("<Button-3>",        self._mouse_down_right)
        self.canvas.bind("<B3-Motion>",       self._mouse_drag_right)
        self.canvas.bind("<ButtonRelease-1>", lambda e: self._end_paint())
        self.canvas.bind("<ButtonRelease-3>", lambda e: self._end_paint())

        self.bytes_var = tk.StringVar(value="0000  0000  0000")
        tk.Label(left, textvariable=self.bytes_var,
                 font=("Courier", 12), fg="#aaaaaa", bg=COLOR_BG)\
            .grid(row=3, column=0, columnspan=2, pady=(PAD//2, 0))

        btn_frame = tk.Frame(left, bg=COLOR_BG)
        btn_frame.grid(row=4, column=0, columnspan=2, pady=(PAD, 0))

        def btn(parent, text, cmd, color="#555555", key=""):
            label = f"{text}  [{key}]" if key else text
            return tk.Button(parent, text=label, command=cmd,
                             font=("Courier", 11, "bold"),
                             bg=color, fg="white", relief="flat",
                             padx=14, pady=6, cursor="hand2",
                             activebackground=color, activeforeground="white")

        btn(btn_frame, "← Back",    self._back,       "#666666",    "←"    ).pack(side="left", padx=4)
        btn(btn_frame, "Next →",    self._next,       COLOR_ACCENT, "Enter").pack(side="left", padx=4)
        btn(btn_frame, "Skip",      self._skip,       "#666666",    "Space").pack(side="left", padx=4)
        btn(btn_frame, "Clear",     self._clear_grid, "#884444",    "Z"    ).pack(side="left", padx=4)
        btn(btn_frame, "Quit+Save", self._quit_save,  "#555555"            ).pack(side="left", padx=4)

        tk.Label(left, text="Left-click: paint  ·  Right-click: erase  ·  Drag to fill",
                 font=("Courier", 9), fg="#666666", bg=COLOR_BG)\
            .grid(row=5, column=0, columnspan=2, pady=(6, 0))

        right = tk.Frame(self, bg=COLOR_BG, padx=PAD//2, pady=PAD)
        right.grid(row=0, column=1, sticky="ns")

        tk.Label(right, text="Font", font=("Courier", 9), fg="#666666", bg=COLOR_BG)\
            .pack()

        strip_h = TOTAL_CHARS * (FONT_H * PREVIEW_CEL + 2) + 4
        strip_w = FONT_W * PREVIEW_CEL + 24

        strip_frame = tk.Frame(right, bg=COLOR_BG)
        strip_frame.pack(fill="y", expand=True)

        scrollbar = tk.Scrollbar(strip_frame, orient="vertical")
        self.strip = tk.Canvas(strip_frame,
                               width=strip_w,
                               height=min(strip_h, 600),
                               bg=COLOR_BG, highlightthickness=0,
                               yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.strip.yview)
        self.strip.pack(side="left")
        scrollbar.pack(side="left", fill="y")
        self.strip.config(scrollregion=(0, 0, strip_w, strip_h))

        self.strip.bind("<Button-1>", self._strip_click)

    # ── painting ─────────────────────────────────────────────────────────────

    def _cell(self, event):
        x = event.x // CELL
        y = event.y // CELL
        if 0 <= x < FONT_W and 0 <= y < FONT_H:
            return x, y
        return None, None

    def _mouse_down_left(self, event):
        x, y = self._cell(event)
        if x is not None:
            self._paint_val = 1 - self.pixels[y][x]
            self._set_pixel(x, y, self._paint_val)

    def _mouse_drag_left(self, event):
        x, y = self._cell(event)
        if x is not None and self._paint_val is not None:
            self._set_pixel(x, y, self._paint_val)

    def _mouse_down_right(self, event):
        x, y = self._cell(event)
        if x is not None:
            self._paint_val = 0
            self._set_pixel(x, y, 0)

    def _mouse_drag_right(self, event):
        x, y = self._cell(event)
        if x is not None:
            self._set_pixel(x, y, 0)

    def _end_paint(self):
        self._paint_val = None

    def _set_pixel(self, x, y, val):
        if self.pixels[y][x] == val:
            return
        self.pixels[y][x] = val
        self._draw_cell(x, y)
        self._update_bytes()

    def _clear_grid(self):
        self.pixels = [[0]*FONT_W for _ in range(FONT_H)]
        self._redraw_grid()
        self._update_bytes()

    # ── drawing ───────────────────────────────────────────────────────────────

    def _draw_cell(self, x, y):
        x0 = x * CELL
        y0 = y * CELL
        color = COLOR_SET if self.pixels[y][x] else COLOR_CLR
        tag = f"cell_{x}_{y}"
        self.canvas.delete(tag)
        self.canvas.create_rectangle(x0+1, y0+1, x0+CELL-1, y0+CELL-1,
                                     fill=color, outline="", tags=tag)

    def _redraw_grid(self):
        self.canvas.delete("all")
        for y in range(FONT_H):
            for x in range(FONT_W):
                self._draw_cell(x, y)
        for x in range(FONT_W + 1):
            self.canvas.create_line(x*CELL, 0, x*CELL, FONT_H*CELL, fill=COLOR_GRID, width=1)
        for y in range(FONT_H + 1):
            self.canvas.create_line(0, y*CELL, FONT_W*CELL, y*CELL, fill=COLOR_GRID, width=1)
        self._update_bytes()

    def _update_bytes(self):
        packed = pack_char(self.pixels)
        words = struct.unpack_from("<3H", packed)
        self.bytes_var.set("→ " + "  ".join(f"{w:04X}" for w in words))

    def _update_header(self):
        char_code = START_ASCII + self.char_index
        c = chr(char_code)
        display = repr(c) if char_code <= 0x20 else f"'{c}'"
        self.header_var.set(f"{display}   0x{char_code:02X}")
        self.progress_var.set(f"{self.char_index + 1} / {TOTAL_CHARS}")

    def _redraw_preview_strip(self):
        self.strip.delete("all")
        x_off = 18
        for idx in range(TOTAL_CHARS):
            char_code = START_ASCII + idx
            data = self.font_data[idx*BYTES_PER_CHAR:(idx+1)*BYTES_PER_CHAR]
            rows = unpack_char(data)

            label = repr(chr(char_code)) if char_code <= 0x20 else chr(char_code)
            y_off = idx * (FONT_H * PREVIEW_CEL + 2) + 2

            is_current = (idx == self.char_index)
            label_color = COLOR_ACCENT if is_current else (
                "#888888" if idx <= self.max_index else "#444444")

            self.strip.create_text(2, y_off + FONT_H * PREVIEW_CEL // 2,
                                   text=label, anchor="w",
                                   font=("Courier", 7), fill=label_color)

            for ry in range(FONT_H):
                row_val = rows[ry]
                for rx in range(FONT_W):
                    bit = (row_val >> (FONT_W - 1 - rx)) & 1
                    if is_current:
                        color = COLOR_ACCENT if bit else "#1a2a3a"
                    elif idx <= self.max_index:
                        color = COLOR_SET if bit else COLOR_VISITED
                    else:
                        color = "#333333"
                    self.strip.create_rectangle(
                        x_off + rx * PREVIEW_CEL,
                        y_off + ry * PREVIEW_CEL,
                        x_off + rx * PREVIEW_CEL + PREVIEW_CEL - 1,
                        y_off + ry * PREVIEW_CEL + PREVIEW_CEL - 1,
                        fill=color, outline=""
                    )

        # scroll to keep current character visible
        frac = self.char_index / TOTAL_CHARS
        self.strip.yview_moveto(max(0.0, frac - 0.1))

    # ── strip click to jump to character ─────────────────────────────────────

    def _strip_click(self, event):
        y = int(self.strip.canvasy(event.y))
        row_h = FONT_H * PREVIEW_CEL + 2
        idx = y // row_h
        if 0 <= idx < TOTAL_CHARS:
            self._commit_current()
            self.char_index = idx
            self.max_index = max(self.max_index, idx)
            self._load_current()

    # ── data management ───────────────────────────────────────────────────────

    def _commit_current(self):
        packed = pack_char(self.pixels)
        offset = self.char_index * BYTES_PER_CHAR
        self.font_data[offset:offset + BYTES_PER_CHAR] = packed

    def _load_current(self):
        offset = self.char_index * BYTES_PER_CHAR
        self.pixels = bits_to_pixels(
            unpack_char(self.font_data[offset:offset + BYTES_PER_CHAR]))
        self._redraw_grid()
        self._update_header()
        self._redraw_preview_strip()

    # ── navigation ────────────────────────────────────────────────────────────

    def _next(self):
        self._commit_current()
        if self.char_index < TOTAL_CHARS - 1:
            self.char_index += 1
            self.max_index = max(self.max_index, self.char_index)
            self._load_current()
        else:
            self._save_and_exit()

    def _back(self):
        if self.char_index == 0:
            return
        self._commit_current()
        self.char_index -= 1
        self._load_current()

    def _skip(self):
        offset = self.char_index * BYTES_PER_CHAR
        self.font_data[offset:offset + BYTES_PER_CHAR] = b"\x00" * BYTES_PER_CHAR
        if self.char_index < TOTAL_CHARS - 1:
            self.char_index += 1
            self.max_index = max(self.max_index, self.char_index)
            self._load_current()
        else:
            self._save_and_exit()

    def _quit_save(self):
        self._commit_current()
        self._save_and_exit()

    def _save_and_exit(self):
        with open(OUTPUT_FILE, "wb") as f:
            f.write(self.font_data)
        print(f"Saved {OUTPUT_FILE}  ({TOTAL_CHARS} chars, {len(self.font_data)} bytes)")
        self.destroy()


if __name__ == "__main__":
    app = FontCreator()
    app.mainloop()

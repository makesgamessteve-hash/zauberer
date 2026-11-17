# SPDX-License-Identifier: MIT
# REUSE-IgnoreStart
"""Generates the base game palette and the retro renderer's lookup data
(palettes/base.png, base.lua, base_lut.png and bayer8.png).

The palette is 64 hand-curated colours in ramps (dark to light) per hue
family: the Oriental accents lapis, turquoise, saffron, pomegranate and ochre,
plus the families natural terrain and sky need. Every colour is snapped to the
5-bit grid of the lookup table, value v -> (v * 255 + 15) // 31, so each
palette colour sits exactly on a LUT cell and maps to itself.

Nearest colours are found in Oklab, computed in integer fixed-point
arithmetic only (sRGB transfer table from the decimal module, exact integer
cube roots), so the output is byte-identical on every platform and Python
version. Ties go to the lower palette index."""
import decimal
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pngw import write_png  # noqa: E402

# Hue families, each a ramp from dark to light. The order here is the palette
# order (1-based indices in base.lua count through these lists).
RAMPS = [
    ("night", ["0a0912", "171630", "25244a", "37376a"]),
    ("stone", ["2d2b2b", "464341", "615d58", "7e7972", "9e9890", "c0bab0"]),
    ("snow", ["a1b2c6", "c8d4e0", "e6ecf1", "ffffff"]),
    ("lapis", ["141a56", "22308c", "3448bb", "5872dc"]),
    ("sky", ["4f86c6", "77a9df", "a2c9ee", "cfe4f7"]),
    ("water", ["0e2b3f", "16455e", "21627a", "348299"]),
    ("turquoise", ["146e6c", "23968e", "43bcaa", "86dccb"]),
    ("green", ["1a3319", "294f23", "3b6c2d", "558a39", "7aa94c"]),
    ("olive", ["44441f", "66652b", "888439", "aca556"]),
    ("brown", ["291b13", "412b1e", "5c3f2a", "79573a", "98734f"]),
    ("ochre", ["754a1a", "9c6624", "c08633", "d9a752"]),
    ("sand", ["a88d62", "c4a979", "dcc596", "efe0b9"]),
    ("saffron", ["c56c15", "e48c1c", "f3b12b", "fad468"]),
    ("pomegranate", ["4c0d1b", "761426", "a2202e", "c9423c", "e4785f"]),
    ("violet", ["3d2160", "673b96", "9c72cf"]),
]

LUT_BITS = 5
LUT_SIZE = 1 << LUT_BITS  # 32 levels per channel
LUT_MAX = LUT_SIZE - 1


def grid_value(level):
    """8-bit value of a 5-bit LUT level."""
    return (level * 255 + 15) // LUT_MAX


def grid_level(value):
    """Nearest 5-bit LUT level of an 8-bit value."""
    return (value * LUT_MAX + 127) // 255


def snap(rgb):
    return tuple(grid_value(grid_level(c)) for c in rgb)


def _parse_hex(text):
    return (int(text[0:2], 16), int(text[2:4], 16), int(text[4:6], 16))


def _build_palette():
    colors, ramps, index = [], [], 1
    for name, hexes in RAMPS:
        ramp = []
        for text in hexes:
            colors.append(snap(_parse_hex(text)))
            ramp.append(index)
            index += 1
        ramps.append((name, ramp))
    return colors, ramps


PALETTE, RAMP_INDICES = _build_palette()  # PALETTE[i - 1] is colour i
assert len(PALETTE) == 64
_RAMP_BY_NAME = dict(RAMP_INDICES)


def ramp_color(ramp, step):
    """0-based palette index of the step-th (1-based, dark to light) colour of a ramp."""
    return _RAMP_BY_NAME[ramp][step - 1] - 1


# --- Oklab in integer fixed point -------------------------------------------

_LIN_BITS = 24  # linear light scaled by 2^24


def _linear_table():
    """sRGB 8-bit value -> linear light in 2^24 fixed point (decimal module,
    platform-independent arbitrary precision)."""
    ctx = decimal.Context(prec=40, rounding=decimal.ROUND_HALF_EVEN)
    scale = decimal.Decimal(1 << _LIN_BITS)
    table = []
    for v in range(256):
        c = ctx.divide(decimal.Decimal(v), decimal.Decimal(255))
        if c <= decimal.Decimal("0.04045"):
            lin = ctx.divide(c, decimal.Decimal("12.92"))
        else:
            base = ctx.divide(ctx.add(c, decimal.Decimal("0.055")), decimal.Decimal("1.055"))
            lin = ctx.power(base, decimal.Decimal("2.4"))
        table.append(int(ctx.multiply(lin, scale).to_integral_value(rounding=decimal.ROUND_HALF_EVEN)))
    assert table[0] == 0 and table[255] == 1 << _LIN_BITS and all(a < b for a, b in zip(table, table[1:]))
    return table


LINEAR = _linear_table()

# Oklab matrices (Björn Ottosson, 2020) with coefficients scaled by 10^10.
_M1 = ((4122214708, 5363325363, 514459929),
       (2119034982, 6806995451, 1073969566),
       (883024619, 2817188376, 6299787005))
_M2 = ((2104542553, 7936177850, -40720468),
       (19779984951, -24285922050, 4505937099),
       (259040371, 7827717662, -8086757660))
_E10 = 10 ** 10
_CBRT_BITS = 32  # cube roots in 2^32 fixed point
_OUT_BITS = 24  # Oklab components in 2^24 fixed point (L = 1.0 -> 2^24)


def icbrt(n):
    """Floor of the real cube root of a non-negative integer (exact)."""
    if n <= 0:
        return 0
    x = int(round(float(n) ** (1.0 / 3.0)))  # only a starting guess
    while x * x * x > n:
        x -= 1
    while (x + 1) * (x + 1) * (x + 1) <= n:
        x += 1
    return x


def oklab(rgb):
    """Oklab (L, a, b) of an 8-bit sRGB colour as integers, 1.0 = 2^24."""
    lin = [LINEAR[c] for c in rgb]
    roots = []
    for row in _M1:
        lms = row[0] * lin[0] + row[1] * lin[1] + row[2] * lin[2]  # scale 1e10 * 2^24
        # cbrt(lms / (1e10 * 2^24)) * 2^32 = cbrt(lms * 2^72 / 1e10)
        roots.append(icbrt((lms << (3 * _CBRT_BITS - _LIN_BITS)) // _E10))
    out = []
    for row in _M2:
        value = row[0] * roots[0] + row[1] * roots[1] + row[2] * roots[2]  # scale 1e10 * 2^32
        out.append(value // (_E10 << (_CBRT_BITS - _OUT_BITS)))
    return tuple(out)


PALETTE_LAB = [oklab(c) for c in PALETTE]


def distance2(lab1, lab2):
    """Squared Euclidean Oklab distance (integer)."""
    dl, da, db = lab1[0] - lab2[0], lab1[1] - lab2[1], lab1[2] - lab2[2]
    return dl * dl + da * da + db * db


def nearest(rgb):
    """0-based index of the nearest palette colour (Oklab; ties -> lowest index)."""
    lab = oklab(rgb)
    best, best_d = -1, None
    for i in range(len(PALETTE)):
        d = distance2(lab, PALETTE_LAB[i])
        if best_d is None or d < best_d:
            best, best_d = i, d
    return best


def lut_rows():
    """The 32x32x32 lookup table as 32 rows of 1024 RGBA pixels: pixel (x, y)
    is colour (r = x % 32, g = y, b = x // 32) on the 5-bit grid."""
    levels = [grid_value(v) for v in range(LUT_SIZE)]
    rows = []
    for g in range(LUT_SIZE):
        row = []
        for b in range(LUT_SIZE):
            for r in range(LUT_SIZE):
                color = PALETTE[nearest((levels[r], levels[g], levels[b]))]
                row.append(color + (255,))
        rows.append(row)
    return rows


def bayer(n):
    """The standard n x n Bayer ordered-dither matrix (n a power of two)."""
    m = [[0]]
    while len(m) < n:
        size = len(m)
        m = ([[4 * m[y][x] for x in range(size)] + [4 * m[y][x] + 2 for x in range(size)] for y in range(size)]
             + [[4 * m[y][x] + 3 for x in range(size)] + [4 * m[y][x] + 1 for x in range(size)]
                for y in range(size)])
    return m


def write_lua(path, generator, lines):
    """Writes a generated Lua data file with the license and "do not edit" header."""
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("-- SPDX-License-Identifier: CC-BY-4.0\n")
        f.write("-- Generated by tools/gen/%s. Do not edit.\n" % generator)
        for line in lines:
            f.write(line + "\n")


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    write_png(os.path.join(out_dir, "base.png"), len(PALETTE), 1, [[c + (255,) for c in PALETTE]])
    write_png(os.path.join(out_dir, "base_lut.png"), LUT_SIZE * LUT_SIZE, LUT_SIZE, lut_rows())
    matrix = bayer(8)
    write_png(os.path.join(out_dir, "bayer8.png"), 8, 8,
              [[(v * 4 + 2,) * 3 + (255,) for v in row] for row in matrix])
    lines = ["return {",
             '  texture = "palettes/base.png",',
             '  lut = "palettes/base_lut.png",',
             "  lut_size = %d," % LUT_SIZE,
             '  dither = "palettes/bayer8.png",',
             "  colors = %d," % len(PALETTE),
             "  -- Ramps run from dark to light; values are 1-based palette indices.",
             "  ramps = {"]
    for name, indices in RAMP_INDICES:
        lines.append("    %s = { %s }," % (name, ", ".join(str(i) for i in indices)))
    lines += ["  },", "}"]
    write_lua(os.path.join(out_dir, "base.lua"), "gen_palette.py", lines)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base/palettes")
# REUSE-IgnoreEnd

# SPDX-License-Identifier: MIT
"""Generates the tile ornaments (textures/ornaments/*.png): geometric star
patterns, arabesques, carpet borders and kilim motifs as pixel art that uses
only base palette colours.

Every ornament is a function of pixel position on a torus of its own size
(coordinates wrap), so the tiles repeat seamlessly along the axes listed in
ORNAMENTS. Geometry is integer-only: positions are doubled so pixel centres
are odd integers, and circles and diagonals compare squared integers."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from gen_palette import PALETTE, ramp_color as c  # noqa: E402
from pngw import write_png  # noqa: E402

CREAM = c("sand", 4)
DARK = c("night", 1)


def _centered(n, size):
    """Doubled offset of pixel n from the tile centre (odd, -size+1..size-1)."""
    return 2 * n + 1 - size


# --- Eight-pointed star (star and cross pattern) ---------------------------

def _star(u, v, r):
    """Inside an eight-pointed star of circumradius r (doubled units): the
    union of a square and a diamond with equal circumradius."""
    au, av = abs(u), abs(v)
    return 2 * max(au, av) ** 2 <= r * r or au + av <= r


def star8(x, y):
    u, v = _centered(x, 32), _centered(y, 32)
    # Stars of circumradius 16 px touch tip to tip at the tile edges; the
    # space between four stars forms the cross around each tile corner.
    if not _star(u, v, 32):
        cu, cv = 32 - abs(u), 32 - abs(v)  # doubled offset from the nearest corner
        if cu + cv <= 6:
            return c("saffron", 3)
        if cu + cv <= 10:
            return c("pomegranate", 3)
        return c("turquoise", 3)
    if not _star(u, v, 28):
        return CREAM
    if not _star(u, v, 16):
        return c("lapis", 2)
    if not _star(u, v, 12):
        return c("saffron", 3)
    if abs(u) + abs(v) <= 4:
        return c("pomegranate", 3)
    return c("lapis", 3)


# --- Kilim lozenge ----------------------------------------------------------

def kilim(x, y):
    # Hooked lozenges in 2 x 2 texel steps, the way flat-weave slit tapestry
    # draws diagonals. A large lozenge sits on the tile centre and a small one
    # on every tile corner, so the tiles build a lattice of alternating
    # motifs on a pomegranate field; teeth ("hooks") stand on both outlines.
    ax, ay = abs(_centered(x, 32)) // 4, abs(_centered(y, 32)) // 4  # 0..7 steps
    d = ax + ay  # steps from the tile centre
    e = 14 - d  # steps from the nearest tile corner
    if d <= 1:
        return c("saffron", 3)
    if d == 2:
        return c("pomegranate", 4)
    if d == 3:
        return CREAM
    if d == 4:
        return c("lapis", 2)
    if d == 5:
        return DARK
    if e <= 1:
        return c("lapis", 2)
    if e == 2:
        return c("ochre", 3)
    if e == 3:
        return DARK
    if (d == 6 or e == 4) and (ax - ay) % 4 == 0:
        return DARK  # hook teeth on the outlines
    if d == 8 and ax % 2 == 1 and ay % 2 == 1:
        return c("ochre", 3)  # small field dots between the motifs
    return c("pomegranate", 2)


# --- Arabesque --------------------------------------------------------------

_ARABESQUE_SIZE = 64
_STEM_R, _CURL_STEP = 8, 3  # lobe radius and spiral step in texels


def _wrap(delta, span):
    """Shortest signed delta on a torus of span units."""
    return (delta + span // 2) % span - span // 2


def _arabesque_arcs():
    """Semicircles (centre x, centre y, radius, side) in doubled units, side
    -1 = upper half, +1 = lower half. Two rows of a wave stem (lobes of
    radius 8 texels, 16 texels apart); every lobe curls into a spiral that
    branches off where it meets the next lobe."""
    arcs = []
    for cy, shift in ((16, 0), (48, 16)):
        for k in range(4):
            cx = 8 + 16 * k + shift
            side = -1 if k % 2 == 0 else 1
            radius, j = _STEM_R, 0
            while radius >= 2:
                ox = 0 if j % 2 == 0 else _CURL_STEP
                arcs.append((2 * (cx + ox), 2 * cy, 2 * radius, side if j % 2 == 0 else -side))
                radius -= _CURL_STEP
                j += 1
    return arcs


_ARCS = _arabesque_arcs()
# Rosettes in the gaps between the rows; leaf pairs where lobes face each other.
_ROSETTES = [(2 * (8 + 32 * m), 2 * 32) for m in range(2)] + [(2 * (24 + 32 * m), 0) for m in range(2)]
_LEAVES = [(2 * (8 + 32 * m), 0) for m in range(2)] + [(2 * (24 + 32 * m), 2 * 32) for m in range(2)]


def arabesque(x, y):
    span = 2 * _ARABESQUE_SIZE
    px, py = 2 * x + 1, 2 * y + 1
    for cx, cy in _ROSETTES:
        dx, dy = abs(_wrap(px - cx, span)), abs(_wrap(py - cy, span))
        # Eight-petal rosette: red heart, cream ring, saffron petals on the
        # axes and turquoise petals on the diagonals.
        if dx + dy <= 2:
            return c("pomegranate", 3)
        if dx * dx + dy * dy <= 20:
            return CREAM
        if (dx <= 1 or dy <= 1) and dx + dy <= 14:
            return c("saffron", 3)
        if abs(dx - dy) <= 2 and dx + dy <= 12:
            return c("turquoise", 4)
    for cx, cy in _LEAVES:
        dx, dy = abs(_wrap(px - cx, span)), abs(_wrap(py - cy, span))
        # Two almond leaves pointing at each other (vesica shapes along y).
        if 4 <= dy <= 16:
            ly = dy - 10
            if (dx + 6) ** 2 + ly * ly <= 100 and (dx - 6) ** 2 + ly * ly <= 100:
                return c("turquoise", 4) if dx <= 1 else c("turquoise", 3)
    for cx, cy, r, side in _ARCS:
        dx, dy = _wrap(px - cx, span), _wrap(py - cy, span)
        if dy * side < 0:
            continue
        d2 = dx * dx + dy * dy
        if (r - 2) ** 2 <= d2 <= (r + 2) ** 2:
            return c("saffron", 3)
    return c("lapis", 1)


# --- Carpet border ----------------------------------------------------------

def carpet_border(x, y):
    # Rows mirror around the middle: outline, guard stripe, outline, then the
    # main band (10 rows) with stepped medallions every 16 texels and small
    # diamonds between them.
    row = min(y, 15 - y)
    if row == 0 or row == 2:
        return DARK
    if row == 1:
        return c("pomegranate", 3) if x % 4 == 1 else CREAM
    au, av = abs(_centered(x % 16, 16)), abs(_centered(y, 16))
    d = au // 4 + av // 4  # stepped distance from the medallion centre
    if d == 0:
        return c("pomegranate", 3)
    if d == 1:
        return c("saffron", 3)
    if d == 2:
        return DARK
    e = (16 - au) // 4 + av // 4  # stepped distance from the cell border
    if e == 0:
        return c("turquoise", 3)
    if e == 1:
        return CREAM
    return c("lapis", 2)


ORNAMENTS = [
    # (file name, width, height, axes the tile repeats along, function)
    ("star8.png", 32, 32, "xy", star8),
    ("kilim.png", 32, 32, "xy", kilim),
    ("carpet_border.png", 64, 16, "x", carpet_border),
    ("arabesque.png", 64, 64, "xy", arabesque),
]


def render(width, height, fn):
    """Grid of 0-based palette indices."""
    return [[fn(x, y) for x in range(width)] for y in range(height)]


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for name, width, height, _, fn in ORNAMENTS:
        grid = render(width, height, fn)
        write_png(os.path.join(out_dir, name), width, height, [[PALETTE[i] + (255,) for i in row] for row in grid])


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base/textures/ornaments")

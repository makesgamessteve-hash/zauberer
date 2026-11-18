# SPDX-License-Identifier: MIT
# REUSE-IgnoreStart
"""Generates the vegetation sprite atlas (sprites/flora.png + sprites/flora.lua):
one 32 x 48 pixel frame per plant form, drawn from small procedural plant
models (trunk, branches, leaf clusters) in colours of the base palette only,
lit from the upper left, with hard alpha for alpha testing. The last frames
are clump impostors: a group of crowns that stands for many trees far away.

Frames are listed in flora.lua by name; the species registry (data/flora.lua)
refers to them by these names."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from gen_palette import PALETTE, ramp_color, write_lua  # noqa: E402
from pngw import write_png  # noqa: E402
from tilenoise import hash32  # noqa: E402

W, H = 32, 48
COLUMNS = 8


def col(ramp, step):
    r, g, b = PALETTE[ramp_color(ramp, step)]
    return (r, g, b, 255)


class Frame:
    def __init__(self, seed):
        self.px = [[(0, 0, 0, 0)] * W for _ in range(H)]
        self.seed = seed

    def put(self, x, y, c):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = c

    def noise(self, x, y, k=0):
        return hash32(self.seed, x, y, k) % 1000 / 1000.0

    def trunk(self, x0, y_top, y_bottom, width, ramp="brown", lean=0.0):
        for y in range(y_top, y_bottom + 1):
            t = (y - y_top) / max(1, y_bottom - y_top)
            cx = x0 + lean * (1.0 - t) ** 2
            w = width * (0.6 + 0.4 * t)
            for x in range(int(cx - w), int(cx + w) + 1):
                side = (x - cx) / max(w, 0.5)
                step = 2 if side > 0.2 else (4 if side < -0.4 else 3)
                self.put(x, y, col(ramp, step))

    def blob(self, cx, cy, rx, ry, ramp, steps, k=0, ragged=0.25):
        """A leaf cluster: an ellipse with ragged edges, lit from the upper left."""
        n = len(steps)
        for y in range(int(cy - ry - 1), int(cy + ry + 2)):
            for x in range(int(cx - rx - 1), int(cx + rx + 2)):
                dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
                d = math.hypot(dx, dy)
                if d > 1.0 + ragged * (self.noise(x, y, k) - 0.5) * 2:
                    continue
                light = 0.5 - 0.35 * dx - 0.45 * dy + 0.3 * (self.noise(x, y, k + 7) - 0.5)
                i = min(n - 1, max(0, int(light * n)))
                self.put(x, y, col(ramp, steps[i]))

    def pixels(self):
        return self.px


def broadleaf(seed, ramp="green", top=6):
    f = Frame(seed)
    f.trunk(16, 26, 47, 1.6)
    for k in range(7):
        a = k / 7 * 2 * math.pi + seed
        r = 4 + 3 * (hash32(seed, k) % 100) / 100
        f.blob(16 + 7 * math.cos(a), 17 + 6 * math.sin(a), r + 3, r + 2, ramp, [1, 2, 3, 4, 5], k)
    f.blob(16, top + 10, 9, 8, ramp, [2, 3, 4, 5], 9)
    return f


def conifer(seed, ramp="green", slim=1.0):
    f = Frame(seed)
    f.trunk(16, 38, 47, 1.2)
    for layer in range(7):
        y = 6 + layer * 5
        half = slim * (2.5 + layer * 1.6)
        for yy in range(y, y + 7):
            t = (yy - y) / 7
            w = half * (0.3 + 0.7 * t)
            for x in range(int(16 - w), int(16 + w) + 1):
                light = 0.55 - 0.4 * (x - 16) / max(w, 1) + 0.25 * (f.noise(x, yy) - 0.5)
                step = [1, 1, 2, 3][min(3, max(0, int(light * 4)))]
                f.put(x, yy, col(ramp, step))
    return f


def pine(seed):
    f = Frame(seed)
    f.trunk(16, 14, 47, 1.3, lean=2.0)
    f.blob(17, 11, 10, 5, "green", [1, 2, 3], 1)
    f.blob(12, 15, 6, 3, "green", [1, 2, 3], 2)
    f.blob(22, 14, 6, 3, "green", [1, 2, 3], 3)
    return f


def palm(seed, date=False):
    f = Frame(seed)
    f.trunk(16, 14, 47, 1.2, ramp="ochre" if date else "brown", lean=-4.0)
    cx, cy = 20, 13
    for k in range(9):
        a = math.pi * (1.05 + k / 8 * 0.95) + 0.2 * math.sin(seed + k)
        for s in range(14):
            t = s / 14
            x = cx + math.cos(a) * s * 1.0
            y = cy + math.sin(a) * s * 0.7 + t * t * 6
            f.put(int(x), int(y), col("green", 2 + (k + s) % 3))
            f.put(int(x), int(y) + 1, col("green", 1 + (k % 2)))
    if date:
        for k in range(6):
            f.put(cx - 2 + k % 3, cy + 2 + k // 3, col("saffron", 1 + k % 2))
    return f


def birch(seed):
    f = Frame(seed)
    f.trunk(16, 16, 47, 1.3, ramp="snow")
    for y in range(18, 47, 4):
        f.put(15 + (y % 3), y, col("night", 2))
    for k in range(5):
        a = k / 5 * 2 * math.pi + seed
        f.blob(16 + 5 * math.cos(a), 14 + 5 * math.sin(a), 5, 5, "olive", [2, 3, 4, 4], k)
    return f


def acacia(seed):
    f = Frame(seed)
    f.trunk(16, 20, 47, 1.2)
    f.trunk(11, 18, 30, 0.8)
    f.trunk(21, 18, 30, 0.8)
    f.blob(16, 16, 15, 4, "olive", [1, 2, 3, 4], 1, ragged=0.15)
    return f


def cypress(seed):
    f = Frame(seed)
    f.trunk(16, 42, 47, 1.0)
    f.blob(16, 23, 4.5, 21, "green", [1, 1, 2, 3], 1, ragged=0.2)
    return f


def shrub(seed, ramp="green"):
    f = Frame(seed)
    for k in range(5):
        f.blob(10 + k * 3, 40 + (k % 2), 5, 5, ramp, [1, 2, 3, 4], k)
    return f


def cactus(seed):
    f = Frame(seed)
    f.blob(16, 30, 3, 17, "turquoise", [1, 2, 2, 3], 1, ragged=0.05)
    f.blob(10, 30, 2, 6, "turquoise", [1, 2, 3], 2, ragged=0.05)
    f.blob(22, 26, 2, 6, "turquoise", [1, 2, 3], 3, ragged=0.05)
    for y in range(31, 36):
        f.put(12, y, col("turquoise", 2))
        f.put(20, y - 2, col("turquoise", 2))
    return f


def snag(seed):
    f = Frame(seed)
    f.trunk(16, 8, 47, 1.4, ramp="stone")
    for k in range(6):
        y = 12 + k * 5
        d = 1 if k % 2 else -1
        for s in range(7 - k // 2):
            f.put(16 + d * s, y - s // 2, col("stone", 2 + s % 2))
    return f


def tropical(seed):
    f = Frame(seed)
    f.trunk(16, 16, 47, 1.8)
    for k in range(6):
        a = k / 6 * 2 * math.pi + seed
        f.blob(16 + 9 * math.cos(a), 11 + 4 * math.sin(a), 6, 4, "green", [1, 1, 2, 3, 4], k)
    return f


def grass(seed):
    f = Frame(seed)
    for k in range(22):
        x = 5 + (hash32(seed, k) % 23)
        tall = 22 + hash32(seed, k, 1) % 22
        lean = (hash32(seed, k, 2) % 9) - 4
        for s in range(tall):
            f.put(x + lean * s // tall, 47 - s, col("olive" if k % 3 == 0 else "green", 3 + (s * 2) // tall))
    return f


def fern(seed):
    f = Frame(seed)
    for k in range(9):
        a = math.pi * (1.08 + k / 8 * 0.84)
        for s in range(17):
            x = 16 + math.cos(a) * s
            y = 46 + math.sin(a) * s * 2.4 + (s * s) / 20
            f.put(int(x), int(y), col("green", 2 + s % 3))
            f.put(int(x) + (1 if k > 3 else -1), int(y) + 1, col("green", 3))
    return f


def flowers(seed):
    f = grass(seed)
    for k in range(9):
        x = 6 + hash32(seed, k, 5) % 20
        y = 8 + hash32(seed, k, 6) % 22
        c = col("saffron", 3) if k % 3 else col("pomegranate", 4)
        f.put(x, y, c)
        f.put(x + 1, y, c)
    return f


def reeds(seed):
    f = Frame(seed)
    for k in range(10):
        x = 8 + 2 * k + hash32(seed, k) % 2
        tall = 28 + hash32(seed, k, 1) % 16
        for s in range(tall):
            f.put(x, 47 - s, col("olive", 2 + (s * 3) // tall))
        f.put(x, 47 - tall, col("brown", 3))
        f.put(x, 46 - tall, col("brown", 3))
    return f


def clump(seed, kind):
    """Several crowns in one frame, for far forests."""
    f = Frame(seed)
    spots = [(8, 30, 7), (24, 30, 7), (16, 22, 8), (11, 38, 6), (22, 39, 6)]
    for k, (x, y, r) in enumerate(spots):
        if kind == "conifer":
            for yy in range(y - r - 4, y + r):
                w = max(0, (yy - (y - r - 4)) * r / (2 * r + 4))
                for xx in range(int(x - w), int(x + w) + 1):
                    light = 0.55 - 0.4 * (xx - x) / max(w, 1)
                    f.put(xx, yy, col("green", [1, 1, 2, 3][min(3, max(0, int(light * 4)))]))
        elif kind == "dry":
            f.blob(x, y, r + 2, r - 2, "olive", [1, 2, 3, 4], k)
        elif kind == "tropical":
            f.blob(x, y, r + 1, r, "green", [1, 1, 2, 3, 4], k)
        else:
            f.blob(x, y, r, r, "green", [1, 2, 3, 4, 5], k)
    return f


FRAMES = [
    ("oak", lambda: broadleaf(1)),
    ("beech", lambda: broadleaf(2, top=4)),
    ("spruce", lambda: conifer(3)),
    ("fir", lambda: conifer(4, slim=0.8)),
    ("pine", lambda: pine(5)),
    ("palm", lambda: palm(6)),
    ("date_palm", lambda: palm(7, date=True)),
    ("birch", lambda: birch(8)),
    ("acacia", lambda: acacia(9)),
    ("cypress", lambda: cypress(10)),
    ("shrub", lambda: shrub(11)),
    ("dry_shrub", lambda: shrub(12, "olive")),
    ("cactus", lambda: cactus(13)),
    ("snag", lambda: snag(14)),
    ("tropical", lambda: tropical(15)),
    ("grass", lambda: grass(16)),
    ("fern", lambda: fern(17)),
    ("flowers", lambda: flowers(18)),
    ("reeds", lambda: reeds(19)),
    ("clump_broadleaf", lambda: clump(20, "broadleaf")),
    ("clump_conifer", lambda: clump(21, "conifer")),
    ("clump_dry", lambda: clump(22, "dry")),
    ("clump_tropical", lambda: clump(23, "tropical")),
]


def main(out_dir):
    rows = (len(FRAMES) + COLUMNS - 1) // COLUMNS
    width, height = W * COLUMNS, H * rows
    pixels = [[(0, 0, 0, 0)] * width for _ in range(height)]
    for i, (_, make) in enumerate(FRAMES):
        frame = make().pixels()
        ox, oy = (i % COLUMNS) * W, (i // COLUMNS) * H
        for y in range(H):
            for x in range(W):
                pixels[oy + y][ox + x] = frame[y][x]
    os.makedirs(out_dir, exist_ok=True)
    write_png(os.path.join(out_dir, "flora.png"), width, height, pixels)
    lines = ["return {", '  texture = "sprites/flora.png",', "  frame_width = %d," % W,
             "  frame_height = %d," % H, "  columns = %d," % COLUMNS, "  frames = {"]
    for i, (name, _) in enumerate(FRAMES):
        lines.append('    "%s", -- %d' % (name, i))
    lines += ["  },", "}"]
    write_lua(os.path.join(out_dir, "flora.lua"), "gen_flora.py", lines)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base/sprites")
# REUSE-IgnoreEnd

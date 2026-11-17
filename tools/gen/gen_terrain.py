# SPDX-License-Identifier: MIT
"""Generates the terrain material textures (textures/terrain/atlas.png and
atlas.lua): tileable 64 x 64 pixel-art layers that use only colours of the
base palette, stacked in one vertical strip (layer i at y = 64 * i).

Scale: one texel is about one metre, so the noise works at three scales:
patches of 16-32 texels, clumps of 4-8 texels and single-texel grain. Every
field is computed on the torus (tilenoise.py), so the layers tile seamlessly.
Colours are assigned by rank (tilenoise.rank_quantize), which fixes each
shade's share of the texture and so its average colour."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import tilenoise as tn  # noqa: E402
from gen_palette import PALETTE, ramp_color as c, write_lua  # noqa: E402
from pngw import write_png  # noqa: E402

SIZE = 64


def _blend(*parts):
    """Weighted sum of fields: parts are (field, weight); result 0..65535."""
    total = sum(w for _, w in parts)
    return [[sum(f[y][x] * w for f, w in parts) // total for x in range(SIZE)] for y in range(SIZE)]


def _mask(field, per_cent):
    """Mask of the per_cent % texels with the highest field values (by rank,
    ties broken by position)."""
    order = sorted(((field[y][x], y, x) for y in range(SIZE) for x in range(SIZE)), reverse=True)
    out = [[False] * SIZE for _ in range(SIZE)]
    for _, y, x in order[:len(order) * per_cent // 100]:
        out[y][x] = True
    return out


def _specks(img, seed, per_mille, color):
    """Sets single texels to color with the given probability (per mille)."""
    for y in range(SIZE):
        for x in range(SIZE):
            if tn.hash32(seed, x, y, 0x5BEC) % 1000 < per_mille:
                img[y][x] = color
    return img


def _edges(cells, width):
    """Mask of texels within about width / 2 texels of a Worley cell boundary
    (exact integer square roots of the squared distances, 16 units per texel)."""
    limit = width * 20
    return [[math.isqrt(d2) - math.isqrt(d1) < limit for d1, d2, _ in row] for row in cells]


def grass(seed):
    field = _blend((tn.fbm(SIZE, [(32, 2), (16, 3), (8, 3), (4, 2)], seed), 6),
                   (tn.white(SIZE, seed + 1), 2))
    img = tn.rank_quantize(field, [(c("green", 1), 3), (c("green", 2), 22), (c("green", 3), 45),
                                   (c("green", 4), 25), (c("green", 5), 5)])
    _specks(img, seed + 2, 12, c("olive", 4))
    return img


def dry_grass(seed):
    field = _blend((tn.fbm(SIZE, [(32, 2), (16, 3), (8, 3), (4, 2)], seed), 5),
                   (tn.white(SIZE, seed + 1), 3))
    img = tn.rank_quantize(field, [(c("brown", 3), 6), (c("olive", 1), 18), (c("olive", 2), 36),
                                   (c("olive", 3), 30), (c("olive", 4), 10)])
    _specks(img, seed + 2, 15, c("sand", 2))
    _specks(img, seed + 3, 10, c("green", 3))
    return img


def forest_floor(seed):
    # Leaf litter and needles with moss cushions in between.
    litter = _blend((tn.fbm(SIZE, [(8, 2), (4, 3), (2, 2)], seed + 1), 3), (tn.white(SIZE, seed + 2), 2))
    img = tn.rank_quantize(litter, [(c("brown", 1), 14), (c("brown", 2), 40), (c("brown", 3), 30),
                                    (c("ochre", 1), 16)])
    moss = _mask(_blend((tn.fbm(SIZE, [(16, 3), (8, 3), (4, 1)], seed), 6), (tn.white(SIZE, seed + 3), 1)), 48)
    tone = tn.rank_quantize(_blend((tn.fbm(SIZE, [(8, 1), (4, 1)], seed + 5), 2), (tn.white(SIZE, seed + 6), 1)),
                            [(c("green", 1), 25), (c("green", 2), 45), (c("green", 3), 18),
                             (c("olive", 2), 12)])
    for y in range(SIZE):
        for x in range(SIZE):
            if moss[y][x]:
                img[y][x] = tone[y][x]
    _specks(img, seed + 4, 14, c("brown", 4))
    return img


def soil(seed):
    clods = tn.worley(SIZE, 4, seed, jitter=14)
    shade = [[min(65535, d1 >> 4) for d1, _, _ in row] for row in clods]
    field = _blend((tn.fbm(SIZE, [(16, 2), (8, 2)], seed + 1), 3), (shade, 3), (tn.white(SIZE, seed + 2), 2))
    img = tn.rank_quantize(field, [(c("brown", 5), 10), (c("brown", 4), 34), (c("brown", 3), 40),
                                   (c("brown", 2), 16)])
    _specks(img, seed + 3, 10, c("stone", 4))
    return img


def sand(seed):
    warp_x = tn.fbm(SIZE, [(16, 1), (8, 1)], seed)
    warp_y = tn.fbm(SIZE, [(16, 1), (8, 1)], seed + 1)
    # Wind ripples: a sawtooth along a diagonal that wraps (period 8 texels),
    # bent by a domain warp.
    ripple = [[((x + 2 * y) % 8) * 8192 for x in range(SIZE)] for y in range(SIZE)]
    ripple = tn.warp(ripple, warp_x, warp_y, 3)
    field = _blend((ripple, 3), (tn.fbm(SIZE, [(32, 2), (16, 2)], seed + 2), 3), (tn.white(SIZE, seed + 3), 2))
    img = tn.rank_quantize(field, [(c("sand", 1), 12), (c("sand", 2), 44), (c("sand", 3), 36),
                                   (c("sand", 4), 8)])
    _specks(img, seed + 4, 8, c("ochre", 3))
    return img


def rock(seed):
    # Weathered rock: faceted blocks (a warped cellular field), broken cracks
    # along some block edges and fine grain.
    cells = tn.worley(SIZE, 16, seed, jitter=14)
    warp_x = tn.fbm(SIZE, [(16, 1), (8, 1)], seed + 5)
    warp_y = tn.fbm(SIZE, [(16, 1), (8, 1)], seed + 6)
    cells = tn.warp(cells, warp_x, warp_y, 3)
    facet = [[tn.hash32(seed, cid) & 0xFFFF for _, _, cid in row] for row in cells]
    field = _blend((facet, 2), (tn.fbm(SIZE, [(16, 2), (8, 2), (4, 1)], seed + 1), 4),
                   (tn.white(SIZE, seed + 2), 2))
    img = tn.rank_quantize(field, [(c("stone", 3), 12), (c("stone", 4), 36), (c("stone", 5), 38),
                                   (c("stone", 6), 14)])
    crack = _edges(cells, 1)
    broken = _mask(tn.fbm(SIZE, [(8, 1), (4, 1)], seed + 3), 70)
    for y in range(SIZE):
        for x in range(SIZE):
            if crack[y][x] and broken[y][x]:
                img[y][x] = c("stone", 2)
    _specks(img, seed + 4, 12, c("stone", 2))
    return img


def gravel(seed):
    # Scree and river gravel: rounded stones of mixed tones, packed tight.
    cells = tn.worley(SIZE, 4, seed, jitter=16)
    tones = [c("stone", 4), c("stone", 5), c("sand", 1), c("brown", 4), c("stone", 4), c("stone", 3)]
    gap = _edges(cells, 1)
    img = []
    for y in range(SIZE):
        row = []
        for x in range(SIZE):
            d1, _, cid = cells[y][x]
            if gap[y][x] and tn.hash32(seed, x, y) % 3:
                row.append(c("brown", 3) if tn.hash32(seed + 3, x, y) % 2 else c("stone", 3))
                continue
            tone = tones[tn.hash32(seed + 1, cid) % len(tones)]
            if d1 < 16 * 16 and tn.hash32(seed + 2, cid) % 2:
                tone = c("sand", 2) if tone in (c("sand", 1), c("brown", 4)) else c("stone", 6)
            row.append(tone)
        img.append(row)
    return img


def snow(seed):
    field = _blend((tn.fbm(SIZE, [(32, 3), (16, 3), (8, 2)], seed), 6), (tn.white(SIZE, seed + 1), 1))
    img = tn.rank_quantize(field, [(c("snow", 1), 6), (c("snow", 2), 24), (c("snow", 3), 50),
                                   (c("snow", 4), 20)])
    _specks(img, seed + 2, 10, c("snow", 4))
    return img


def ice(seed):
    # Lake and glacier ice: smooth blue depth variation, white cracks.
    field = _blend((tn.fbm(SIZE, [(32, 3), (16, 3), (8, 1)], seed), 12), (tn.white(SIZE, seed + 1), 1))
    img = tn.rank_quantize(field, [(c("sky", 2), 10), (c("turquoise", 4), 25), (c("sky", 3), 45),
                                   (c("sky", 4), 20)])
    cracks = _edges(tn.worley(SIZE, 16, seed + 2, jitter=16), 1)
    for y in range(SIZE):
        for x in range(SIZE):
            if cracks[y][x] and tn.hash32(seed + 3, x, y) % 5:
                img[y][x] = c("snow", 3)
    _specks(img, seed + 4, 6, c("snow", 4))
    return img


def mud(seed):
    # Churned wet earth with small puddles that catch the sky.
    field = _blend((tn.fbm(SIZE, [(16, 3), (8, 3), (4, 1)], seed), 5), (tn.white(SIZE, seed + 2), 2))
    img = tn.rank_quantize(field, [(c("stone", 3), 8), (c("brown", 3), 30), (c("stone", 2), 22),
                                   (c("brown", 2), 28), (c("brown", 1), 12)])
    wet = tn.fbm(SIZE, [(16, 2), (8, 3), (4, 1)], seed + 1)
    puddle, rim = _mask(wet, 9), _mask(wet, 14)
    for y in range(SIZE):
        for x in range(SIZE):
            if puddle[y][x]:
                img[y][x] = c("water", 2) if tn.hash32(seed + 3, x, y) % 5 else c("water", 3)
            elif rim[y][x]:
                img[y][x] = c("water", 1)
    return img


def savanna(seed):
    field = _blend((tn.fbm(SIZE, [(32, 2), (16, 3), (8, 2), (4, 2)], seed), 5), (tn.white(SIZE, seed + 1), 3))
    img = tn.rank_quantize(field, [(c("ochre", 2), 10), (c("ochre", 3), 36), (c("ochre", 4), 34),
                                   (c("sand", 3), 20)])
    _specks(img, seed + 2, 25, c("olive", 3))
    _specks(img, seed + 3, 10, c("brown", 3))
    return img


def tundra(seed):
    # Low moss, sedge and lichen over stony ground.
    field = _blend((tn.fbm(SIZE, [(32, 2), (16, 3), (8, 3), (4, 1)], seed), 5), (tn.white(SIZE, seed + 1), 2))
    img = tn.rank_quantize(field, [(c("brown", 2), 10), (c("green", 2), 22), (c("olive", 1), 30),
                                   (c("olive", 2), 26), (c("olive", 3), 12)])
    stones = _mask(_blend((tn.fbm(SIZE, [(8, 1), (4, 1)], seed + 2), 3), (tn.white(SIZE, seed + 3), 1)), 10)
    for y in range(SIZE):
        for x in range(SIZE):
            if stones[y][x]:
                img[y][x] = c("stone", 4) if tn.hash32(seed + 4, x, y) % 3 else c("stone", 5)
    _specks(img, seed + 5, 6, c("pomegranate", 3))
    return img


# Strip order of the atlas (the layer index the renderer uses).
MATERIALS = [
    ("grass", grass),
    ("dry_grass", dry_grass),
    ("forest_floor", forest_floor),
    ("soil", soil),
    ("sand", sand),
    ("rock", rock),
    ("gravel", gravel),
    ("snow", snow),
    ("ice", ice),
    ("mud", mud),
    ("savanna", savanna),
    ("tundra", tundra),
]


def layers():
    """(name, 64x64 grid of 0-based palette indices) per material."""
    return [(name, recipe(1000 + 97 * k)) for k, (name, recipe) in enumerate(MATERIALS)]


def main(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    rows = []
    for _, grid in layers():
        rows += [[PALETTE[i] + (255,) for i in row] for row in grid]
    write_png(os.path.join(out_dir, "atlas.png"), SIZE, SIZE * len(MATERIALS), rows)
    lines = ["-- Terrain material layers, stacked vertically (layer i at y = size * (i - 1)).",
             "return {",
             '  texture = "textures/terrain/atlas.png",',
             "  size = %d," % SIZE,
             "  layers = {"]
    lines += ['    "%s",' % name for name, _ in MATERIALS]
    lines += ["  },", "}"]
    write_lua(os.path.join(out_dir, "atlas.lua"), "gen_terrain.py", lines)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base/textures/terrain")

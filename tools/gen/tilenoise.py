# SPDX-License-Identifier: MIT
"""Deterministic, tileable noise for texture generators (integer math only).

Every function works on a square torus of SIZE x SIZE pixels: lattice and
feature-point coordinates wrap around, so anything built from these fields
tiles seamlessly. All arithmetic is on Python integers, so results are
identical on every platform and Python version.

Fields are lists of rows (field[y][x]); noise values run from 0 to 65535."""

ONE = 1 << 16
_MASK = 0xFFFFFFFF


def hash32(*values):
    """32-bit integer hash of a sequence of integers."""
    h = 0x9E3779B9
    for v in values:
        h = (h ^ (v & _MASK)) * 0x85EBCA6B & _MASK
        h ^= h >> 13
        h = h * 0xC2B2AE35 & _MASK
        h ^= h >> 16
    h = h * 0x27D4EB2F & _MASK
    return h ^ (h >> 15)


def _smooth(t):
    """Integer smoothstep: t in [0, 65536] -> [0, 65536]."""
    return (t * t * (3 * ONE - 2 * t)) >> 32


def value_noise(size, cell, seed, offset=(0, 0)):
    """Smooth value noise with lattice spacing `cell` (which must divide
    size), shifted by `offset` pixels to hide the lattice."""
    if size % cell:
        raise ValueError("cell %d does not divide %d" % (cell, size))
    n = size // cell
    lattice = [[hash32(seed, i, j) & 0xFFFF for i in range(n)] for j in range(n)]
    weights = [_smooth(t * ONE // cell) for t in range(cell)]
    ox, oy = offset
    field = []
    for y in range(size):
        py = (y + oy) % size
        j0, wy = py // cell, weights[py % cell]
        j1 = (j0 + 1) % n
        row_a, row_b = lattice[j0], lattice[j1]
        out = []
        for x in range(size):
            px = (x + ox) % size
            i0, wx = px // cell, weights[px % cell]
            i1 = (i0 + 1) % n
            top = row_a[i0] + (((row_a[i1] - row_a[i0]) * wx) >> 16)
            bottom = row_b[i0] + (((row_b[i1] - row_b[i0]) * wx) >> 16)
            out.append(top + (((bottom - top) * wy) >> 16))
        field.append(out)
    return field


def fbm(size, octaves, seed):
    """Weighted sum of value-noise octaves, normalised to 0..65535.
    octaves: list of (cell, weight) with integer weights."""
    total = sum(w for _, w in octaves)
    acc = [[0] * size for _ in range(size)]
    for k, (cell, weight) in enumerate(octaves):
        h = hash32(seed, k, 0x0C7A)
        layer = value_noise(size, cell, seed * 31 + k, (h % size, (h >> 8) % size))
        for y in range(size):
            row, src = acc[y], layer[y]
            for x in range(size):
                row[x] += src[x] * weight
    return [[v // total for v in row] for row in acc]


def white(size, seed):
    """Per-pixel hash noise, 0..65535."""
    return [[hash32(seed, x, y, 0x5EED) & 0xFFFF for x in range(size)] for y in range(size)]


def worley(size, cell, seed, jitter=16):
    """Cellular noise on the torus with one feature point per cell, searched
    in the 5 x 5 neighbouring cells (each distinct cell once, so few large
    cells work too).
    Returns rows of (d1, d2, cell_id): the squared distances to the nearest
    and second-nearest feature point in 1/16 pixel units, and the id of the
    nearest cell. jitter (0..16) scales how far points stray from the cell
    centre."""
    n = size // cell
    if size % cell or n < 2:
        raise ValueError("cell %d must divide %d at least twice" % (cell, size))
    sub = 16
    span, half = size * sub, size * sub // 2
    reach = cell * sub * jitter // 16
    base = (cell * sub - reach) // 2
    points = []  # points[j * n + i]
    for j in range(n):
        for i in range(n):
            h = hash32(seed, i, j, 0xFEA7)
            points.append((i * cell * sub + base + h % (reach + 1),
                           j * cell * sub + base + (h >> 12) % (reach + 1)))
    # The distinct neighbour cells of each cell, in a fixed order.
    around = [sorted({((cy + dj) % n) * n + (cx + di) % n for dj in range(-2, 3) for di in range(-2, 3)})
              for cy in range(n) for cx in range(n)]
    field = []
    for y in range(size):
        cy, py = y // cell, y * sub + sub // 2
        out = []
        for x in range(size):
            px = x * sub + sub // 2
            d1 = d2 = None
            nearest = 0
            for cid in around[cy * n + x // cell]:
                fx, fy = points[cid]
                dx = (px - fx + half) % span - half
                dy = (py - fy + half) % span - half
                d = dx * dx + dy * dy
                if d1 is None or d < d1:
                    d1, d2, nearest = d, d1, cid
                elif d2 is None or d < d2:
                    d2 = d
            out.append((d1, d2, nearest))
        field.append(out)
    return field


def warp(field, dx_field, dy_field, amount):
    """Domain warp: samples field at (x + dx, y + dy) with integer offsets of
    up to +-amount pixels taken from two noise fields (0..65535)."""
    size = len(field)
    out = []
    for y in range(size):
        row = []
        for x in range(size):
            ox = (dx_field[y][x] * (2 * amount + 1) >> 16) - amount
            oy = (dy_field[y][x] * (2 * amount + 1) >> 16) - amount
            row.append(field[(y + oy) % size][(x + ox) % size])
        out.append(row)
    return out


def rank_quantize(field, shades):
    """Maps a scalar field to colour indices by rank: the lowest values get
    shades[0] and so on. shades: list of (index, weight) with integer weights;
    each shade covers its share of the pixels. Ties break by position."""
    size_y, size_x = len(field), len(field[0])
    order = sorted((field[y][x], y, x) for y in range(size_y) for x in range(size_x))
    total = sum(w for _, w in shades)
    count = len(order)
    out = [[0] * size_x for _ in range(size_y)]
    start, acc = 0, 0
    for index, weight in shades:
        acc += weight
        end = count * acc // total
        for k in range(start, end):
            _, y, x = order[k]
            out[y][x] = index
        start = end
    return out

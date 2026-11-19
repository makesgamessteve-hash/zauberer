# SPDX-License-Identifier: MIT
"""Generates the kit pieces buildings are assembled from (concept 6.5): unit
shapes in every building material, each with an embedded 16x16 pattern
texture (models/kit/<shape>_<material>.glb). Pieces are scaled per axis
when drawn: a box spans x and z from -0.5 to 0.5 and y from 0 to 1.
Deterministic: the same script always writes the same bytes."""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pngw import png_bytes  # noqa: E402
from gen_home import write_glb  # noqa: E402

COLOURS = {
    "mudbrick": (190, 150, 100), "brick": (160, 80, 55), "plaster": (230, 220, 195), "stone": (150, 145, 135),
    "timber": (120, 85, 50), "felt": (225, 215, 190), "rammed_earth": (180, 130, 85), "coral_stone": (220, 205, 175),
    "sandstone": (205, 160, 110), "bamboo": (170, 160, 90), "tile": (40, 140, 150), "thatch": (190, 165, 90),
    "wool": (150, 30, 40), "cloth": (200, 190, 160), "clay": (170, 100, 60), "copper": (180, 100, 50),
    "iron": (70, 70, 75), "reed": (180, 170, 110), "hay": (210, 190, 110), "water": (50, 110, 150),
    "rubble": (130, 120, 110),
}
SHAPES = {
    "box": list(COLOURS),
    "dome": ["tile", "plaster", "mudbrick", "brick"],
    "gable": ["tile", "thatch", "timber", "felt"],
    "vault": ["mudbrick", "brick", "stone", "plaster", "rammed_earth", "coral_stone", "sandstone", "timber"],
    "cone": ["felt", "thatch"],
    "cylinder": ["stone", "mudbrick", "brick", "timber", "rammed_earth", "sandstone", "coral_stone", "plaster"],
}


def _hash(x, y, k):
    v = (x * 374761393 + y * 668265263 + k * 2147483647) & 0xFFFFFFFF
    v = ((v ^ (v >> 13)) * 1274126177) & 0xFFFFFFFF
    return (v ^ (v >> 16)) / 0xFFFFFFFF


def texture(mat):
    """A 16x16 pattern: courses, blocks, grain or tiles in the colour."""
    base = COLOURS[mat]
    rows = []
    for y in range(16):
        row = []
        for x in range(16):
            shade = 1.0 + (_hash(x, y, len(mat)) - 0.5) * 0.12
            if mat in ("brick", "mudbrick"):
                if y % 4 == 3 or (x + (4 if (y // 4) % 2 else 0)) % 8 == 7:
                    shade *= 0.72
            elif mat in ("stone", "sandstone", "coral_stone", "rubble"):
                if y % 8 == 7 or (x + (6 if (y // 8) % 2 else 0)) % 12 == 11:
                    shade *= 0.7
            elif mat in ("timber", "bamboo"):
                if (x + int(2 * math.sin(y * 0.7))) % 5 == 0:
                    shade *= 0.78
            elif mat in ("rammed_earth", "thatch", "reed", "hay"):
                if y % 3 == 0:
                    shade *= 0.82
            elif mat == "tile":
                d = abs(x % 8 - 3.5) + abs(y % 8 - 3.5)
                if d < 2:
                    row.append((220, 170, 60, 255))
                    continue
                if d > 5.5:
                    row.append((40, 50, 120, 255))
                    continue
            elif mat == "wool":
                d = abs(x - 7.5) + abs(y - 7.5)
                if d < 3:
                    row.append((220, 170, 60, 255))
                    continue
                if x in (0, 15) or y in (0, 15):
                    row.append((40, 50, 120, 255))
                    continue
            row.append(tuple(max(0, min(255, int(c * shade))) for c in base) + (255,))
        rows.append(row)
    return png_bytes(16, 16, rows)


class Mesh:
    def __init__(self):
        self.positions, self.normals, self.uvs, self.indices = [], [], [], []

    def quad(self, a, b, c, d, n, uv=((0, 1), (1, 1), (1, 0), (0, 0))):
        base = len(self.positions)
        self.positions.extend([a, b, c, d])
        self.normals.extend([n] * 4)
        self.uvs.extend(uv)
        self.indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    def tri(self, a, b, c, n, uv=((0, 1), (1, 1), (0.5, 0))):
        base = len(self.positions)
        self.positions.extend([a, b, c])
        self.normals.extend([n] * 3)
        self.uvs.extend(uv)
        self.indices.extend([base, base + 1, base + 2])

    def data(self):
        return self.positions, self.normals, self.uvs, self.indices


def box():
    m = Mesh()
    lo, hi = -0.5, 0.5
    m.quad((lo, 0, hi), (hi, 0, hi), (hi, 1, hi), (lo, 1, hi), (0, 0, 1))
    m.quad((hi, 0, lo), (lo, 0, lo), (lo, 1, lo), (hi, 1, lo), (0, 0, -1))
    m.quad((hi, 0, hi), (hi, 0, lo), (hi, 1, lo), (hi, 1, hi), (1, 0, 0))
    m.quad((lo, 0, lo), (lo, 0, hi), (lo, 1, hi), (lo, 1, lo), (-1, 0, 0))
    m.quad((lo, 1, hi), (hi, 1, hi), (hi, 1, lo), (lo, 1, lo), (0, 1, 0))
    m.quad((lo, 0, lo), (hi, 0, lo), (hi, 0, hi), (lo, 0, hi), (0, -1, 0))
    return m.data()


def dome(segments=12, rings=5):
    """A half ellipsoid on the unit square's circle, 1 high."""
    m = Mesh()
    for r in range(rings):
        p0, p1 = r / rings * math.pi / 2, (r + 1) / rings * math.pi / 2
        for k in range(segments):
            a0, a1 = k / segments * 2 * math.pi, (k + 1) / segments * 2 * math.pi

            def pt(p, a):
                return (0.5 * math.cos(p) * math.sin(a), math.sin(p), 0.5 * math.cos(p) * math.cos(a))
            a, b, c, d = pt(p0, a0), pt(p0, a1), pt(p1, a1), pt(p1, a0)
            n = (math.sin((a0 + a1) / 2) * math.cos(p0), math.sin(p0), math.cos((a0 + a1) / 2) * math.cos(p0))
            m.quad(a, b, c, d, n, ((k / 3, r / 2 + 0.5), ((k + 1) / 3, r / 2 + 0.5), ((k + 1) / 3, r / 2),
                                   (k / 3, r / 2)))
    return m.data()


def gable():
    """A ridge roof along x: the ridge at y 1 over z 0, eaves at z +-0.5."""
    m = Mesh()
    s = 1 / math.sqrt(2)
    m.quad((-0.5, 0, 0.5), (0.5, 0, 0.5), (0.5, 1, 0), (-0.5, 1, 0), (0, s, s), ((0, 2), (3, 2), (3, 0), (0, 0)))
    m.quad((0.5, 0, -0.5), (-0.5, 0, -0.5), (-0.5, 1, 0), (0.5, 1, 0), (0, s, -s), ((0, 2), (3, 2), (3, 0), (0, 0)))
    m.tri((-0.5, 0, -0.5), (-0.5, 0, 0.5), (-0.5, 1, 0), (-1, 0, 0))
    m.tri((0.5, 0, 0.5), (0.5, 0, -0.5), (0.5, 1, 0), (1, 0, 0))
    return m.data()


def vault(segments=8):
    """A barrel vault along x: a half cylinder over z -0.5 .. 0.5, 1 high."""
    m = Mesh()
    for k in range(segments):
        a0, a1 = k / segments * math.pi, (k + 1) / segments * math.pi
        z0, y0, z1, y1 = 0.5 * math.cos(a0), math.sin(a0), 0.5 * math.cos(a1), math.sin(a1)
        n = (0, math.sin((a0 + a1) / 2), math.cos((a0 + a1) / 2))
        m.quad((-0.5, y0, z0), (0.5, y0, z0), (0.5, y1, z1), (-0.5, y1, z1), n,
               ((0, k / 4), (2, k / 4), (2, (k + 1) / 4), (0, (k + 1) / 4)))
        m.tri((-0.5, 0, 0), (-0.5, y1, z1), (-0.5, y0, z0), (-1, 0, 0))
        m.tri((0.5, 0, 0), (0.5, y0, z0), (0.5, y1, z1), (1, 0, 0))
    return m.data()


def cone(segments=12):
    m = Mesh()
    for k in range(segments):
        a0, a1 = k / segments * 2 * math.pi, (k + 1) / segments * 2 * math.pi
        a = (0.5 * math.sin(a0), 0, 0.5 * math.cos(a0))
        b = (0.5 * math.sin(a1), 0, 0.5 * math.cos(a1))
        n = (math.sin((a0 + a1) / 2) * 0.9, 0.45, math.cos((a0 + a1) / 2) * 0.9)
        m.tri(a, b, (0, 1, 0), n, ((k / 4, 1), ((k + 1) / 4, 1), ((k + 0.5) / 4, 0)))
    return m.data()


def cylinder(segments=12):
    m = Mesh()
    for k in range(segments):
        a0, a1 = k / segments * 2 * math.pi, (k + 1) / segments * 2 * math.pi
        x0, z0, x1, z1 = 0.5 * math.sin(a0), 0.5 * math.cos(a0), 0.5 * math.sin(a1), 0.5 * math.cos(a1)
        n = (math.sin((a0 + a1) / 2), 0, math.cos((a0 + a1) / 2))
        m.quad((x0, 0, z0), (x1, 0, z1), (x1, 1, z1), (x0, 1, z0), n,
               ((k / 3, 3), ((k + 1) / 3, 3), ((k + 1) / 3, 0), (k / 3, 0)))
        m.tri((0, 1, 0), (x0, 1, z0), (x1, 1, z1), (0, 1, 0))
    return m.data()


MAKERS = {"box": box, "dome": dome, "gable": gable, "vault": vault, "cone": cone, "cylinder": cylinder}


def main(models_dir):
    out = os.path.join(models_dir, "kit")
    os.makedirs(out, exist_ok=True)
    gen = "zauberer tools/gen/gen_kit.py"
    textures = {mat: texture(mat) for mat in COLOURS}
    for shape, mats in SHAPES.items():
        mesh = MAKERS[shape]()
        for mat in mats:
            write_glb(os.path.join(out, "%s_%s.glb" % (shape, mat)), "%s_%s" % (shape, mat), mesh, textures[mat], gen)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base/models")

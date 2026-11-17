# SPDX-License-Identifier: MIT
# REUSE-IgnoreStart
"""Tests for the art generators in tools/gen/ and the art they commit to
assets/base: the palette, its lookup table, the dither matrix, the terrain
material atlas and the tile ornaments.

The checks read the committed files and use their own floating-point Oklab
reference, independent of the generators' integer implementation."""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import REPO_ROOT  # noqa: E402  (also puts tools/ on sys.path)

GEN_DIR = REPO_ROOT / "tools" / "gen"
if str(GEN_DIR) not in sys.path:
    sys.path.insert(0, str(GEN_DIR))

import gen_ornaments  # noqa: E402
import gen_terrain  # noqa: E402
import lua_literal  # noqa: E402
from pngw import read_png  # noqa: E402

BASE = REPO_ROOT / "assets" / "base"
NEW_ART = [
    "palettes/base.png", "palettes/base.lua", "palettes/base_lut.png", "palettes/bayer8.png",
    "textures/terrain/atlas.png", "textures/terrain/atlas.lua",
    "textures/ornaments/star8.png", "textures/ornaments/arabesque.png",
    "textures/ornaments/carpet_border.png", "textures/ornaments/kilim.png",
]
GENERATORS = {
    "palettes/": "tools/gen/gen_palette.py",
    "textures/terrain/": "tools/gen/gen_terrain.py",
    "textures/ornaments/": "tools/gen/gen_ornaments.py",
}
MATERIALS = ["grass", "dry_grass", "forest_floor", "soil", "sand", "rock", "gravel", "snow", "ice", "mud",
             "savanna", "tundra"]
ORNAMENTS = {  # file -> (width, height, axes it tiles along)
    "star8.png": (32, 32, "xy"),
    "arabesque.png": (64, 64, "xy"),
    "carpet_border.png": (64, 16, "x"),
    "kilim.png": (32, 32, "xy"),
}


def generator_of(rel):
    return next(source for prefix, source in GENERATORS.items() if rel.startswith(prefix))


# --- Reference colour maths (floating point, independent of the generators) --

def srgb_to_linear(v):
    c = v / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def oklab(rgb):
    r, g, b = (srgb_to_linear(v) for v in rgb[:3])
    lms = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b,
           0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b,
           0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b)
    l_, m_, s_ = (max(0.0, v) ** (1.0 / 3.0) for v in lms)
    return (0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
            1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
            0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_)


def distance(lab1, lab2):
    return sum((a - b) ** 2 for a, b in zip(lab1, lab2)) ** 0.5


def grid_value(level):
    return (level * 255 + 15) // 31


def grid_level(value):
    return (value * 31 + 127) // 255


# --- Seam check ----------------------------------------------------------------

def line_differences(lightness, blur=7):
    """Mean absolute lightness difference between neighbouring columns
    (x, x + 1), after averaging each column over `blur` texels along y (with
    wrap-around). The last entry is the wrap-around seam (w - 1, 0)."""
    h, w = len(lightness), len(lightness[0])
    r = blur // 2
    blurred = [[sum(lightness[(y + k) % h][x] for k in range(-r, r + 1)) / blur for x in range(w)]
               for y in range(h)]
    return [sum(abs(blurred[y][x] - blurred[y][(x + 1) % w]) for y in range(h)) / h for x in range(w)]


def seam_problems(lightness, axes):
    """Edges whose wrap-around difference exceeds 1.25 x the largest or 2 x
    the mean interior difference along the same axis."""
    problems = []
    for axis in axes:
        grid = lightness if axis == "x" else [list(col) for col in zip(*lightness)]
        diffs = line_differences(grid)
        seam, interior = diffs[-1], diffs[:-1]
        mean = sum(interior) / len(interior)
        if seam > 1.25 * max(interior) or seam > 2.0 * mean:
            problems.append("%s seam %.4f (interior mean %.4f, max %.4f)" % (axis, seam, mean, max(interior)))
    return problems


class ArtTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, _, rows = read_png(BASE / "palettes" / "base.png")
        cls.palette = rows[0]
        cls.palette_set = set(cls.palette)
        cls.palette_lab = [oklab(c) for c in cls.palette]

    def lightness(self, rows):
        cache = {c: oklab(c)[0] for c in self.palette_set}
        return [[cache[p] for p in row] for row in rows]

    def atlas_layers(self):
        width, height, rows = read_png(BASE / "textures" / "terrain" / "atlas.png")
        return [rows[64 * i:64 * (i + 1)] for i in range(height // 64)]


def committed_bytes(rel):
    """The committed file's bytes. Text files are compared with LF line ends,
    because a Windows checkout may convert them to CRLF; the generators always
    write LF."""
    data = (BASE / rel).read_bytes()
    return data.replace(b"\r\n", b"\n") if rel.endswith(".lua") else data


class RegenerationTest(unittest.TestCase):
    def test_regenerate_into_empty_directory(self):
        # spec: art-generators: Regenerate into an empty directory
        # The M0 assets (font, sprite, carpet, sounds) are written too; CI's
        # "git diff --exit-code -- assets" step checks those, because the
        # music depends on the installed oggenc.
        with tempfile.TemporaryDirectory() as tmp:
            env = dict(os.environ, PYTHONHASHSEED="4242", PYTHONDONTWRITEBYTECODE="1")
            result = subprocess.run([sys.executable, str(GEN_DIR / "gen_all.py"), tmp], env=env,
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            for rel in NEW_ART:
                with self.subTest(file=rel):
                    written = Path(tmp) / rel
                    self.assertTrue(written.is_file(), "gen_all.py did not write %s" % rel)
                    self.assertEqual(written.read_bytes(), committed_bytes(rel),
                                     "%s differs from the committed file; run tools/gen/gen_all.py" % rel)


class PaletteTest(ArtTestCase):
    def test_palette_size(self):
        # spec: art-generators: Palette size
        width, height, rows = read_png(BASE / "palettes" / "base.png")
        self.assertEqual((width, height), (64, 1))
        self.assertEqual(len(set(rows[0])), 64, "palette colours are distinct")
        self.assertTrue(all(c[3] == 255 for c in rows[0]), "palette colours are opaque")

    def test_ramps_run_dark_to_light(self):
        # spec: art-generators: Ramps run dark to light
        data = lua_literal.parse_file(BASE / "palettes" / "base.lua")
        self.assertEqual(data["colors"], 64)
        ramps = data["ramps"]
        for family in ("lapis", "turquoise", "saffron", "pomegranate", "ochre", "green", "brown", "sand",
                       "stone", "snow", "water", "sky", "night"):
            self.assertIn(family, ramps)
        indices = sorted(i for ramp in ramps.values() for i in ramp)
        self.assertEqual(indices, list(range(1, 65)), "ramps cover every palette index once")
        for name, ramp in sorted(ramps.items()):
            self.assertGreaterEqual(len(ramp), 3, name)
            lightness = [self.palette_lab[i - 1][0] for i in ramp]
            self.assertEqual(lightness, sorted(lightness), "%s runs dark to light" % name)
            self.assertEqual(len(set(lightness)), len(lightness), name)

    def test_palette_lies_on_the_lut_grid(self):
        for color in self.palette:
            for value in color[:3]:
                self.assertEqual(grid_value(grid_level(value)), value, color)


class LutTest(ArtTestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.width, cls.height, cls.lut = read_png(BASE / "palettes" / "base_lut.png")

    def cell(self, r, g, b):
        return self.lut[g][b * 32 + r]

    def test_layout(self):
        self.assertEqual((self.width, self.height), (1024, 32))

    def test_palette_colours_map_to_themselves(self):
        # spec: art-generators: Palette colours map to themselves
        for color in self.palette:
            r, g, b = (grid_level(v) for v in color[:3])
            self.assertEqual(self.cell(r, g, b), color)

    def test_only_palette_colours(self):
        # spec: art-generators: Only palette colours in the table
        for row in self.lut:
            for texel in row:
                self.assertIn(texel, self.palette_set)

    def test_nearest_colour_in_oklab(self):
        # spec: art-generators: Nearest colour in Oklab
        checked = 0
        levels = [grid_value(v) for v in range(32)]
        for g in range(32):
            for b in range(32):
                for r in range(32):
                    lab = oklab((levels[r], levels[g], levels[b]))
                    d = sorted((distance(lab, p), i) for i, p in enumerate(self.palette_lab))
                    if d[1][0] - d[0][0] < 1e-6:
                        continue  # too close to call in floating point
                    checked += 1
                    self.assertEqual(self.cell(r, g, b), self.palette[d[0][1]], (r, g, b))
        self.assertGreater(checked, 32000)


class DitherTest(unittest.TestCase):
    def test_bayer_thresholds(self):
        # spec: art-generators: Bayer thresholds
        width, height, rows = read_png(BASE / "palettes" / "bayer8.png")
        self.assertEqual((width, height), (8, 8))
        self.assertEqual([p[0] for p in rows[0]], [2, 130, 34, 162, 10, 138, 42, 170])
        self.assertEqual([p[0] for p in rows[7]], [254, 126, 222, 94, 246, 118, 214, 86])
        values = []
        for row in rows:
            for r, g, b, a in row:
                self.assertEqual((r, g, b, a), (r, r, r, 255))
                values.append(r)
        self.assertEqual(sorted(values), [4 * i + 2 for i in range(64)])
        # Ordered dither: each 2x2 block holds one value of each quarter.
        for y in range(0, 8, 2):
            for x in range(0, 8, 2):
                block = sorted((rows[y + j][x + i][0] - 2) // 4 // 16 for j in (0, 1) for i in (0, 1))
                self.assertEqual(block, [0, 1, 2, 3])


class TerrainTest(ArtTestCase):
    def test_layer_list_matches_strip(self):
        # spec: art-generators: Layer list matches the strip
        data = lua_literal.parse_file(BASE / "textures" / "terrain" / "atlas.lua")
        self.assertEqual(data["size"], 64)
        self.assertEqual(data["layers"], MATERIALS)
        self.assertEqual(data["texture"], "textures/terrain/atlas.png")
        width, height, _ = read_png(BASE / "textures" / "terrain" / "atlas.png")
        self.assertEqual((width, height), (64, 64 * len(MATERIALS)))
        self.assertEqual([name for name, _ in gen_terrain.MATERIALS], MATERIALS)

    def test_seamless_terrain_tiling(self):
        # spec: art-generators: Seamless terrain tiling
        for name, layer in zip(MATERIALS, self.atlas_layers()):
            with self.subTest(material=name):
                self.assertEqual(seam_problems(self.lightness(layer), "xy"), [])

    def test_seam_check_detects_a_drifting_texture(self):
        # spec: art-generators: Seamless terrain tiling
        layer = self.lightness(self.atlas_layers()[0])
        drifting = [[v + 0.003 * x for x, v in enumerate(row)] for row in layer]
        self.assertEqual(len(seam_problems(drifting, "x")), 1)
        shifted = [row[17:] + row[:17] for row in layer]  # still tiles: a shifted torus
        self.assertEqual(seam_problems(shifted, "xy"), [])

    def test_distinguishable_materials(self):
        # spec: art-generators: Distinguishable materials
        averages = []
        for layer in self.atlas_layers():
            texels = [p for row in layer for p in row]
            averages.append(oklab(tuple(sum(p[k] for p in texels) / len(texels) for k in range(3))))
        for i in range(len(MATERIALS)):
            for j in range(i + 1, len(MATERIALS)):
                with self.subTest(pair=(MATERIALS[i], MATERIALS[j])):
                    self.assertGreaterEqual(distance(averages[i], averages[j]), 0.04)

    def test_restrained_shading(self):
        # spec: art-generators: Restrained shading
        for name, layer in zip(MATERIALS, self.atlas_layers()):
            colors = {p for row in layer for p in row}
            self.assertTrue(3 <= len(colors) <= 10, "%s uses %d colours" % (name, len(colors)))


class OrnamentTest(ArtTestCase):
    def test_ornament_set(self):
        # spec: art-generators: Ornament set
        directory = BASE / "textures" / "ornaments"
        self.assertGreaterEqual(len(list(directory.glob("*.png"))), 4)
        for name, (width, height, _) in ORNAMENTS.items():
            w, h, _ = read_png(directory / name)
            self.assertEqual((w, h), (width, height), name)
        self.assertEqual({(n, w, h, axes) for n, w, h, axes, _ in gen_ornaments.ORNAMENTS},
                         {(n, w, h, axes) for n, (w, h, axes) in ORNAMENTS.items()})

    def test_seamless_ornament_tiling(self):
        # spec: art-generators: Seamless ornament tiling
        for name, (_, _, axes) in ORNAMENTS.items():
            _, _, rows = read_png(BASE / "textures" / "ornaments" / name)
            with self.subTest(ornament=name):
                self.assertEqual(seam_problems(self.lightness(rows), axes), [])

    def test_ornaments_use_several_palette_families(self):
        for name in ORNAMENTS:
            _, _, rows = read_png(BASE / "textures" / "ornaments" / name)
            self.assertGreaterEqual(len({p for row in rows for p in row}), 4, name)


class PaletteOnlyArtTest(ArtTestCase):
    def test_every_texel_is_a_palette_colour(self):
        # spec: art-generators: Every texel is a palette colour
        files = [BASE / "textures" / "terrain" / "atlas.png"]
        files += [BASE / "textures" / "ornaments" / name for name in ORNAMENTS]
        for path in files:
            _, _, rows = read_png(path)
            foreign = {p for row in rows for p in row} - self.palette_set
            self.assertEqual(foreign, set(), path.name)


class LicensingTest(unittest.TestCase):
    def test_generated_media_are_credited(self):
        # spec: art-generators: Generated media are credited
        credits = {e["file"]: e for e in lua_literal.parse_file(BASE / "credits.lua")}
        for rel in NEW_ART:
            if not rel.endswith(".png"):
                continue
            with self.subTest(file=rel):
                entry = credits[rel]
                self.assertEqual(entry["author"], "Zauberer contributors")
                self.assertEqual(entry["source"], generator_of(rel))
                self.assertEqual(entry["license"], "CC-BY-4.0")
                self.assertTrue((REPO_ROOT / entry["source"]).is_file())

    def test_generated_data_files_are_marked(self):
        # spec: art-generators: Generated data files are marked
        for rel in NEW_ART:
            if not rel.endswith(".lua"):
                continue
            lines = (BASE / rel).read_text(encoding="utf-8").splitlines()
            self.assertEqual(lines[0], "-- SPDX-License-Identifier: CC-BY-4.0", rel)
            self.assertEqual(lines[1], "-- Generated by %s. Do not edit." % generator_of(rel), rel)


class BudgetTest(unittest.TestCase):
    def test_art_fits_the_budget(self):
        # spec: art-generators: Art fits the budget
        total = 0
        for rel in NEW_ART:
            if rel.endswith(".png"):
                width, height, _ = read_png(BASE / rel)
                total += width * height * 4
        self.assertLessEqual(total, 512 * 1024)


if __name__ == "__main__":
    unittest.main()
# REUSE-IgnoreEnd

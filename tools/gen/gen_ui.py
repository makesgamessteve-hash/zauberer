# SPDX-License-Identifier: MIT
"""Generates the UI's art kit: nine-slice frames, the icon atlas and the
palette (assets/base/ui/frames.png, ui/icons.png and the data the scripts
read, scripts/ui/kit_data.lua).

Frames are kilim borders taken from the carpet: an ink edge, a pomegranate
band with saffron diamonds, a turquoise line and knotted corners; light
"thread" frames for toasts, the compass and the gauge; a name plate; key
chips; the book's parchment page; the story band; the focus bar. Each has a
high-contrast twin (opaque fills, a white inner line). Edges and middles are
patterns whose period divides the tile, so the nine-slice tiling never shows
a seam. Icons are 16x16 pixel art from small bitmaps.

All colours come from the base palette's ramps (gen_palette.RAMPS)."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from gen_palette import RAMPS  # noqa: E402
from pngw import write_png  # noqa: E402

_HEX = {name: [tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) for h in steps] for name, steps in RAMPS}


def col(ramp, step, alpha=255):
    """The step-th (1-based, dark to light) colour of a palette ramp."""
    return _HEX[ramp][step - 1] + (alpha,)


CLEAR = (0, 0, 0, 0)
INK = col("night", 1)
WHITE = col("snow", 4)
PALETTE = {
    "ink": col("night", 1), "ink2": col("night", 2), "lapis": col("lapis", 2), "lapis_light": col("lapis", 3),
    "turquoise": col("turquoise", 3), "saffron": col("saffron", 3), "saffron_light": col("saffron", 4),
    "pomegranate": col("pomegranate", 3), "pomegranate_dark": col("pomegranate", 2), "ochre": col("ochre", 3),
    "parchment": col("sand", 4), "parchment_dark": col("sand", 2), "cream": col("snow", 3), "grey": col("stone", 5),
    "white": WHITE,
}
# The mana colours with their glyphs; one set per colour-vision mode (glyphs unchanged).
MANA_ORDER = ["fire", "storm", "light", "life", "water", "earth", "void", "decay"]
GLYPHS = {"fire": "*", "storm": "~", "light": "^", "life": "+", "water": "=", "earth": "#", "void": "o", "decay": "%"}
MANA = {
    "normal": {"fire": col("pomegranate", 4), "storm": col("violet", 3), "light": col("saffron", 4),
               "life": col("green", 5), "water": col("lapis", 4), "earth": col("brown", 5), "void": col("night", 4),
               "decay": col("olive", 4)},
    # Kept apart for red-green and blue-yellow colour vision (brightness steps as well as hue).
    "deuteranopia": {"fire": col("saffron", 2), "storm": col("violet", 3), "light": col("snow", 4),
                     "life": col("lapis", 4), "water": col("sky", 2), "earth": col("brown", 3),
                     "void": col("night", 3), "decay": col("stone", 4)},
    "protanopia": {"fire": col("saffron", 3), "storm": col("violet", 2), "light": col("snow", 4),
                   "life": col("sky", 3), "water": col("lapis", 2), "earth": col("brown", 4),
                   "void": col("night", 3), "decay": col("stone", 5)},
    "tritanopia": {"fire": col("pomegranate", 4), "storm": col("night", 4), "light": col("snow", 4),
                   "life": col("turquoise", 4), "water": col("turquoise", 1), "earth": col("brown", 3),
                   "void": col("snow", 1), "decay": col("pomegranate", 1)},
}
MIN_MANA_DISTANCE = 40  # the smallest RGB distance between two mana colours of a set


def _hash(x, y, salt):
    """A small integer hash for grain (deterministic, platform-independent)."""
    h = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return h ^ (h >> 16)


# --- Frames ---------------------------------------------------------------------------------

def _canvas(w, h, fill=CLEAR):
    return [[fill for _ in range(w)] for _ in range(h)]


def _edge_dist(x, y, w, h):
    return min(x, y, w - 1 - x, h - 1 - y)


def kilim(contrast):
    """16x16, border 6: ink edge, a pomegranate band with saffron diamonds every 4 px, a turquoise line,
    knotted corners; a translucent ink middle (opaque in high contrast)."""
    w = h = 16
    fill = INK if contrast else col("night", 1, 210)
    line = WHITE if contrast else col("turquoise", 3)
    px = _canvas(w, h, fill)
    for y in range(h):
        for x in range(w):
            d = _edge_dist(x, y, w, h)
            along = y if min(x, w - 1 - x) == d else x  # position along the nearest edge
            if d == 0:
                px[y][x] = INK
            elif d <= 3:
                band = col("pomegranate", 3)
                if d == 2 and along % 4 == 1:
                    band = col("saffron", 3)
                elif d == 2 and along % 4 == 3:
                    band = col("pomegranate", 2)
                px[y][x] = band
            elif d == 4:
                px[y][x] = line
    for cx, cy in ((0, 0), (11, 0), (0, 11), (11, 11)):  # the knots
        for y in range(5):
            for x in range(5):
                edge = x in (0, 4) or y in (0, 4)
                px[cy + y][cx + x] = INK if edge else col("saffron", 3)
    return px, (6, 6, 6, 6)


def thread(contrast):
    """16x16, border 3: an ink edge and a saffron line, small knots; a lighter translucent middle."""
    w = h = 16
    fill = INK if contrast else col("night", 1, 185)
    line = WHITE if contrast else col("saffron", 3)
    px = _canvas(w, h, fill)
    for y in range(h):
        for x in range(w):
            d = _edge_dist(x, y, w, h)
            if d == 0:
                px[y][x] = INK
            elif d == 1:
                px[y][x] = line
    for cx, cy in ((0, 0), (13, 0), (0, 13), (13, 13)):
        for y in range(3):
            for x in range(3):
                px[cy + y][cx + x] = line if (x, y) != (1, 1) else INK
    return px, (3, 3, 3, 3)


def plate(contrast):
    """16x16, border 3: a pomegranate name plate edged in saffron."""
    w = h = 16
    px = _canvas(w, h, INK if contrast else col("pomegranate", 2))
    for y in range(h):
        for x in range(w):
            d = _edge_dist(x, y, w, h)
            if d == 0:
                px[y][x] = INK
            elif d == 1:
                px[y][x] = WHITE if contrast else col("saffron", 3)
    return px, (3, 3, 3, 3)


def chip(contrast):
    """8x8, border 2: a key or button chip, saffron-edged ink."""
    w = h = 8
    px = _canvas(w, h, INK)
    for y in range(h):
        for x in range(w):
            if _edge_dist(x, y, w, h) == 0:
                px[y][x] = WHITE if contrast else col("saffron", 3)
    for cx, cy in ((0, 0), (7, 0), (0, 7), (7, 7)):  # rounded corners
        px[cy][cx] = CLEAR
    return px, (2, 2, 2, 2)


def page(contrast):
    """32x32, border 8: the book's parchment, darker towards the edge, grain tiling every 16 px."""
    w = h = 32
    px = _canvas(w, h)
    light, mid, dark = col("sand", 4), col("sand", 3), col("sand", 1)
    for y in range(h):
        for x in range(w):
            if contrast:
                px[y][x] = WHITE
                continue
            d = _edge_dist(x, y, w, h)
            grain = _hash(x % 16, y % 16, 7) % 100 < 10
            c = mid if grain else light
            if d < 8:
                k = (8 - d) / 8.0 * 0.35
                c = tuple(int(c[i] * (1 - k) + dark[i] * k) for i in range(3)) + (255,)
            px[y][x] = c
    return px, (8, 8, 8, 8)


def band(contrast):
    """16x16, borders 3 above and below: the story's ink band with saffron dots along its edges."""
    w = h = 16
    px = _canvas(w, h, INK if contrast else col("night", 1, 175))
    for x in range(w):
        for y in (0, h - 1):
            px[y][x] = (WHITE if contrast else col("saffron", 3)) if x % 4 in (0, 1) else CLEAR
        px[1][x] = px[h - 2][x] = CLEAR
    return px, (0, 3, 0, 3)


def focus(contrast):
    """8x8, border 2: the focused row, lapis with a saffron edge."""
    w = h = 8
    px = _canvas(w, h, WHITE if contrast else col("lapis", 2, 235))
    for y in range(h):
        for x in range(w):
            if _edge_dist(x, y, w, h) == 0:
                px[y][x] = INK if contrast else col("saffron", 3)
    return px, (2, 2, 2, 2)


FRAMES = [("kilim", kilim), ("thread", thread), ("plate", plate), ("chip", chip), ("page", page),
          ("band", band), ("focus", focus)]


# --- Icons ----------------------------------------------------------------------------------

_K = {"k": INK, "r": col("pomegranate", 3), "R": col("pomegranate", 5), "d": col("pomegranate", 2),
      "s": col("saffron", 3), "S": col("saffron", 4), "o": col("ochre", 3), "O": col("ochre", 2),
      "t": col("turquoise", 3), "T": col("turquoise", 4), "b": col("lapis", 4), "B": col("sky", 3),
      "g": col("green", 4), "G": col("green", 5), "w": col("snow", 3), "W": col("snow", 4),
      "l": col("lapis", 2), "n": col("brown", 3), "N": col("brown", 4), "p": col("sand", 3),
      "e": col("stone", 4), "E": col("stone", 6), "v": col("violet", 3), "y": col("saffron", 4)}

# 12x12 bitmaps, drawn at (2, 2) of a 16x16 cell with an ink outline added around them.
ICONS = {
    "health": [  # a pomegranate
        "....g..g....", ".....gg.....", "...rrrrrr...", "..rRRrrrrr..", ".rRRrrrrrrr.", ".rRrrrrrrrr.",
        ".rrrrrrrrrr.", ".rrrrrrrrrd.", "..rrrrrrdd..", "..drrrrrdd..", "...ddddd....", "............"],
    "thirst": [  # a drop
        ".....b......", ".....b......", "....bbb.....", "....bbb.....", "...bBbbb....", "..bBBbbbb...",
        "..bBbbbbb...", "..bbbbbbb...", "..bbbbbbl...", "...bbbbl....", "....lll.....", "............"],
    "hunger": [  # flatbread
        "............", "...oooooo...", "..oSSSSSSo..", ".oSSsSSsSSo.", ".oSSSSSSSSo.", ".oSsSSSSsSo.",
        ".oSSSSsSSSo.", ".oSSSSSSSSo.", "..oSSsSSSo..", "...oooooo...", "............", "............"],
    "rest": [  # a crescent moon
        "....yyyy....", "...yy.......", "..yy........", ".yy.........", ".yy.........", ".yy.........",
        ".yy.........", ".yyy........", "..yyy....y..", "...yyyyyy...", "............", "............"],
    "cold": [  # a snowflake
        ".....W......", "..W..W..W...", "...W.W.W....", "....WWW.....", "WWWWWWWWWWW.", "....WWW.....",
        "...W.W.W....", "..W..W..W...", ".....W......", "............", "............", "............"],
    "heat": [  # a sun
        ".....s......", ".s...s...s..", "..s.....s...", "....SSS.....", "...SyyyS....", "ss.SyyyS.ss.",
        "...SyyyS....", "....SSS.....", "..s.....s...", ".s...s...s..", ".....s......", "............"],
    "flame": [
        ".....r......", "....rr......", "....rrr.....", "...rrsr.....", "...rrssr....", "..rrsssrr...",
        "..rssyssr...", ".rrsyyysrr..", ".rssyyyssr..", "..rsyyysr...", "...rrrrr....", "............"],
    "spark": [
        ".....y......", ".....y......", "..y..y..y...", "...y.y.y....", "....yWy.....", "yyyyWWWyyyy.",
        "....yWy.....", "...y.y.y....", "..y..y..y...", ".....y......", ".....y......", "............"],
    "freeze": [
        ".....T......", "..T..T..T...", "...T.T.T....", "....TWT.....", "TTTTWWWTTTT.", "....TWT.....",
        "...T.T.T....", "..T..T..T...", ".....T......", "............", "............", "............"],
    "light": [
        "............", ".....y......", "....yWy.....", "...yWWWy....", "..yWWWWWy...", "...yWWWy....",
        "....yWy.....", ".....y......", "............", "............", "............", "............"],
    "unmake": [
        "............", ".e.......e..", "..e.....e...", "...e...e....", "....e.e.....", ".....e......",
        "....e.e.....", "...e...e....", "..e.....e...", ".e.......e..", "............", "............"],
    "wind": [
        "............", "..ttttt.....", ".......t....", "tttttttt....", "............", "..tttttttt..",
        "..........t.", "..tttttttt..", "............", "...tttt.....", "......t.....", "............"],
    "shield": [
        "............", "..tttttttt..", "..tTTTTTTt..", "..tTTttTTt..", "..tTTttTTt..", "..tTttttTt..",
        "..tTTttTTt..", "...tTTTTt...", "....tTTt....", ".....tt.....", "............", "............"],
    "carpet": [
        "............", "ssssssssssss", "srrrrrrrrrrs", "srlllsslllrs", "srlssyysslrs", "srlllsslllrs",
        "srrrrrrrrrrs", "ssssssssssss", "s.s.s.s.s.s.", "............", "............", "............"],
    "bag": [
        "....nnnn....", "...n....n...", "...n....n...", "..NNNNNNNN..", ".NoooooooON.", ".NooorroooN.",
        ".NooorroooN.", ".NoooooooON.", ".NoooooooON.", "..NNNNNNNN..", "............", "............"],
    "coin": [
        "............", "...ssss.....", "..sSSSSs....", ".sSSssSSs...", ".sSsSSsSs...", ".sSsSSsSs...",
        ".sSSssSSs...", "..sSSSSs....", "...ssss.....", "............", "............", "............"],
    "herb": [
        "............", ".....G......", "....GgG.....", "..G.GgG.G...", "..GGgggGG...", "...GGgGG....",
        ".....g......", ".....g......", ".....g......", "............", "............", "............"],
    "meat": [
        "............", "...RRRR.....", "..RrrrrR....", ".RrrRRrrR...", ".RrrrrrrrR..", "..RrrrrrrR..",
        "...RRRRRR...", ".......ww...", "........ww..", "............", "............", "............"],
    "water_skin": [
        "....nn......", "....nn......", "...nbbn.....", "..nbBbbn....", "..nbbbbbn...", "..nbbbbbn...",
        "..nbbbbbn...", "...nbbbn....", "....nnn.....", "............", "............", "............"],
    "embers": [
        "..r..r......", ".r..r.......", "..r..r......", "............", ".oooooooo...", "oSSrSSrSSo..",
        ".oooooooo...", "............", "............", "............", "............", "............"],
    "goal": [
        ".....s......", "....sSs.....", "...sSySs....", "..sSyyySs...", ".sSyyyyySs..", "..sSyyySs...",
        "...sSySs....", "....sSs.....", ".....s......", "............", "............", "............"],
    "place": [
        ".....t......", "....tTt.....", "...tTTTt....", "..tTTTTTt...", "...tTTTt....", "....tTt.....",
        ".....t......", "............", "............", "............", "............", "............"],
    "heir": [
        ".....W......", "....WWW.....", "...WWWWW....", "..WWWWWWW...", ".....W......", ".....W......",
        "............", "............", "............", "............", "............", "............"],
    "ally": [
        ".....B......", "....BBB.....", "...BBBBB....", "..BBBBBBB...", ".....B......", ".....B......",
        "............", "............", "............", "............", "............", "............"],
    "mouse_left": [
        "...eeeee....", "..ess.eee...", "..ess.eee...", "..ess.eee...", "..eeeeeee...", "..eeeeeee...",
        "..eeeeeee...", "..eeeeeee...", "...eeeee....", "............", "............", "............"],
    "mouse_right": [
        "...eeeee....", "..eee.sse...", "..eee.sse...", "..eee.sse...", "..eeeeeee...", "..eeeeeee...",
        "..eeeeeee...", "..eeeeeee...", "...eeeee....", "............", "............", "............"],
    "touch": [
        "....ee......", "....ee......", "....eeee....", "....eeeeee..", "..e.eeeeee..", "..eeeeeeee..",
        "..eeeeeeee..", "...eeeeeee..", "....eeeee...", "............", "............", "............"],
}
# Gamepad faces, shoulders and the mana gems are drawn from shapes.
PAD = {"pad_a": (col("green", 5), "A"), "pad_b": (col("pomegranate", 4), "B"), "pad_x": (col("lapis", 4), "X"),
       "pad_y": (col("saffron", 4), "Y")}
SHOULDERS = ["pad_lb", "pad_rb", "pad_lt", "pad_rt"]


def _blank():
    return _canvas(16, 16)


def _from_bitmap(rows):
    px = _blank()
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            c = _K.get(ch)
            if c:
                px[2 + j][2 + i] = c
    return _outline(px)


def _outline(px):
    """An ink outline around the drawn pixels (so icons read on any ground)."""
    out = [row[:] for row in px]
    for y in range(16):
        for x in range(16):
            if px[y][x][3] == 0 and any(0 <= x + dx < 16 and 0 <= y + dy < 16 and px[y + dy][x + dx][3] > 0
                                        and px[y + dy][x + dx] != INK
                                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = INK
    return out


def _disc(colour, r=6.0):
    px = _blank()
    for y in range(16):
        for x in range(16):
            dx, dy = x - 7.5, y - 7.5
            if dx * dx + dy * dy <= r * r:
                px[y][x] = colour
    return px


def _gem(colour):
    """A mana gem: a faceted diamond in the colour with a light facet."""
    px = _blank()
    light = tuple(min(255, c + 70) for c in colour[:3]) + (255,)
    for y in range(16):
        for x in range(16):
            d = abs(x - 7.5) + abs(y - 7.5)
            if d <= 6.5:
                px[y][x] = light if (x < 8 and y < 8 and d > 3.5) else colour
    return _outline(px)


# 5x5 letters for the pad faces.
_LETTERS = {"A": [".###.", "#...#", "#####", "#...#", "#...#"], "B": ["####.", "#...#", "####.", "#...#", "####."],
            "X": ["#...#", ".#.#.", "..#..", ".#.#.", "#...#"], "Y": ["#...#", ".#.#.", "..#..", "..#..", "..#.."],
            "L": ["#....", "#....", "#....", "#....", "#####"], "R": ["####.", "#...#", "####.", "#..#.", "#...#"]}


def _letter(px, ch, ox, oy, colour):
    for j, row in enumerate(_LETTERS[ch]):
        for i, c in enumerate(row):
            if c == "#":
                px[oy + j][ox + i] = colour


def icon_cells():
    cells = [(name, _from_bitmap(rows)) for name, rows in ICONS.items()]
    for name, (colour, letter) in PAD.items():
        px = _disc(INK, 6.5)
        inner = _disc(colour, 5.5)
        for y in range(16):
            for x in range(16):
                if inner[y][x][3]:
                    px[y][x] = colour
        _letter(px, letter, 6, 5, INK)
        cells.append((name, px))
    for name in SHOULDERS:  # pad_lb, pad_rb, pad_lt, pad_rt: a shoulder button with its side and kind
        px = _blank()
        for y in range(4, 12):
            for x in range(1, 15):
                px[y][x] = INK if (y in (4, 11) or x in (1, 14)) else col("stone", 5)
        _letter(px, name[4].upper(), 3, 6, INK)
        if name[5] == "b":
            _letter(px, "B", 9, 6, INK)
        else:
            _trigger(px)
        cells.append((name, px))
    for mana in MANA_ORDER:
        cells.append(("mana_" + mana, _gem(MANA["normal"][mana])))
    return cells


def _trigger(px):
    for j in range(5):
        px[6 + j][10] = INK
    for i in range(9, 12):
        px[6][i] = INK


# --- Writing ------------------------------------------------------------------------------

def _paste(dst, src, ox, oy):
    for y, row in enumerate(src):
        for x, c in enumerate(row):
            dst[oy + y][ox + x] = c


def _lua_colour(c):
    return "{ %.3f, %.3f, %.3f, %.3f }" % tuple(v / 255.0 for v in c)


def build():
    """The frame atlas, the icon atlas and the kit's data (Lua source)."""
    # Frames: normal in the top half, high contrast below; cells laid left to right.
    frames, layout, x = [], {}, 0
    for name, fn in FRAMES:
        normal, border = fn(False)
        contrast, _ = fn(True)
        w, h = len(normal[0]), len(normal)
        frames.append((name, normal, contrast, x, w, h, border))
        x += w
    fw, fh = x, 32
    fpx = _canvas(fw, fh * 2)
    for name, normal, contrast, ox, w, h, border in frames:
        _paste(fpx, normal, ox, 0)
        _paste(fpx, contrast, ox, fh)
        layout[name] = (ox, w, h, border)
    cells = icon_cells()
    cols = 8
    rows = (len(cells) + cols - 1) // cols
    ipx = _canvas(cols * 16, rows * 16)
    for i, (_, px) in enumerate(cells):
        _paste(ipx, px, (i % cols) * 16, (i // cols) * 16)
    lines = ["-- SPDX-" + "License-Identifier: MIT",  # (split so REUSE does not read this file's own line)
             "-- Generated by tools/gen/gen_ui.py. Do not edit.",
             "return {", '  frames_texture = "ui/frames.png",', '  icons_texture = "ui/icons.png",',
             "  contrast_offset = %d, -- (high-contrast frames lie this far below)" % fh, "  frames = {"]
    for name, (ox, w, h, border) in layout.items():
        lines.append("    %s = { src = { %d, 0, %d, %d }, border = { %d, %d, %d, %d } }," % (
            name, ox, w, h, *border))
    lines += ["  },", "  icon_size = 16,", "  icons = {"]
    for i, (name, _) in enumerate(cells):
        lines.append("    %s = { %d, %d }," % (name, (i % cols) * 16, (i // cols) * 16))
    lines += ["  },", "  palette = {"]
    for name, c in PALETTE.items():
        lines.append("    %s = %s," % (name, _lua_colour(c)))
    lines += ["  },", "  mana_order = { %s }," % ", ".join('"%s"' % m for m in MANA_ORDER),
              "  glyphs = { %s }," % ", ".join('%s = "%s"' % (m, GLYPHS[m]) for m in MANA_ORDER),
              "  min_mana_distance = %d," % MIN_MANA_DISTANCE, "  mana = {"]
    for mode, colours in MANA.items():
        lines.append("    %s = {" % mode)
        for m in MANA_ORDER:
            lines.append("      %s = %s," % (m, _lua_colour(colours[m])))
        lines.append("    },")
    lines += ["  },", "}", ""]
    return (fw, fh * 2, fpx), (cols * 16, rows * 16, ipx), "\n".join(lines)


def check_mana_sets():
    """Every mode's mana colours stay MIN_MANA_DISTANCE apart (raises otherwise)."""
    for mode, colours in MANA.items():
        names = list(colours)
        for i, a in enumerate(names):
            for b in names[i + 1:]:
                d = sum((colours[a][k] - colours[b][k]) ** 2 for k in range(3)) ** 0.5
                if d < MIN_MANA_DISTANCE:
                    raise ValueError(f"{mode}: {a} and {b} are too close ({d:.0f})")


def main(base):
    check_mana_sets()
    (fw, fh, fpx), (iw, ih, ipx), data = build()
    os.makedirs(os.path.join(base, "ui"), exist_ok=True)
    write_png(os.path.join(base, "ui", "frames.png"), fw, fh, fpx)
    write_png(os.path.join(base, "ui", "icons.png"), iw, ih, ipx)
    os.makedirs(os.path.join(base, "scripts", "ui"), exist_ok=True)
    with open(os.path.join(base, "scripts", "ui", "kit_data.lua"), "w", encoding="utf-8") as f:
        f.write(data)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "assets", "base"))

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Mockups of the proposed UI (openspec/changes/redesign-*), drawn over real frames.

Everything is drawn on the game's UI grid (640x360, the default internal
resolution) in the game's own pixel font, then scaled up 3x with
nearest-neighbour, so a mockup looks the way the game would draw it. The
frames, icons and ornaments here are prototypes of what tools/gen/gen_ui.py
is to generate (see the add-ui-art-kit change).

    python3 tools/design/ui_mockups.py --frames DIR --hud-frame PNG --out docs/design/ui

--frames is the landscape footage of tools/reels/reel.lua (reel_00001.png ...);
--hud-frame a frame of the current HUD for the before/after.
"""
from __future__ import annotations

import argparse
import math
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "reels"))
import make_reels as mr  # noqa: E402

W, H = 640, 360
INK = (22, 16, 38)
LAPIS = (38, 62, 140)
LAPIS_D = (26, 38, 92)
TURQ = (56, 178, 170)
SAFFRON = (242, 184, 64)
POME = (176, 36, 56)
POME_D = (112, 22, 40)
OCHRE = (196, 128, 52)
PARCH = (234, 216, 172)
PARCH_D = (204, 178, 128)
PARCH_DD = (150, 120, 80)
CREAM = (250, 240, 214)
MANA = {"fire": (226, 72, 40), "storm": (150, 96, 220), "light": (250, 222, 110), "life": (96, 190, 80),
        "water": (60, 140, 230), "earth": (160, 112, 64), "void": (90, 80, 110), "decay": (130, 150, 60)}
GLYPH = {"fire": "*", "storm": "~", "light": "^", "life": "+", "water": "=", "earth": "#", "void": "o",
         "decay": "%"}

FONT = mr.PixelFont()

# 9x9 icons: . clear, a..z from the palette below.
ICON_COL = {"k": INK, "r": POME, "R": (230, 80, 90), "s": SAFFRON, "o": OCHRE, "t": TURQ, "b": MANA["water"],
            "B": (150, 200, 250), "g": MANA["life"], "w": CREAM, "p": PARCH_D, "l": LAPIS, "y": MANA["light"],
            "f": MANA["fire"]}
ICONS = {
    "pomegranate": ["...g.g...", "....g....", "..rrrrr..", ".rRrrrrr.", ".rRrrrrr.", ".rrrrrrr.", ".rrrrrrr.",
                    "..rrrrr..", "...rrr..."],
    "drop": ["....b....", "....b....", "...bbb...", "...bBb...", "..bbBbb..", "..bBbbb..", "..bbbbb..",
             "...bbb...", "........."],
    "bread": [".........", "..ooooo..", ".ossssso.", "ossosssso", "osssosso.", ".osssssso", "..ooooo..",
              ".........", "........."],
    "moon": ["...yyy...", "..yy.....", ".yy......", ".yy......", ".yy......", ".yy......", "..yy.....",
             "...yyy...", "........."],
    "flame": ["....f....", "...ff....", "...fff...", "..fffs...", "..ffssf..", ".ffsysff.", ".fssyssf.",
              "..fsysf..", "...fff..."],
    "spark": ["....y....", "....y....", "..y.y.y..", "...yyy...", "yyyywyyyy", "...yyy...", "..y.y.y..",
              "....y....", "....y...."],
    "sun": ["....s....", ".s..s..s.", "..sssss..", "..syyys..", "sssyyysss",
            "..syyys..", "..sssss..", ".s..s..s.", "....s...."],
    "carpet": [".........", "sssssssss", "srrrrrrrs", "srllsllrs", "srlsyslrs", "srllsllrs", "srrrrrrrs",
               "sssssssss", "s.s.s.s.s"],
    "bag": ["...ooo...", "..o...o..", ".ooooooo.", "osssssso.", "ossrrsso.", "osssssso.", "osssssso.",
            ".oooooo..", "........."],
    "warm": ["..f..f...", ".f..f....", "..f..f...", ".........", ".ooooooo.", "osssssso.", ".ooooooo.",
             ".........", "........."],
}


def put_icon(img: Image.Image, name: str, x: int, y: int) -> None:
    rows = ICONS[name]
    for j, row in enumerate(rows):
        for i, ch in enumerate(row[:9]):
            col = ICON_COL.get(ch)
            if col:
                img.putpixel((x + i, y + j), col + (255,))


def text(img: Image.Image, s: str, x: int, y: int, col=CREAM, shadow=INK, alpha=255) -> int:
    """Text at (x, y) at 1x with a one-pixel shadow; returns its width."""
    m = FONT.mask(s)
    if shadow:
        img.paste(Image.new("RGBA", m.size, shadow + (alpha,)), (x + 1, y + 1), m)
    img.paste(Image.new("RGBA", m.size, col + (alpha,)), (x, y), m)
    return m.width


def text_c(img, s, cx, y, col=CREAM, shadow=INK):
    return text(img, s, cx - FONT.width(s, 1) // 2, y, col, shadow)


def panel(img: Image.Image, box, fill=INK + (205,), border=True, kilim=True) -> None:
    """A panel in a carpet border: ink edge, a pomegranate kilim band with saffron diamonds, a turquoise line."""
    x0, y0, x1, y1 = box
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)
    d.rectangle(box, fill=fill)
    if border:
        d.rectangle(box, outline=INK + (255,))
        if kilim:
            d.rectangle((x0 + 1, y0 + 1, x1 - 1, y1 - 1), outline=POME + (255,), width=3)
            for x in range(x0 + 3, x1 - 2, 6):  # saffron diamonds along the band
                for yy in (y0 + 2, y1 - 2):
                    d.point([(x, yy)], fill=SAFFRON + (255,))
                    d.point([(x + 3, yy)], fill=POME_D + (255,))
            for y in range(y0 + 3, y1 - 2, 6):
                for xx in (x0 + 2, x1 - 2):
                    d.point([(xx, y)], fill=SAFFRON + (255,))
                    d.point([(xx, y + 3)], fill=POME_D + (255,))
            d.rectangle((x0 + 4, y0 + 4, x1 - 4, y1 - 4), outline=TURQ + (255,))
        else:
            d.rectangle((x0 + 1, y0 + 1, x1 - 1, y1 - 1), outline=SAFFRON + (200,))
        for cx, cy in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):  # corner knots
            d.rectangle((cx - 2, cy - 2, cx + 2, cy + 2), fill=SAFFRON + (255,), outline=INK + (255,))
    img.alpha_composite(ov)


def parchment(img: Image.Image, box, seed=1) -> None:
    """A parchment page: warm paper with grain, darker towards the edges."""
    x0, y0, x1, y1 = box
    rnd = random.Random(seed)
    page = Image.new("RGBA", (x1 - x0 + 1, y1 - y0 + 1), PARCH + (255,))
    px = page.load()
    w, h = page.size
    for y in range(h):
        for x in range(w):
            e = min(x, y, w - 1 - x, h - 1 - y)
            k = max(0.0, 1 - e / 14)
            n = rnd.random()
            c = PARCH if n > 0.12 else PARCH_D
            c = tuple(int(c[i] * (1 - 0.35 * k) + PARCH_DD[i] * 0.35 * k) for i in range(3))
            px[x, y] = c + (255,)
    img.alpha_composite(page, (x0, y0))


def chip(img, label: str, x: int, y: int, key=True) -> int:
    """A key or button chip: the binding in a small saffron-edged box."""
    w = FONT.width(label, 1) + 6
    d = ImageDraw.Draw(img)
    d.rectangle((x, y, x + w, y + 11), fill=INK + (235,), outline=SAFFRON + (255,) if key else TURQ + (255,))
    text(img, label, x + 3, y + 2, SAFFRON if key else CREAM, None)
    return w


def bar(img, x, y, w, h, frac, col, back=INK) -> None:
    d = ImageDraw.Draw(img)
    d.rectangle((x, y, x + w, y + h), fill=back + (230,), outline=INK + (255,))
    fw = int((w - 2) * max(0, min(1, frac)))
    if fw > 0:
        d.rectangle((x + 1, y + 1, x + fw, y + h - 1), fill=col + (255,))
        d.line((x + 1, y + 1, x + fw, y + 1), fill=tuple(min(255, c + 60) for c in col) + (255,))


def mana_ring(img, cx, cy, r, amounts: dict[str, int], cap: int) -> None:
    """The mana ring on the wrist: one segment per colour, its glyph at the segment's middle."""
    d = ImageDraw.Draw(img)
    d.ellipse((cx - r - 3, cy - r - 3, cx + r + 3, cy + r + 3), fill=INK + (230,), outline=SAFFRON + (255,))
    total = sum(amounts.values())
    start = -90.0
    for name, v in amounts.items():
        sweep = 360.0 * v / max(cap, total)
        d.pieslice((cx - r, cy - r, cx + r, cy + r), start, start + sweep, fill=MANA[name] + (255,))
        mid = math.radians(start + sweep / 2)
        gx, gy = cx + math.cos(mid) * (r + 9), cy + math.sin(mid) * (r + 9)
        if sweep > 25:
            text(img, GLYPH[name], int(gx) - 2, int(gy) - 4, MANA[name])
        start += sweep
    d.ellipse((cx - r + 5, cy - r + 5, cx + r - 5, cy + r - 5), fill=INK + (255,), outline=TURQ + (255,))
    text_c(img, str(total), cx, cy - 4, CREAM)


def medallion(img, cx, cy, icon: str, name: str, key: str, charge=0.0, ready=True) -> None:
    """A hand: the weave in it as an illuminated medallion, its name and binding under it."""
    d = ImageDraw.Draw(img)
    r = 13
    d.ellipse((cx - r - 2, cy - r - 2, cx + r + 2, cy + r + 2), fill=INK + (255,))
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=LAPIS_D + (255,), outline=SAFFRON + (255,))
    if charge > 0:
        d.arc((cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1), -90, -90 + 360 * charge, fill=CREAM + (255,), width=2)
    for k in range(8):  # star ornament
        a = k * math.pi / 4
        d.point([(cx + math.cos(a) * (r - 3), cy + math.sin(a) * (r - 3))], fill=TURQ + (255,))
    big = Image.new("RGBA", (9, 9), (0, 0, 0, 0))
    put_icon(big, icon, 0, 0)
    big = big.resize((18, 18), Image.NEAREST)
    img.alpha_composite(big, (cx - 9, cy - 9))
    text_c(img, name, cx, cy + r + 5, CREAM if ready else PARCH_D)
    w = FONT.width(key, 1) + 6
    chip(img, key, cx - w // 2, cy + r + 15)


def compass(img, cx, y, heading: float, marks) -> None:
    """The woven compass band: headings, the goal and places as small marks, the time of day in its knot."""
    w = 220
    x0 = cx - w // 2
    panel(img, (x0, y, x0 + w, y + 16), INK + (190,), True, False)
    d = ImageDraw.Draw(img)
    span = 140.0  # degrees across the band
    for deg in range(0, 360, 15):
        off = ((deg - heading + 540) % 360) - 180
        if abs(off) > span / 2:
            continue
        x = int(cx + off / span * (w - 16))
        if deg % 90 == 0:
            text_c(img, "NESW"[deg // 90], x, y + 5, SAFFRON if deg == 0 else CREAM)
        elif deg % 45 == 0:
            d.line((x, y + 6, x, y + 10), fill=PARCH_D + (255,))
        else:
            d.point([(x, y + 9)], fill=PARCH_DD + (255,))
    for bearing, kind, label in marks:
        off = ((bearing - heading + 540) % 360) - 180
        off = max(-span / 2 + 4, min(span / 2 - 4, off))
        x = int(cx + off / span * (w - 16))
        col = SAFFRON if kind == "goal" else TURQ
        d.polygon([(x, y + 17), (x - 3, y + 21), (x, y + 25), (x + 3, y + 21)], fill=col + (255,),
                  outline=INK + (255,))
        if label:
            text_c(img, label, x, y + 27, col)
    d.line((cx, y + 1, cx, y + 3), fill=CREAM + (255,))


def toast(img, x, y, lines, icon=None, alpha=1.0) -> None:
    w = max(FONT.width(s, 1) for s in lines) + (20 if icon else 10)
    h = 6 + len(lines) * 10
    panel(img, (x - w, y, x, y + h), INK + (int(200 * alpha),), True, False)
    if icon:
        put_icon(img, icon, x - w + 5, y + 4)
    for i, s in enumerate(lines):
        text(img, s, x - w + (16 if icon else 5), y + 4 + i * 10, CREAM if i == 0 else PARCH_D)


def base(frames: Path, n: int, dim=0.0, blur=0) -> Image.Image:
    im = Image.open(frames / f"reel_{n:05d}.png").convert("RGB").resize((W, H), Image.NEAREST)
    if blur:
        im = im.filter(ImageFilter.GaussianBlur(blur))
    if dim:
        im = Image.blend(im, Image.new("RGB", im.size, INK), dim)
    return im.convert("RGBA")


def save(img: Image.Image, out: Path, name: str) -> None:
    img.convert("RGB").resize((W * 3, H * 3), Image.NEAREST).save(out / name)
    print(f"wrote {out / name}")


# --- The mockups -------------------------------------------------------------------------------

def flight_hud(img: Image.Image, calm=False) -> Image.Image:
    # Top: the compass band, the time in its knot; the goal and the next place on it.
    compass(img, W // 2, 6, 80.0, [(95.0, "goal", "Anarvan 2.4 km"), (40.0, "place", "")])
    put_icon(img, "sun", W // 2 + 114, 9)
    if not calm:
        # Top left: the objective card.
        panel(img, (6, 6, 178, 60))
        text(img, "The morning chores", 14, 12, SAFFRON)
        for i, (done, s) in enumerate(((True, "Fetch water"), (True, "Feed the goats"),
                                       (False, "Pick pomegranates 1/3"))):
            text(img, ("x " if done else "- ") + s, 14, 25 + i * 10, PARCH_D if done else CREAM)
        # Right: a toast that is fading.
        toast(img, W - 6, 64, ["Taken: pomegranate", "Pack 6.7 of 12 kg"], "pomegranate")
    # Bottom left: health in a carpet frame; the needs only when they want something.
    panel(img, (6, H - 44 + (12 if calm else 0), 146, H - 6))
    put_icon(img, "pomegranate", 13, H - 20 - (0 if calm else 12))
    bar(img, 25, H - 19 - (0 if calm else 12), 112, 7, 0.82, POME)
    if not calm:
        put_icon(img, "drop", 13, H - 20)
        text(img, "Thirsty", 25, H - 19, SAFFRON)
        put_icon(img, "moon", 80, H - 20)
        text(img, "Tired", 92, H - 19, PARCH_D)
    # Bottom middle: the two hands and the mana ring between them.
    cx = W // 2
    mana_ring(img, cx, H - 50, 17, {"fire": 35, "storm": 40, "light": 20, "life": 10, "water": 60}, 200)
    medallion(img, cx - 60, H - 56, "flame", "Fireball", "LMB")
    medallion(img, cx + 60, H - 56, "spark", "Kindle", "RMB", charge=0.6)
    # Right: the carpet gauge, a hanging tassel: height above ground and speed.
    gx = W - 22
    panel(img, (gx - 12, 150, gx + 14, 302), INK + (170,), True, False)
    d = ImageDraw.Draw(img)
    for k in range(0, 11):
        yy = 280 - k * 12
        d.line((gx - 6, yy, gx - 2 + (3 if k % 5 == 0 else 0), yy), fill=PARCH_D + (255,))
    yy = 280 - int(30 / 100 * 120)
    d.polygon([(gx + 1, yy), (gx + 7, yy - 3), (gx + 7, yy + 3)], fill=SAFFRON + (255,))
    text_c(img, "30m", gx + 1, 155, CREAM)
    put_icon(img, "carpet", gx - 4, 268 + 4)
    text_c(img, "34", gx + 1, 290, SAFFRON)
    if not calm:
        # The contextual prompt above the hands.
        w = chip(img, "E", cx - 30, H - 104)
        text(img, "Step off", cx - 30 + w + 4, H - 102, CREAM)
    return img


def dialogue(img: Image.Image) -> Image.Image:
    compass(img, W // 2, 6, 10.0, [(355.0, "goal", "Farrokh")])
    # The speaker's plate and the words, low in the frame, the world left open above.
    panel(img, (60, H - 98, W - 60, H - 12))
    panel(img, (72, H - 110, 172, H - 94), POME_D + (255,), True, False)
    text_c(img, "Farrokh", 122, H - 106, SAFFRON)
    lines = ["\"Your father's carpet does not like strangers.",
             "Be kind to it, and fetch the water before the",
             "sun is high. The goats will not wait either.\""]
    for i, s in enumerate(lines):
        text(img, s, 78, H - 86 + i * 11, CREAM)
    for i, (k, s) in enumerate((("1", "I'll fetch it now."), ("2", "Tell me about my father."),
                                ("3", "Why does the carpet not like me?"))):
        y = H - 48 + i * 11
        if i == 0:
            ImageDraw.Draw(img).rectangle((76, y - 2, 330, y + 8), fill=LAPIS + (230,))
        chip(img, k, 78, y - 2)
        text(img, s, 92, y, CREAM if i == 0 else PARCH_D)
    return img


def chapter(img: Image.Image) -> Image.Image:
    # A turn of the story: a title card in the middle, the ornament drawn out from its centre.
    d = ImageDraw.Draw(img)
    cy = 120
    d.rectangle((0, cy - 34, W, cy + 34), fill=INK + (170,))
    for x in range(0, W, 6):
        d.point([(x, cy - 34)], fill=SAFFRON + (255,))
        d.point([(x + 3, cy + 34)], fill=SAFFRON + (255,))
    text_c(img, "ACT I", W // 2, cy - 24, TURQ)
    t = Image.new("RGBA", (W, 40), (0, 0, 0, 0))
    text_c(t, "The Ashes of Anarvan", W // 2 // 2, 0, SAFFRON)
    t = t.crop((0, 0, W // 2, 10)).resize((W, 20), Image.NEAREST)
    img.alpha_composite(t, (0, cy - 10))
    text_c(img, "Day 2 of spring, dawn", W // 2, cy + 16, PARCH_D)
    # Under it, the new goal arrives in the objective card's place.
    panel(img, (6, 6, 196, 40))
    text(img, "New goal", 14, 12, TURQ)
    text(img, "Bury the dead of Anarvan", 14, 24, CREAM)
    compass(img, W // 2 + 120, 6, 200.0, [(210.0, "goal", "")])
    return img


def book(img: Image.Image, tab: str) -> Image.Image:
    """The menu as the father's book: two pages, tabs above, the world dim behind."""
    x0, y0, x1, y1 = 40, 34, W - 40, H - 22
    panel(img, (x0 - 6, y0 - 6, x1 + 6, y1 + 6), LAPIS_D + (255,))
    parchment(img, (x0, y0, W // 2 - 2, y1), 3)
    parchment(img, (W // 2 + 2, y0, x1, y1), 4)
    d = ImageDraw.Draw(img)
    d.line((W // 2, y0, W // 2, y1), fill=PARCH_DD + (255,), width=2)
    tabs = ["Journal", "Map", "Bag", "Weaves", "Codex", "Settings"]
    tx = x0
    for t in tabs:
        w = FONT.width(t, 1) + 12
        sel = t == tab
        d.rectangle((tx, y0 - 22 + (0 if sel else 3), tx + w, y0 - 7), fill=(PARCH if sel else POME_D) + (255,),
                    outline=INK + (255,))
        text(img, t, tx + 6, y0 - 18 + (0 if sel else 2), INK if sel else PARCH, None)
        tx += w + 3
    chip(img, "Q", x0 - 22, y0 - 21)
    chip(img, "E", tx + 4, y0 - 21)
    hint_y = y1 + 10
    x = x0
    for k, s in (("Enter", "Use"), ("X", "Drop"), ("Esc", "Back to the world")):
        x += chip(img, k, x, hint_y) + 4
        x += text(img, s, x, hint_y + 2, CREAM) + 12
    return img


def bag_page(img: Image.Image) -> Image.Image:
    book(img, "Bag")
    x0, y0 = 52, 44
    text(img, "Pack  6.7 of 12 kg", x0, y0, INK, None)
    bar(img, x0, y0 + 11, 220, 4, 6.7 / 12, OCHRE, PARCH_D)
    items = [("pomegranate", "Pomegranate", 3), ("drop", "River water", 2), ("drop", "Boiled water", 2),
             ("carpet", "Rug", 1), ("bread", "Flatbread", 4), ("warm", "Embers", 1), ("bag", "Endless bag", 1)]
    d = ImageDraw.Draw(img)
    for i, (icon, name, n) in enumerate(items):
        cx, cy = x0 + (i % 6) * 38, y0 + 24 + (i // 6) * 38
        sel = i == 0
        d.rectangle((cx, cy, cx + 32, cy + 32), fill=(PARCH_D if not sel else SAFFRON) + (255,),
                    outline=INK + (255,))
        big = Image.new("RGBA", (9, 9), (0, 0, 0, 0))
        put_icon(big, icon, 0, 0)
        img.alpha_composite(big.resize((18, 18), Image.NEAREST), (cx + 7, cy + 5))
        text(img, str(n), cx + 24, cy + 23, INK, None)
    # The right page: what is chosen.
    rx = W // 2 + 14
    big = Image.new("RGBA", (9, 9), (0, 0, 0, 0))
    put_icon(big, "pomegranate", 0, 0)
    img.alpha_composite(big.resize((36, 36), Image.NEAREST), (rx, 48))
    text(img, "Pomegranate", rx + 44, 52, POME_D, None)
    text(img, "Food, 0.3 kg each", rx + 44, 64, INK, None)
    for i, s in enumerate(["From the orchard behind", "Farrokh's house. Eaten", "fresh it eases hunger;",
                           "its juice is an ingredient", "of life weaves."]):
        text(img, s, rx, 96 + i * 11, INK, None)
    for i, (k, s) in enumerate((("Enter", "Eat"), ("C", "Cook at a fire"), ("X", "Drop"))):
        y = 170 + i * 15
        w = chip(img, k, rx, y)
        text(img, s, rx + w + 5, y + 2, INK, None)
    return img


def map_page(img: Image.Image) -> Image.Image:
    book(img, "Map")
    # The left and right pages together hold the map: land drawn by height, unseen land left as paper.
    x0, y0, x1, y1 = 50, 42, W - 50, H - 30
    rnd = random.Random(7)
    m = Image.new("RGBA", (x1 - x0, y1 - y0), (0, 0, 0, 0))
    px = m.load()
    cx, cy = (x1 - x0) * 0.45, (y1 - y0) * 0.55
    for y in range(m.height):
        for x in range(m.width):
            seen = ((x - cx) / 210) ** 2 + ((y - cy) / 120) ** 2 < 1 + 0.15 * math.sin(x * 0.07) * math.cos(y * 0.09)
            if not seen:
                continue
            h = (math.sin(x * 0.045) + math.cos(y * 0.06) + 0.6 * math.sin((x + y) * 0.11)
                 + 0.3 * math.sin(x * 0.23) * math.cos(y * 0.19) - 0.2 + (x - 250) * 0.006)
            if h < -0.2:
                c = (78, 124, 160) if (x + y) % 2 else (70, 112, 150)
            elif h < 0.0:
                c = (214, 196, 140)
            elif h < 1.0:
                c = (128, 150, 84) if rnd.random() > 0.1 else (112, 136, 76)
            elif h < 1.6:
                c = (168, 134, 88)
            else:
                c = (236, 230, 220)
            px[x, y] = c + (235,)
    img.alpha_composite(m, (x0, y0))
    d = ImageDraw.Draw(img)
    # The way flown, the places, the heir, the goal.
    path = [(160, 230), (190, 214), (230, 205), (262, 190), (300, 186), (340, 170)]
    for a, b in zip(path, path[1:]):
        for t in range(0, 10):
            if t % 2 == 0:
                d.point([(a[0] + (b[0] - a[0]) * t / 10, a[1] + (b[1] - a[1]) * t / 10)], fill=POME_D + (255,))
    for (x, y, name) in ((160, 232, "Anarvan"), (262, 192, "Kesh"), (420, 150, "The sanctum")):
        d.rectangle((x - 2, y - 2, x + 2, y + 2), fill=OCHRE + (255,), outline=INK + (255,))
        text(img, name, x + 5, y - 4, INK, PARCH)
    d.polygon([(340, 164), (346, 170), (340, 176), (334, 170)], fill=SAFFRON + (255,), outline=INK + (255,))
    text(img, "You", 349, 166, INK, PARCH)
    d.line((416, 146, 424, 154), fill=POME + (255,), width=2)
    d.line((416, 154, 424, 146), fill=POME + (255,), width=2)
    # The compass rose and the scale.
    rx, ry = W - 80, 70
    d.ellipse((rx - 14, ry - 14, rx + 14, ry + 14), outline=INK + (255,))
    d.polygon([(rx, ry - 16), (rx + 3, ry), (rx, ry + 16), (rx - 3, ry)], fill=POME_D + (255,))
    text_c(img, "N", rx, ry - 26, INK, None)
    d.line((60, H - 42, 110, H - 42), fill=INK + (255,), width=2)
    text(img, "5 km", 114, H - 46, INK, None)
    return img


def title(img: Image.Image) -> Image.Image:
    d = ImageDraw.Draw(img)
    # The name, large, in the carpet's colours, with a woven band under it.
    t = Image.new("RGBA", (W, 12), (0, 0, 0, 0))
    text_c(t, "ZAUBERER", W // 8, 0, SAFFRON, INK)
    t = t.crop((0, 0, W // 4, 10)).resize((W, 40), Image.NEAREST)
    img.alpha_composite(t, (0, 46))
    for x in range(170, W - 170, 6):
        d.rectangle((x, 92, x + 2, 94), fill=POME + (255,))
        d.point([(x + 4, 93)], fill=SAFFRON + (255,))
    text_c(img, "The heir of the carpet", W // 2, 100, CREAM)
    # The choices in a panel, the save's own picture of where it was left.
    panel(img, (W // 2 - 110, 140, W // 2 + 110, 300))
    items = ["Continue", "Create a world", "Load game", "Settings", "Help", "Credits", "Quit"]
    for i, s in enumerate(items):
        y = 154 + i * 18
        if i == 0:
            d.rectangle((W // 2 - 100, y - 4, W // 2 + 100, y + 12), fill=LAPIS + (240,), outline=SAFFRON + (255,))
            text(img, ">", W // 2 - 94, y, SAFFRON)
        text_c(img, s, W // 2, y, CREAM if i == 0 else PARCH_D)
    panel(img, (W // 2 + 124, 140, W - 20, 216), INK + (200,), True, False)
    text(img, "Nasrin's world", W // 2 + 132, 148, SAFFRON)
    for i, s in enumerate(["Day 6 of summer", "Near Kesh", "Act II: The Circle", "Saved 2 minutes ago"]):
        text(img, s, W // 2 + 132, 162 + i * 11, CREAM if i < 3 else PARCH_D)
    text(img, "1.0.0", W - 40, H - 12, PARCH_DD, None)
    return img


def before_after(hud_frame: Path, after: Image.Image) -> Image.Image:
    before = Image.open(hud_frame).convert("RGB").resize((W * 3, H * 3), Image.NEAREST)
    a = after.convert("RGB").resize((W * 3, H * 3), Image.NEAREST)
    out = Image.new("RGB", (W * 3, H * 3 * 2 + 60), INK)
    out.paste(before, (0, 0))
    out.paste(a, (0, H * 3 + 60))
    lab = Image.new("RGBA", (W, 20), INK + (255,))
    text(lab, "Now: every module adds its line to one list", 6, 6, SAFFRON)
    text(lab, "Proposed: each thing in its place, framed, quiet when nothing changes", 330, 6, TURQ)
    out.paste(lab.resize((W * 3, 60), Image.NEAREST), (0, H * 3))
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=Path, required=True)
    ap.add_argument("--hud-frame", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    hud = flight_hud(base(a.frames, 250))
    save(hud, a.out, "01-flight-hud.png")
    save(flight_hud(base(a.frames, 250), calm=True), a.out, "02-flight-calm.png")
    save(dialogue(base(a.frames, 360)), a.out, "03-dialogue.png")
    save(chapter(base(a.frames, 690)), a.out, "04-chapter-card.png")
    save(bag_page(base(a.frames, 330, dim=0.55, blur=2)), a.out, "05-book-bag.png")
    save(map_page(base(a.frames, 330, dim=0.55, blur=2)), a.out, "06-book-map.png")
    save(title(base(a.frames, 250, dim=0.35)), a.out, "07-title.png")
    before_after(a.hud_frame, hud).save(a.out / "00-before-after.png")
    print(f"wrote {a.out / '00-before-after.png'}")


if __name__ == "__main__":
    main()

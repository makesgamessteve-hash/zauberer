#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Cuts the reel footage into videos for social media.

The footage comes from tools/reels/reel.lua, rendered in a portrait window
(1080x1920) and a landscape one (1920x1080); its log says where each shot
begins. This script adds a caption to each shot in the game's own pixel font,
an end card, and the game's music and ambiences cut to the shots, and writes:

  reel-9x16.mp4    1080x1920  TikTok, Instagram Reels, Facebook Reels, YouTube Shorts
  feed-4x5.mp4     1080x1350  Instagram and Facebook feed posts (cut from the portrait footage)
  trailer-16x9.mp4 1920x1080  YouTube, Facebook video, X
  cover-9x16.png, cover-4x5.png, thumbnail-16x9.png (YouTube thumbnail, 1280x720)

    python3 tools/reels/make_reels.py --portrait DIR --portrait-log LOG \\
        --landscape DIR --landscape-log LOG --out OUTDIR
"""
from __future__ import annotations

import argparse
import re
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets" / "base"
FPS = 30
URL = "makesgamessteve-hash.github.io/zauberer"

# What each shot says (shown from a moment after the shot begins).
CAPTIONS = {
    "globe": "Grow a whole planet",
    "carpet": "...then fly it on a magic carpet",
    "coast": "Coasts and continents, grown not drawn",
    "village": "Villages with their own people",
    "creatures": "Wolves hunt. Deer flee.",
    "fire": "Fire spreads with the wind",
    "night": "...and burns on under the stars",
    "storm": "Storms roll in",
    "home": "Your carpet folds into a home",
}
# The music and the ambience under each shot (assets/base/music, ambience).
MUSIC = {"globe": "title", "carpet": "title", "coast": "loop", "village": "loop", "creatures": "danger",
         "fire": "danger", "night": "night", "storm": "danger", "home": "story", "end": "title"}
AMBIENCE = {"coast": "wind", "village": "village", "night": "night", "storm": "rain", "carpet": "wind"}
END_FRAMES = 105  # the end card: 3.5 seconds

# Where captions go in each format: (centre y of the caption, font scale, widest line in pixels).
# The portrait reel keeps clear of the platforms' buttons and text (top 250, bottom 450, right 140).
LAYOUT = {
    "reel": {"size": (1080, 1920), "caption_y": 360, "scale": 7, "width": 860},
    "feed": {"size": (1080, 1350), "caption_y": 150, "scale": 6, "width": 940, "crop_y": 285},
    "trailer": {"size": (1920, 1080), "caption_y": 900, "scale": 7, "width": 1600},
}


class PixelFont:
    """The game's 8x8 pixel font (assets/base/fonts/pixel.lua and .png), drawn at an integer scale."""

    def __init__(self) -> None:
        spec = (ASSETS / "fonts" / "pixel.lua").read_text(encoding="utf-8")
        chars = re.search(r'chars = "((?:[^"\\]|\\.)*)"', spec).group(1)
        self.chars = re.sub(r"\\(.)", r"\1", chars)
        self.gw = int(re.search(r"glyph_width = (\d+)", spec).group(1))
        self.gh = int(re.search(r"glyph_height = (\d+)", spec).group(1))
        self.cols = int(re.search(r"columns = (\d+)", spec).group(1))
        self.adv = int(re.search(r"advance = (\d+)", spec).group(1))
        self.sheet = Image.open(ASSETS / "fonts" / "pixel.png").convert("RGBA")

    def width(self, text: str, scale: int) -> int:
        return len(text) * self.adv * scale

    def mask(self, text: str) -> Image.Image:
        """The text as an alpha mask at scale 1."""
        out = Image.new("L", (max(1, len(text) * self.adv), self.gh), 0)
        for i, ch in enumerate(text):
            k = self.chars.find(ch)
            if k < 0:
                k = self.chars.find("?")
            x, y = (k % self.cols) * self.gw, (k // self.cols) * self.gh
            glyph = self.sheet.crop((x, y, x + self.gw, y + self.gh)).getchannel("A")
            out.paste(glyph, (i * self.adv, 0), glyph)
        return out

    def wrap(self, text: str, scale: int, width: int) -> list[str]:
        lines, line = [], ""
        for word in text.split():
            if line and self.width(line + " " + word, scale) > width:
                lines.append(line)
                line = word
            else:
                line = (line + " " + word).strip()
        return lines + [line]

    def draw(self, img: Image.Image, text: str, cx: int, cy: int, scale: int, width: int,
             colour=(255, 255, 255), alpha: float = 1.0) -> None:
        """Draws text centred on (cx, cy) with a dark outline, wrapped to width."""
        lines = self.wrap(text, scale, width)
        line_h = (self.gh + 3) * scale
        top = cy - len(lines) * line_h // 2
        for n, line in enumerate(lines):
            m = self.mask(line)
            m = m.resize((m.width * scale, m.height * scale), Image.NEAREST)
            x = cx - m.width // 2
            y = top + n * line_h
            a = int(255 * alpha)
            shade = Image.new("RGBA", m.size, (12, 8, 24, a))
            for dx in (-scale, 0, scale):
                for dy in (-scale, 0, scale, 2 * scale):
                    if dx or dy:
                        img.paste(shade, (x + dx, y + dy), m)
            img.paste(Image.new("RGBA", m.size, colour + (a,)), (x, y), m)


def read_shots(log: Path) -> list[tuple[str, int]]:
    shots = []
    for line in log.read_text(errors="replace").splitlines():
        m = re.search(r"reel shot (\w+) from (\d+)", line)
        if m:
            shots.append((m.group(1), int(m.group(2))))
    total = re.findall(r"reel frames (\d+)", log.read_text(errors="replace"))
    if not shots or not total:
        raise SystemExit(f"{log}: no shots logged (did the reel scenario finish?)")
    return shots + [("end", int(total[-1]) + 1)]


def shot_at(shots, f: int) -> tuple[str, int, int]:
    """The shot frame f (1-based) is in: (name, first frame, frames in it)."""
    for (name, first), (_, nxt) in zip(shots, shots[1:]):
        if first <= f < nxt:
            return name, first, nxt - first
    return shots[-1][0], shots[-1][1], END_FRAMES


def fade(i: int, n: int, inn: int = 6, out: int = 6) -> float:
    return max(0.0, min(1.0, i / inn, (n - i) / out))


def caption_frame(font: PixelFont, img: Image.Image, fmt: str, name: str, i: int, n: int) -> None:
    text = CAPTIONS.get(name)
    if not text or i < 4:
        return
    lay = LAYOUT[fmt]
    font.draw(img, text, img.width // 2, lay["caption_y"], lay["scale"], lay["width"],
              alpha=fade(i - 4, n - 4, 6, 5))


def end_card(font: PixelFont, base: Image.Image, fmt: str, i: int) -> Image.Image:
    """The last frame darkened, with the title, what it is and where to play it."""
    t = min(1.0, i / 15)
    img = ImageEnhance.Brightness(base).enhance(1 - 0.7 * t).convert("RGBA")
    w, h = img.size
    portrait = h > w
    big = 18 if portrait else 20
    mid = 6 if portrait else 6
    small = 4 if portrait else 4
    cy = h * (0.40 if portrait else 0.38)
    a1, a2, a3 = (max(0.0, min(1.0, (i - d) / 10)) for d in (4, 14, 24))
    font.draw(img, "ZAUBERER", w // 2, int(cy), big, w - 80, colour=(255, 214, 102), alpha=a1)
    line = (font.gh + 3) * mid
    font.draw(img, "Grow a world.", w // 2, int(cy + 34 * big / 2 + 40 - line // 2), mid, w - 160, alpha=a2)
    font.draw(img, "Fly a magic carpet.", w // 2, int(cy + 34 * big / 2 + 40 + line // 2), mid, w - 160, alpha=a2)
    font.draw(img, "Free in your browser", w // 2, int(cy + 34 * big / 2 + 40 + (180 if portrait else 110)),
              mid, w - 160, colour=(140, 220, 255), alpha=a3)
    font.draw(img, URL, w // 2, int(cy + 34 * big / 2 + 40 + (290 if portrait else 190)), small, w - 80,
              colour=(220, 220, 220), alpha=a3)
    return img


def frame_image(frames: Path, f: int, fmt: str) -> Image.Image:
    img = Image.open(frames / f"reel_{f:05d}.png").convert("RGBA")
    lay = LAYOUT[fmt]
    if img.size != lay["size"]:
        if "crop_y" in lay:
            y = lay["crop_y"]
            img = img.crop((0, y, lay["size"][0], y + lay["size"][1]))
        else:
            img = img.resize(lay["size"], Image.NEAREST)
    return img


def soundtrack(shots, tmp: Path) -> Path:
    """The music and ambience cut to the shots, as one WAV."""
    tmp.mkdir(parents=True, exist_ok=True)
    pieces = [(name, (nxt - first) / FPS) for (name, first), (_, nxt) in zip(shots, shots[1:])]
    pieces.append(("end", END_FRAMES / FPS))
    # Runs of the same music play on without a cut.
    runs: list[list] = []
    for name, secs in pieces:
        track = MUSIC.get(name, "loop")
        if runs and runs[-1][0] == track:
            runs[-1][1] += secs
        else:
            runs.append([track, secs])
    parts = []
    for k, (track, secs) in enumerate(runs):
        out = tmp / f"m{k:02d}.wav"
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-stream_loop", "-1", "-i", str(ASSETS / "music" / f"{track}.ogg"),
                        "-t", f"{secs:.3f}", "-af", f"afade=t=in:d=0.15,afade=t=out:st={max(0, secs - 0.3):.3f}:d=0.3",
                        "-ar", "48000", "-ac", "2", str(out)], check=True)
        parts.append(out)
    amb = []
    for k, (name, secs) in enumerate(pieces):
        out = tmp / f"a{k:02d}.wav"
        src = AMBIENCE.get(name)
        if src:
            cmd = ["-stream_loop", "-1", "-i", str(ASSETS / "ambience" / f"{src}.ogg")]
            af = f"volume=0.8,afade=t=in:d=0.3,afade=t=out:st={max(0, secs - 0.3):.3f}:d=0.3"
        else:
            cmd = ["-f", "lavfi", "-i", "anullsrc=r=48000:cl=stereo"]
            af = "anull"
        subprocess.run(["ffmpeg", "-v", "error", "-y", *cmd, "-t", f"{secs:.3f}", "-af", af, "-ar", "48000", "-ac", "2",
                        str(out)], check=True)
        amb.append(out)

    def concat(files, out):
        lst = out.with_suffix(".txt")
        lst.write_text("".join(f"file '{f}'\n" for f in files))
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", str(lst), "-c", "copy",
                        str(out)], check=True)
        return out

    music, ambience = concat(parts, tmp / "music.wav"), concat(amb, tmp / "ambience.wav")
    total = sum(s for _, s in pieces)
    out = tmp / "soundtrack.wav"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(music), "-i", str(ambience), "-filter_complex",
                    f"[0]volume=1.0[m];[1]volume=0.6[a];[m][a]amix=inputs=2:normalize=0,"
                    f"afade=t=out:st={total - 1.2:.3f}:d=1.2,alimiter=limit=0.9[o]",
                    "-map", "[o]", "-ar", "48000", str(out)], check=True)
    return out


def make_video(font: PixelFont, frames: Path, shots, fmt: str, audio: Path, out: Path) -> None:
    w, h = LAYOUT[fmt]["size"]
    last = shots[-1][1] - 1
    enc = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{w}x{h}", "-r", str(FPS),
         "-i", "-", "-i", str(audio), "-c:v", "libx264", "-preset", "slow", "-crf", "18", "-maxrate", "12M", "-bufsize", "24M", "-pix_fmt", "yuv420p",
         "-profile:v", "high", "-level", "4.2", "-c:a", "aac", "-b:a", "192k", "-ar", "48000",
         "-movflags", "+faststart", "-shortest", str(out)], stdin=subprocess.PIPE)
    prev = None
    for f in range(1, last + 1):
        path = frames / f"reel_{f:05d}.png"
        img = frame_image(frames, f, fmt) if path.exists() else prev.copy()
        prev = img.copy()
        name, first, n = shot_at(shots, f)
        caption_frame(font, img, fmt, name, f - first, n)
        enc.stdin.write(img.convert("RGB").tobytes())
    for i in range(END_FRAMES):
        enc.stdin.write(end_card(font, prev, fmt, i).convert("RGB").tobytes())
    enc.stdin.close()
    if enc.wait() != 0:
        raise SystemExit(f"ffmpeg failed for {out}")
    print(f"wrote {out}")


def cover(font: PixelFont, frames: Path, shots, fmt: str, shot: str, out: Path, size=None) -> None:
    """A still for the cover: the middle of a shot with the title over it."""
    first = dict(shots)[shot]
    nxt = next(f for (n, f) in shots if f > first)
    img = frame_image(frames, (first + nxt) // 2, fmt)
    w, h = img.size
    portrait = h > w
    shade = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(shade)
    for y in range(h):
        k = max(0.0, 1 - abs(y - LAYOUT[fmt]["caption_y"] * (1.6 if fmt == "trailer" else 1)) / (h * 0.3))
        d.line([(0, y), (w, y)], fill=(8, 4, 20, int(150 * k)))
    img = Image.alpha_composite(img, shade)
    cy = LAYOUT[fmt]["caption_y"] + (40 if portrait else 0)
    if fmt == "trailer":
        cy = 250
    font.draw(img, "ZAUBERER", w // 2, cy, 16 if portrait else 22, w - 60, colour=(255, 214, 102))
    small = 6 if portrait else 7
    below = cy + (180 if portrait else 220)
    font.draw(img, "Grow a world.", w // 2, below, small, w - 100)
    font.draw(img, "Fly a magic carpet.", w // 2, below + (font.gh + 3) * small, small, w - 100)
    img = img.convert("RGB")
    if size:
        img = img.resize(size, Image.LANCZOS)
    img.save(out)
    print(f"wrote {out}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--portrait", type=Path, required=True)
    ap.add_argument("--portrait-log", type=Path, required=True)
    ap.add_argument("--landscape", type=Path, required=True)
    ap.add_argument("--landscape-log", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--only", choices=["reel", "feed", "trailer"], action="append")
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    font = PixelFont()
    tall, wide = read_shots(args.portrait_log), read_shots(args.landscape_log)
    want = args.only or ["reel", "feed", "trailer"]
    with tempfile.TemporaryDirectory() as tmp:
        if "reel" in want or "feed" in want:
            audio = soundtrack(tall, Path(tmp) / "tall")
        for fmt, ratio in (("reel", "9x16"), ("feed", "4x5")):
            if fmt in want:
                make_video(font, args.portrait, tall, fmt, audio, args.out / f"{fmt}-{ratio}.mp4")
        if "trailer" in want:
            audio = soundtrack(wide, Path(tmp) / "wide")
            make_video(font, args.landscape, wide, "trailer", audio, args.out / "trailer-16x9.mp4")
    cover(font, args.portrait, tall, "reel", "coast", args.out / "cover-9x16.png")
    cover(font, args.portrait, tall, "feed", "coast", args.out / "cover-4x5.png")
    cover(font, args.landscape, wide, "trailer", "coast", args.out / "thumbnail-16x9.png", size=(1280, 720))


if __name__ == "__main__":
    main()

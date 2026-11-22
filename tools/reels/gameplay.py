#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Gameplay videos from scripted playthroughs.

A scenario from tests/scenarios is run with tools/reels/capture_header.lua in
front of it, in a 1920x1080 window, so every other tick it steps is filmed
(with the game's HUD) as play_00001.png ... This script cuts such footage into

  <name>-16x9.mp4   1920x1080 for YouTube and Facebook
  <name>-9x16.mp4   1080x1920 for Reels, TikTok and Shorts (the footage framed
                    over a blurred copy of itself, a title above it)

each with a title, the game's music and the end card, sped up if it runs
longer than --max-seconds.

    python3 tools/reels/gameplay.py --frames DIR --name slice-a \\
        --title "Magic in the wild" --subtitle "Fire, ice and a storm bird" \\
        --music danger --out OUTDIR
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_reels as mr  # noqa: E402

FPS = 30


def music_bed(tracks: list[str], seconds: float, out: Path) -> Path:
    """The tracks one after another (each looped to an even share), faded out at the end."""
    share = seconds / len(tracks)
    parts = []
    for k, track in enumerate(tracks):
        p = out.parent / f"bed{k}.wav"
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-stream_loop", "-1", "-i",
                        str(mr.ASSETS / "music" / f"{track}.ogg"), "-t", f"{share:.3f}", "-af",
                        f"afade=t=in:d=0.3,afade=t=out:st={max(0, share - 0.4):.3f}:d=0.4", "-ar", "48000",
                        "-ac", "2", str(p)], check=True)
        parts.append(p)
    lst = out.parent / "bed.txt"
    lst.write_text("".join(f"file '{p}'\n" for p in parts))
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", str(lst), "-af",
                    f"afade=t=out:st={seconds - 1.5:.3f}:d=1.5,volume=0.9", str(out)], check=True)
    return out


def vertical(frame: Image.Image) -> Image.Image:
    """A landscape frame on a 1080x1920 canvas: blurred and enlarged behind, whole in the middle."""
    bg = frame.resize((3413, 1920), Image.BILINEAR).crop((1166, 0, 2246, 1920))
    bg = bg.resize((270, 480), Image.BILINEAR).filter(ImageFilter.GaussianBlur(6)).resize((1080, 1920))
    bg = Image.blend(bg, Image.new("RGB", bg.size, (6, 4, 16)), 0.45)
    fg = frame.resize((1080, 608), Image.LANCZOS)
    bg.paste(fg, (0, 656))
    return bg.convert("RGBA")


def encode(frames: list[Path], fmt: str, args, font: mr.PixelFont, audio: Path, out: Path) -> None:
    w, h = (1920, 1080) if fmt == "wide" else (1080, 1920)
    enc = subprocess.Popen(
        ["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{w}x{h}", "-r", str(FPS),
         "-i", "-", "-i", str(audio), "-c:v", "libx264", "-preset", "slow", "-crf", "19", "-maxrate", "12M", "-bufsize", "24M", "-pix_fmt", "yuv420p",
         "-profile:v", "high", "-level", "4.2", "-c:a", "aac", "-b:a", "192k", "-ar", "48000",
         "-movflags", "+faststart", "-shortest", str(out)], stdin=subprocess.PIPE)
    n = len(frames)
    last = None
    for i, path in enumerate(frames):
        src = Image.open(path).convert("RGB")
        img = (src.resize((w, h), Image.NEAREST) if src.size != (w, h) else src).convert("RGBA") \
            if fmt == "wide" else vertical(src)
        last = img
        if fmt == "wide":
            a = mr.fade(i, min(n, 150), 8, 12)
            if i < 150:
                font.draw(img, args.title, w // 2, 110, 9, w - 200, colour=(255, 214, 102), alpha=a)
                font.draw(img, args.subtitle, w // 2, 210, 5, w - 300, alpha=a)
        else:
            font.draw(img, args.title, w // 2, 380, 8, 940, colour=(255, 214, 102), alpha=mr.fade(i, n, 8, 8))
            font.draw(img, args.subtitle, w // 2, 520, 5, 940, alpha=mr.fade(i, n, 8, 8))
            font.draw(img, "Zauberer: free in your browser", w // 2, 1360, 5, 940, colour=(140, 220, 255),
                      alpha=mr.fade(i, n, 8, 8))
        enc.stdin.write(img.convert("RGB").tobytes())
    for i in range(mr.END_FRAMES):
        enc.stdin.write(mr.end_card(font, last.convert("RGB"), "reel" if fmt == "tall" else "trailer", i)
                        .convert("RGB").tobytes())
    enc.stdin.close()
    if enc.wait() != 0:
        raise SystemExit(f"ffmpeg failed for {out}")
    print(f"wrote {out} ({(n + mr.END_FRAMES) / FPS:.0f} s)")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=Path, required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--title", required=True)
    ap.add_argument("--subtitle", default="")
    ap.add_argument("--music", default="loop", help="music tracks in turn, comma-separated (assets/base/music)")
    ap.add_argument("--max-seconds", type=float, default=75)
    ap.add_argument("--skip", type=int, default=0, help="frames to leave out at the start")
    ap.add_argument("--formats", default="wide,tall")
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    frames = sorted(args.frames.glob("play_*.png"))[args.skip:]
    if not frames:
        raise SystemExit(f"no frames in {args.frames}")
    most = int(args.max_seconds * FPS)
    if len(frames) > most:  # (sped up evenly)
        frames = [frames[int(k * len(frames) / most)] for k in range(most)]
    args.out.mkdir(parents=True, exist_ok=True)
    font = mr.PixelFont()
    seconds = (len(frames) + mr.END_FRAMES) / FPS
    with tempfile.TemporaryDirectory() as tmp:
        audio = music_bed(args.music.split(","), seconds, Path(tmp) / "bed.wav")
        for fmt in args.formats.split(","):
            encode(frames, fmt, args, font, audio, args.out / f"{args.name}-{'16x9' if fmt == 'wide' else '9x16'}.mp4")


if __name__ == "__main__":
    main()

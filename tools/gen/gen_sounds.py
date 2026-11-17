# SPDX-License-Identifier: MIT
"""Synthesises the demo sounds: sounds/chime.wav (WAV) and music/loop.ogg
(encoded with oggenc using a fixed stream serial, so output is reproducible)."""
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

RATE = 48000


def write_wav(path, samples, rate=RATE):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples))


def chime():
    n = int(RATE * 0.6)
    out = []
    for i in range(n):
        t = i / RATE
        env = math.exp(-t * 6.0) * min(1.0, t * 200.0)
        s = 0.5 * math.sin(2 * math.pi * 880 * t) + 0.25 * math.sin(2 * math.pi * 1320 * t)
        out.append(s * env * 0.8)
    return out


def loop():
    """Eight seconds of a gentle arpeggio in D (an ornamented scale)."""
    notes = [293.66, 311.13, 369.99, 392.00, 440.00, 466.16, 554.37, 587.33]
    pattern = [0, 2, 4, 7, 4, 2, 1, 3, 5, 3, 2, 0, 4, 6, 5, 4]
    step = int(RATE * 0.5)
    out = [0.0] * (step * len(pattern))
    for k, p in enumerate(pattern):
        f = notes[p]
        for i in range(step):
            t = i / RATE
            env = math.exp(-t * 3.0) * min(1.0, t * 100.0)
            s = math.sin(2 * math.pi * f * t) + 0.3 * math.sin(2 * math.pi * 2 * f * t)
            out[k * step + i] += 0.35 * s * env
    # drone
    for i in range(len(out)):
        t = i / RATE
        out[i] += 0.08 * math.sin(2 * math.pi * 146.83 * t)
    return out


def main(base):
    sounds = os.path.join(base, "sounds")
    music = os.path.join(base, "music")
    os.makedirs(sounds, exist_ok=True)
    os.makedirs(music, exist_ok=True)
    write_wav(os.path.join(sounds, "chime.wav"), chime())
    oggenc = shutil.which("oggenc")
    if not oggenc:
        print("oggenc not found; skipping music/loop.ogg", file=sys.stderr)
        return
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, "loop.wav")
        write_wav(wav, loop())
        subprocess.run([oggenc, "-Q", "-q", "1", "--serial", "1234", "--discard-comments",
                        "-o", os.path.join(music, "loop.ogg"), wav], check=True)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base")

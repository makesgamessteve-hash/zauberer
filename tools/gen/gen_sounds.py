# SPDX-License-Identifier: MIT
"""Synthesises the game's sounds: the effects (sounds/*.wav), the music for
each situation and the ambiences (music/*.ogg, ambience/*.ogg, encoded with
oggenc using a fixed stream serial, so output is reproducible). Noise comes
from a seeded generator."""
import math
import os
import random
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


def env_ad(t, attack, decay):
    return min(1.0, t / attack) * math.exp(-t * decay) if attack > 0 else math.exp(-t * decay)


def tone_sweep(f0, f1, length, decay, harmonics=(1.0, 0.3)):
    n = int(RATE * length)
    out, phase = [], 0.0
    for i in range(n):
        t = i / RATE
        f = f0 + (f1 - f0) * (t / length)
        phase += 2 * math.pi * f / RATE
        s = sum(a * math.sin(phase * (k + 1)) for k, a in enumerate(harmonics))
        out.append(0.6 * s * env_ad(t, 0.005, decay))
    return out


def noise_burst(seed, length, decay, smooth=0.0):
    rng = random.Random(seed)
    n = int(RATE * length)
    out, last = [], 0.0
    for i in range(n):
        t = i / RATE
        v = rng.uniform(-1, 1)
        last = last * smooth + v * (1 - smooth)
        out.append(0.7 * last * env_ad(t, 0.002, decay))
    return out


def mix(*parts):
    n = max(len(p) for p in parts)
    return [sum(p[i] for p in parts if i < len(p)) for i in range(n)]


# The effects: what the heir's actions and the story sound like.
EFFECTS = {
    "cast": lambda: tone_sweep(440, 1320, 0.45, 5, (1.0, 0.4, 0.2)),
    "misfire": lambda: mix(noise_burst(11, 0.35, 9, 0.3), tone_sweep(300, 120, 0.35, 8)),
    "hit": lambda: mix(noise_burst(12, 0.25, 18, 0.85), tone_sweep(160, 60, 0.25, 14)),
    "pickup": lambda: mix(tone_sweep(660, 660, 0.12, 20), [0.0] * int(RATE * 0.07) + tone_sweep(990, 990, 0.15, 18)),
    "defeat": lambda: tone_sweep(392, 98, 1.2, 2.5, (1.0, 0.5, 0.25)),
    "beat": lambda: tone_sweep(196, 196, 2.0, 1.4, (1.0, 0.6, 0.3, 0.2)),
    # The UI's own sounds (soft, so they sit under the world).
    "ui_page": lambda: mix(noise_burst(31, 0.22, 16, 0.25), noise_burst(32, 0.12, 30, 0.15)),
    "ui_cloth": lambda: noise_burst(33, 0.16, 22, 0.12),
    "ui_goal": lambda: mix(tone_sweep(587, 587, 0.5, 6, (1.0, 0.3)),
                           [0.0] * int(RATE * 0.12) + tone_sweep(880, 880, 0.6, 5, (1.0, 0.3))),
}


def arpeggio(notes, pattern, step_s, decay, drone, gain=0.35):
    step = int(RATE * step_s)
    out = [0.0] * (step * len(pattern))
    for k, p in enumerate(pattern):
        f = notes[p]
        for i in range(step):
            t = i / RATE
            s = math.sin(2 * math.pi * f * t) + 0.3 * math.sin(2 * math.pi * 2 * f * t)
            out[k * step + i] += gain * s * env_ad(t, 0.01, decay)
    for i in range(len(out)):
        out[i] += 0.08 * math.sin(2 * math.pi * drone * i / RATE)
    return out


HIJAZ = [293.66, 311.13, 369.99, 392.00, 440.00, 466.16, 554.37, 587.33]  # D Hijaz
NAHAWAND = [293.66, 329.63, 349.23, 392.00, 440.00, 466.16, 554.37, 587.33]  # D Nahawand

# The music for each situation (eight to twelve seconds, looping).
MUSIC = {
    "title": lambda: arpeggio(NAHAWAND, [0, 4, 7, 4, 2, 4, 5, 4, 0, 3, 5, 3, 1, 2, 4, 2], 0.6, 2.0, 146.83),
    "night": lambda: arpeggio(NAHAWAND, [0, 2, 4, 2, 1, 3, 2, 0], 1.0, 1.2, 110.0, 0.25),
    "danger": lambda: arpeggio(HIJAZ, [0, 1, 0, 1, 2, 1, 4, 3, 0, 1, 0, 1, 5, 4, 2, 1] * 2, 0.18, 9.0, 73.42, 0.4),
    "story": lambda: arpeggio(HIJAZ, [0, 4, 7, 6, 4, 2, 1, 0], 0.9, 1.6, 146.83, 0.3),
}


def noise_loop(seed, seconds, smooth, gain, swell=0.0, swell_hz=0.1):
    rng = random.Random(seed)
    n, out, last = int(RATE * seconds), [], 0.0
    for i in range(n):
        t = i / RATE
        last = last * smooth + rng.uniform(-1, 1) * (1 - smooth)
        out.append(gain * last * (1 - swell + swell * math.sin(2 * math.pi * swell_hz * t)))
    return out


def chirps(seed, seconds, freq, rate_hz, gain):
    rng = random.Random(seed)
    out = [0.0] * int(RATE * seconds)
    t = 0.0
    while t < seconds - 0.2:
        start = int(t * RATE)
        f = freq * rng.uniform(0.9, 1.1)
        for i in range(int(RATE * 0.06)):
            if start + i < len(out):
                tt = i / RATE
                out[start + i] += gain * math.sin(2 * math.pi * f * tt) * env_ad(tt, 0.005, 40)
        t += rng.uniform(0.5, 1.5) / rate_hz
    return out


# The ambiences of the places (eight seconds, looping).
AMBIENCE = {
    "wind": lambda: noise_loop(21, 8, 0.985, 2.5, 0.5, 0.125),
    "village": lambda: mix(noise_loop(22, 8, 0.95, 0.5, 0.3, 0.25), chirps(23, 8, 2400, 1.5, 0.12)),
    "cave": lambda: mix(noise_loop(24, 8, 0.995, 2.0), chirps(25, 8, 1800, 0.6, 0.2)),
    "rain": lambda: noise_loop(26, 8, 0.4, 0.35),
    "night": lambda: chirps(27, 8, 4200, 6, 0.08),
}


def write_ogg(path, samples):
    oggenc = shutil.which("oggenc")
    if not oggenc:
        print("oggenc not found; skipping " + path, file=sys.stderr)
        return
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, "x.wav")
        write_wav(wav, samples)
        subprocess.run([oggenc, "-Q", "-q", "1", "--serial", "1234", "--discard-comments", "-o", path, wav], check=True)


def main(base):
    sounds = os.path.join(base, "sounds")
    music = os.path.join(base, "music")
    os.makedirs(sounds, exist_ok=True)
    os.makedirs(music, exist_ok=True)
    ambience = os.path.join(base, "ambience")
    os.makedirs(ambience, exist_ok=True)
    write_wav(os.path.join(sounds, "chime.wav"), chime())
    for name, make in EFFECTS.items():
        write_wav(os.path.join(sounds, name + ".wav"), make())
    write_ogg(os.path.join(music, "loop.ogg"), loop())  # (the calm of flight)
    for name, make in MUSIC.items():
        write_ogg(os.path.join(music, name + ".ogg"), make())
    for name, make in AMBIENCE.items():
        write_ogg(os.path.join(ambience, name + ".ogg"), make())


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base")

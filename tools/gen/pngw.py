# SPDX-License-Identifier: MIT
"""Minimal deterministic PNG writer (RGBA8, standard library only)."""
import struct
import zlib


def write_png(path, width, height, pixels):
    """pixels: list of rows, each a list of (r, g, b, a) tuples."""
    raw = bytearray()
    for row in pixels:
        raw.append(0)  # filter: none
        for r, g, b, a in row:
            raw += bytes((r, g, b, a))
    return _write(path, width, height, bytes(raw))


def png_bytes(width, height, pixels):
    raw = bytearray()
    for row in pixels:
        raw.append(0)
        for r, g, b, a in row:
            raw += bytes((r, g, b, a))
    return _encode(width, height, bytes(raw))


def _chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def _encode(width, height, raw):
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header) + _chunk(b"IDAT", zlib.compress(raw, 9))
            + _chunk(b"IEND", b""))


def _write(path, width, height, raw):
    data = _encode(width, height, raw)
    with open(path, "wb") as f:
        f.write(data)
    return data

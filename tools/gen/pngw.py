# SPDX-License-Identifier: MIT
"""Minimal deterministic PNG writer (RGBA8, standard library only), plus a
reader for the 8-bit RGBA files it writes (used by generators and tests)."""
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


def read_png(path):
    """Reads an 8-bit RGBA, non-interlaced PNG (any filter types).
    Returns (width, height, rows), each row a list of (r, g, b, a) tuples."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("%s: not a PNG file" % path)
    pos, idat, header = 8, bytearray(), None
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
        pos += 12 + length
    if header is None:
        raise ValueError("%s: no IHDR chunk" % path)
    width, height, depth, color_type, _, _, interlace = header
    if depth != 8 or color_type != 6 or interlace != 0:
        raise ValueError("%s: only 8-bit non-interlaced RGBA is supported" % path)
    raw = zlib.decompress(bytes(idat))
    stride = width * 4
    rows, prev = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            left = line[i - 4] if i >= 4 else 0
            up = prev[i]
            if kind == 1:
                line[i] = (line[i] + left) & 0xFF
            elif kind == 2:
                line[i] = (line[i] + up) & 0xFF
            elif kind == 3:
                line[i] = (line[i] + ((left + up) >> 1)) & 0xFF
            elif kind == 4:
                corner = prev[i - 4] if i >= 4 else 0
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                pred = left if pa <= pb and pa <= pc else (up if pb <= pc else corner)
                line[i] = (line[i] + pred) & 0xFF
            elif kind != 0:
                raise ValueError("%s: bad filter type %d" % (path, kind))
        rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(width)])
        prev = line
    return width, height, rows

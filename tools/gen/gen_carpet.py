# SPDX-License-Identifier: MIT
"""Generates the low-poly flying carpet (models/carpet.glb) with an embedded
pattern texture. Deterministic: the same script always writes the same bytes."""
import json
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pngw import png_bytes  # noqa: E402

NX, NZ = 12, 6
SIZE_X, SIZE_Z = 2.0, 1.2
WAVE = 0.06

RED = (150, 30, 40, 255)
DEEP = (90, 15, 35, 255)
GOLD = (220, 170, 60, 255)
INDIGO = (40, 50, 120, 255)
CREAM = (235, 220, 190, 255)


def pattern():
    """64x32 carpet pattern: border, field and a central medallion."""
    w, h = 64, 32
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            bx, by = min(x, w - 1 - x), min(y, h - 1 - y)
            border = min(bx, by)
            if border < 2:
                c = DEEP
            elif border < 4:
                c = GOLD if (x + y) % 4 < 2 else DEEP
            elif border < 5:
                c = CREAM
            else:
                dx, dy = abs(x - w / 2 + 0.5) / 2.0, abs(y - h / 2 + 0.5)
                d = dx + dy
                if d < 4:
                    c = GOLD
                elif d < 6:
                    c = INDIGO
                elif d < 7:
                    c = CREAM
                elif (x // 4 + y // 4) % 2 == 0 and d > 9:
                    c = INDIGO
                else:
                    c = RED
            row.append(c)
        rows.append(row)
    return png_bytes(w, h, rows)


def build():
    positions, normals, uvs, indices = [], [], [], []
    for side in (0, 1):  # 0 = top, 1 = bottom
        base = len(positions)
        for iz in range(NZ + 1):
            for ix in range(NX + 1):
                u, v = ix / NX, iz / NZ
                x = (u - 0.5) * SIZE_X
                z = (v - 0.5) * SIZE_Z
                y = WAVE * math.sin(u * math.pi * 2.0) * math.cos(v * math.pi)
                positions.append((x, y - 0.01 * side, z))
                normals.append((0.0, -1.0 if side else 1.0, 0.0))
                uvs.append((u, v))
        for iz in range(NZ):
            for ix in range(NX):
                a = base + iz * (NX + 1) + ix
                b, c, d = a + 1, a + NX + 1, a + NX + 2
                if side == 0:
                    indices += [a, c, b, b, c, d]
                else:
                    indices += [a, b, c, b, d, c]
    return positions, normals, uvs, indices


def main(out_path):
    positions, normals, uvs, indices = build()
    image = pattern()
    pos_bytes = b"".join(struct.pack("<3f", *p) for p in positions)
    nrm_bytes = b"".join(struct.pack("<3f", *n) for n in normals)
    uv_bytes = b"".join(struct.pack("<2f", *t) for t in uvs)
    idx_bytes = b"".join(struct.pack("<H", i) for i in indices)

    def pad(b):
        return b + b"\x00" * ((4 - len(b) % 4) % 4)

    chunks = [pad(pos_bytes), pad(nrm_bytes), pad(uv_bytes), pad(idx_bytes), pad(image)]
    offsets, total = [], 0
    for c in chunks:
        offsets.append(total)
        total += len(c)
    binary = b"".join(chunks)
    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    gltf = {
        "asset": {"version": "2.0", "generator": "zauberer tools/gen/gen_carpet.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": "carpet"}],
        "meshes": [{"name": "carpet", "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3, "material": 0}]}],
        "materials": [{"name": "weave", "pbrMetallicRoughness": {
            "baseColorFactor": [1, 1, 1, 1], "baseColorTexture": {"index": 0},
            "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728}],
        "images": [{"bufferView": 4, "mimeType": "image/png"}],
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": offsets[0], "byteLength": len(pos_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[1], "byteLength": len(nrm_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[2], "byteLength": len(uv_bytes), "target": 34962},
            {"buffer": 0, "byteOffset": offsets[3], "byteLength": len(idx_bytes), "target": 34963},
            {"buffer": 0, "byteOffset": offsets[4], "byteLength": len(image)},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
    }
    js = json.dumps(gltf, separators=(",", ":"), sort_keys=True).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(binary))
    out += struct.pack("<II", len(js), 0x4E4F534A) + js
    out += struct.pack("<II", len(binary), 0x004E4942) + binary
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(out)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base/models/carpet.glb")

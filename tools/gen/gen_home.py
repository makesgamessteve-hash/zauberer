# SPDX-License-Identifier: MIT
"""Generates the carpet folded as a tent (models/tent.glb) and a room of the
pocket home (models/room.glb), each with an embedded pattern texture.
Deterministic: the same script always writes the same bytes."""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pngw import png_bytes  # noqa: E402

CANVAS = (214, 196, 160, 255)
CANVAS_DARK = (170, 150, 115, 255)
RED = (150, 30, 40, 255)
GOLD = (220, 170, 60, 255)
INDIGO = (40, 50, 120, 255)
TURQUOISE = (40, 140, 150, 255)
CREAM = (235, 220, 190, 255)
TERRACOTTA = (170, 90, 60, 255)


def canvas():
    """32x32 canvas with a woven red-and-gold border band at the bottom."""
    rows = []
    for y in range(32):
        row = []
        for x in range(32):
            if y >= 26:
                c = GOLD if (x // 2 + y) % 3 == 0 else RED
            elif (x + y) % 8 == 0:
                c = CANVAS_DARK
            else:
                c = CANVAS
            row.append(c)
        rows.append(row)
    return png_bytes(32, 32, rows)


def tiles():
    """64x64: the top half floor tiles (star and cross), the bottom half a
    wall of plaster over a tiled dado with an arch line."""
    rows = []
    for y in range(64):
        row = []
        for x in range(64):
            if y < 32:  # floor: 16 px tiles
                tx, ty = x % 16 - 7.5, y % 16 - 7.5
                d = abs(tx) + abs(ty)
                if d < 3:
                    c = GOLD
                elif d < 6:
                    c = TURQUOISE
                elif abs(tx) > 6.5 or abs(ty) > 6.5:
                    c = CREAM
                else:
                    c = INDIGO
            else:  # wall: plaster above, a tiled dado below
                wy = y - 32
                if wy < 18:
                    c = CREAM if (x * 7 + wy * 3) % 23 else (225, 208, 178, 255)
                    if wy in (16, 17):
                        c = TERRACOTTA
                else:
                    c = TURQUOISE if ((x // 4) + (wy // 4)) % 2 == 0 else INDIGO
            row.append(c)
        rows.append(row)
    return png_bytes(64, 64, rows)


def tent_mesh():
    """A ridge tent 2.4 m square, 1.6 m high, its door facing -z."""
    w, d, h = 1.2, 1.2, 1.6
    positions, normals, uvs, indices = [], [], [], []

    def quad(a, b, c, e, n, uv):
        base = len(positions)
        positions.extend([a, b, c, e])
        normals.extend([n] * 4)
        uvs.extend(uv)
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    def tri(a, b, c, n, uv):
        base = len(positions)
        positions.extend([a, b, c])
        normals.extend([n] * 3)
        uvs.extend(uv)
        indices.extend([base, base + 1, base + 2])

    s = 0.8  # normal of the slopes
    ny = 0.6
    quad((-w, 0, -d), (-w, 0, d), (0, h, d), (0, h, -d), (-s, ny, 0), [(0, 1), (1, 1), (1, 0), (0, 0)])
    quad((w, 0, d), (w, 0, -d), (0, h, -d), (0, h, d), (s, ny, 0), [(0, 1), (1, 1), (1, 0), (0, 0)])
    tri((w, 0, -d), (-w, 0, -d), (0, h, -d), (0, 0, -1), [(1, 1), (0, 1), (0.5, 0)])
    tri((-w, 0, d), (w, 0, d), (0, h, d), (0, 0, 1), [(1, 1), (0, 1), (0.5, 0)])
    return positions, normals, uvs, indices


def room_mesh():
    """A room 8 m wide, 6 m deep and 4 m high, seen from inside: a tiled
    floor, four walls and a plaster ceiling (normals face inwards)."""
    hw, hd, h = 4.0, 3.0, 4.0
    positions, normals, uvs, indices = [], [], [], []

    def quad(a, b, c, e, n, uv):
        base = len(positions)
        positions.extend([a, b, c, e])
        normals.extend([n] * 4)
        uvs.extend(uv)
        indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    floor_uv = [(0, 0), (4, 0), (4, 1.5), (0, 1.5)]  # the floor tiles repeat (v 0 .. 0.5 of the texture)
    quad((-hw, 0, -hd), (hw, 0, -hd), (hw, 0, hd), (-hw, 0, hd), (0, 1, 0), [(u, v * 0.5 / 1.5) for u, v in floor_uv])
    wall = [(0, 1), (1, 1), (1, 0.5), (0, 0.5)]
    quad((-hw, 0, -hd), (-hw, 0, hd), (-hw, h, hd), (-hw, h, -hd), (1, 0, 0), wall)
    quad((hw, 0, hd), (hw, 0, -hd), (hw, h, -hd), (hw, h, hd), (-1, 0, 0), wall)
    quad((hw, 0, -hd), (-hw, 0, -hd), (-hw, h, -hd), (hw, h, -hd), (0, 0, 1), wall)
    quad((-hw, 0, hd), (hw, 0, hd), (hw, h, hd), (-hw, h, hd), (0, 0, -1), wall)
    ceiling = [(0, 0.5), (1, 0.5), (1, 0.75), (0, 0.75)]
    quad((-hw, h, hd), (hw, h, hd), (hw, h, -hd), (-hw, h, -hd), (0, -1, 0), ceiling)
    return positions, normals, uvs, indices


def both_sides(mesh):
    """Every triangle also wound the other way, so it shows from both sides."""
    positions, normals, uvs, indices = mesh
    n = len(positions)
    positions = positions + positions
    normals = normals + [(-x, -y, -z) for x, y, z in normals]
    uvs = uvs + uvs
    back = []
    for i in range(0, len(indices), 3):
        a, b, c = indices[i:i + 3]
        back += [a + n, c + n, b + n]
    return positions, normals, uvs, indices + back


def write_glb(out_path, name, mesh, image, generator):
    positions, normals, uvs, indices = both_sides(mesh)
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
        "asset": {"version": "2.0", "generator": generator},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
            "indices": 3, "material": 0}]}],
        "materials": [{"name": name, "doubleSided": True, "pbrMetallicRoughness": {
            "baseColorFactor": [1, 1, 1, 1], "baseColorTexture": {"index": 0},
            "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
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


def main(models_dir):
    gen = "zauberer tools/gen/gen_home.py"
    write_glb(os.path.join(models_dir, "tent.glb"), "tent", tent_mesh(), canvas(), gen)
    write_glb(os.path.join(models_dir, "room.glb"), "room", room_mesh(), tiles(), gen)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/base/models")

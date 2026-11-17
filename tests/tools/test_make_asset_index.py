# SPDX-License-Identifier: MIT
"""Tests for tools/make_asset_index.py."""
import hashlib
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase, run_main  # noqa: E402

import make_asset_index  # noqa: E402

ASSETS = {
    "assets/base/package.lua": "return { id = 'base' }\n",
    "assets/base/credits.lua": "return {}\n",
    "assets/base/scripts/main.lua": "print('hi')\n",
    "assets/base/scripts/demo/state.lua": "return {}\n",
    "assets/base/data/input.lua": "return {}\n",
    "assets/base/locale/en.lua": "return {}\n",
    "assets/base/shaders/model.vert": "void main() {}\n",
    "assets/base/fonts/pixel.lua": "return {}\n",
    "assets/base/fonts/pixel.png": b"\x89PNG font",
    "assets/base/sprites/mote.lua": "return {}\n",
    "assets/base/sprites/mote.png": b"\x89PNG mote",
    "assets/base/sprites/sheets/big.lua": "return {}\n",
    "assets/base/models/carpet.glb": b"glTF" + bytes(range(256)),
    "assets/base/.hidden": "secret\n",
    "assets/.cache/x.bin": b"cache",
    "assets/index.tsv": "# stale index\n",
}

EXPECTED_BOOT = {
    "base/credits.lua": 1, "base/data/input.lua": 1, "base/fonts/pixel.lua": 1,
    "base/fonts/pixel.png": 0, "base/locale/en.lua": 1, "base/models/carpet.glb": 0,
    "base/package.lua": 1, "base/scripts/demo/state.lua": 1, "base/scripts/main.lua": 1,
    "base/shaders/model.vert": 1, "base/sprites/mote.lua": 1, "base/sprites/mote.png": 0,
    "base/sprites/sheets/big.lua": 0,
}


class MakeAssetIndexTest(TempDirTestCase):
    def setUp(self):
        super().setUp()
        self.write(ASSETS)

    def test_index_lines(self):
        code, out = run_main(make_asset_index.main, "--root", self.root)
        self.assertEqual(code, 0, out)
        self.assertIn("wrote 13 entries (9 boot)", out)
        lines = (self.root / "assets" / "index.tsv").read_text(encoding="utf-8").splitlines()
        self.assertEqual(lines[0], "# zauberer asset index v1")
        rows = [line.split("\t") for line in lines[1:]]
        paths = [row[0] for row in rows]
        self.assertEqual(paths, sorted(EXPECTED_BOOT))
        for path, size, digest, boot in rows:
            data = (self.root / "assets" / path).read_bytes()
            self.assertEqual(int(size), len(data), path)
            self.assertEqual(digest, hashlib.sha256(data).hexdigest(), path)
            self.assertEqual(int(boot), EXPECTED_BOOT[path], path)

    def test_output_is_deterministic_and_excludes_itself(self):
        out = self.root / "assets" / "index.tsv"
        run_main(make_asset_index.main, "--root", self.root)
        first = out.read_bytes()
        code, _ = run_main(make_asset_index.main, "--assets", self.root / "assets")
        self.assertEqual(code, 0)
        self.assertEqual(out.read_bytes(), first)
        self.assertNotIn(b"index.tsv", first)
        self.assertNotIn(b"\r", first)
        self.assertTrue(first.endswith(b"\n"))

    def test_custom_output_inside_assets_is_skipped(self):
        out = self.root / "assets" / "base" / "files.tsv"
        code, _ = run_main(make_asset_index.main, "--assets", self.root / "assets", "--out", out)
        self.assertEqual(code, 0)
        code, _ = run_main(make_asset_index.main, "--assets", self.root / "assets", "--out", out)
        self.assertNotIn("files.tsv", out.read_text(encoding="utf-8"))
        self.assertFalse((self.root / "assets" / "index.tsv").read_text().startswith("# zauberer"))

    def test_boot_patterns(self):
        self.assertTrue(make_asset_index.is_boot("mod/scripts/a/b/c.lua"))
        self.assertTrue(make_asset_index.is_boot("mod/shaders/x.frag"))
        self.assertFalse(make_asset_index.is_boot("mod/sub/package.lua"))
        self.assertFalse(make_asset_index.is_boot("package.lua"))
        self.assertFalse(make_asset_index.is_boot("mod/fonts/a.png"))

    def test_missing_assets_directory_is_an_error(self):
        code, out = run_main(make_asset_index.main, "--assets", self.root / "nope")
        self.assertEqual(code, 2)
        self.assertIn("does not exist", out)


if __name__ == "__main__":
    unittest.main()

# SPDX-License-Identifier: MIT
# REUSE-IgnoreStart
"""Tests for the UI's art kit (tools/gen/gen_ui.py) and what it commits to
assets/base: the frames, the icon atlas and the kit's data."""
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import REPO_ROOT  # noqa: E402  (also puts tools/ on sys.path)

GEN_DIR = REPO_ROOT / "tools" / "gen"
if str(GEN_DIR) not in sys.path:
    sys.path.insert(0, str(GEN_DIR))

import gen_ui  # noqa: E402
import lua_literal  # noqa: E402
from pngw import read_png  # noqa: E402

BASE = REPO_ROOT / "assets" / "base"
SCRIPTS = BASE / "scripts"


def kit_data():
    text = (SCRIPTS / "ui" / "kit_data.lua").read_text(encoding="utf-8")
    return lua_literal.parse(text[text.index("return"):])


class GenUiTest(unittest.TestCase):
    # spec: ui-art-kit: The kit is complete and reproducible
    def test_reproducible_and_committed(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b:
            for d in (a, b):
                (Path(d) / "scripts" / "ui").mkdir(parents=True)
                gen_ui.main(d)
            for rel in ("ui/frames.png", "ui/icons.png", "scripts/ui/kit_data.lua"):
                first, second = (Path(a) / rel).read_bytes(), (Path(b) / rel).read_bytes()
                self.assertEqual(first, second, rel)
                self.assertEqual(first, (BASE / rel).read_bytes(), f"{rel} is not the committed file")

    def test_frames_fit_their_cells(self):
        data = kit_data()
        w, h, _ = read_png(BASE / "ui" / "frames.png")
        for name, f in data["frames"].items():
            x, y, fw, fh = f["src"]
            left, top, right, bottom = f["border"]
            self.assertLessEqual(x + fw, w, name)
            self.assertLessEqual(fh + data["contrast_offset"], h, name)
            self.assertLess(left + right, fw, name)
            self.assertLess(top + bottom, fh, name)
        for name in ("kilim", "thread", "plate", "chip", "page", "band", "focus"):
            self.assertIn(name, data["frames"])

    def test_every_name_the_scripts_use_exists(self):
        data = kit_data()
        used_icons, used_colours = set(), set()
        for path in list((SCRIPTS / "ui").glob("*.lua")) + [SCRIPTS / "main.lua"]:
            text = path.read_text(encoding="utf-8")
            used_icons |= set(re.findall(r'kit\.icon\("([a-z_]+)"', text))
            used_icons |= set(re.findall(r'icon = "([a-z_]+)"', text))
            used_icons |= set(re.findall(r'= "((?:pad|mouse)_[a-z]+|touch)"', text))
            used_colours |= set(re.findall(r'\bC\("([a-z_]+)"', text))
            used_colours |= set(re.findall(r'kit\.colour\("([a-z_]+)"', text))
        self.assertTrue(used_icons, "the scripts use icons")
        for name in sorted(used_icons):
            self.assertIn(name, data["icons"], f"icon {name}")
        for name in sorted(used_colours):
            self.assertIn(name, data["palette"], f"colour {name}")
        for colour in data["mana_order"]:
            self.assertIn("mana_" + colour, data["icons"])

    def test_mana_sets_stay_apart(self):
        gen_ui.check_mana_sets()
        old = dict(gen_ui.MANA["normal"])
        try:
            gen_ui.MANA["normal"]["void"] = gen_ui.MANA["normal"]["earth"]
            with self.assertRaises(ValueError):
                gen_ui.check_mana_sets()
        finally:
            gen_ui.MANA["normal"].update(old)


if __name__ == "__main__":
    unittest.main()

# SPDX-License-Identifier: MIT
# REUSE-IgnoreStart
"""Tests for tools/check_credits.py."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase, run_main  # noqa: E402

import check_credits  # noqa: E402

PACKAGE = 'return { id = "base" }\n'


def credits(*entries: str) -> str:
    return "-- SPDX-License-Identifier: CC-BY-4.0\nreturn {\n" + "".join(f"  {e},\n" for e in entries) + "}\n"


def entry(file, license="CC-BY-4.0", author="Zauberer contributors", source="tools/gen/x.py", extra=""):
    return (f'{{ file = "{file}", author = "{author}", source = "{source}", '
            f'license = "{license}"{extra} }}')


class CheckCreditsTest(TempDirTestCase):
    def make_package(self, path="assets/base", files=None, entries=()):
        tree = {f"{path}/package.lua": PACKAGE, f"{path}/credits.lua": credits(*entries)}
        for name, content in (files or {}).items():
            tree[f"{path}/{name}"] = content
        self.write(tree)
        return self.root / path

    def check(self, package):
        return run_main(check_credits.main, "--root", self.root, "--package", package)

    def test_fully_credited_package_passes(self):
        package = self.make_package(
            files={"sprites/mote.png": b"\x89PNG", "sounds/chime.wav": b"RIFF", "scripts/main.lua": "x = 1\n",
                   "sprites/mote.lua": "return {}\n"},
            entries=[entry("sprites/mote.png"), entry("sounds/chime.wav", license="CC0-1.0")])
        code, out = self.check(package)
        self.assertEqual(code, 0, out)
        self.assertIn("assets/base: 2 credited asset(s), OK", out)

    def test_new_sprite_without_credit(self):
        # spec: content-licensing: New sprite without credit
        package = self.make_package(
            files={"sprites/mote.png": b"\x89PNG", "sprites/new_sprite.png": b"\x89PNG"},
            entries=[entry("sprites/mote.png")])
        code, out = self.check(package)
        self.assertEqual(code, 1)
        self.assertIn("assets/base/sprites/new_sprite.png: asset file has no credits entry", out)
        self.assertNotIn("sprites/mote.png:", out)

    def test_all_asset_extensions_need_credits(self):
        names = [f"x/file{ext.upper() if i % 2 else ext}" for i, ext in
                 enumerate(sorted(check_credits.ASSET_EXTENSIONS))]
        package = self.make_package(files={name: b"data" for name in names})
        code, out = self.check(package)
        self.assertEqual(code, 1)
        for name in names:
            self.assertIn(f"assets/base/{name}: asset file has no credits entry", out)

    def test_entry_pointing_to_missing_file(self):
        package = self.make_package(entries=[entry("sprites/gone.png")])
        code, out = self.check(package)
        self.assertEqual(code, 1)
        self.assertIn("assets/base/sprites/gone.png: credits entry points to a missing file", out)

    def test_incompatible_third_party_asset(self):
        # spec: content-licensing: Incompatible third-party asset
        package = self.make_package(
            files={"music/theme.ogg": b"OggS"},
            entries=[entry("music/theme.ogg", license="CC-BY-NC-4.0", author="Someone Else",
                           source="https://example.org/theme")])
        code, out = self.check(package)
        self.assertEqual(code, 1)
        self.assertIn("assets/base/music/theme.ogg: license 'CC-BY-NC-4.0' is not on the allowlist", out)

    def test_exception_with_rationale_allows_other_license(self):
        package = self.make_package(
            files={"fonts/f.ttf": b"\0\1"},
            entries=[entry("fonts/f.ttf", license="OFL-1.1",
                           extra=', exception = "SIL OFL font, compatible for bundling; approved in review"')])
        code, out = self.check(package)
        self.assertEqual(code, 0, out)

    def test_blank_exception_is_not_a_rationale(self):
        package = self.make_package(
            files={"fonts/f.ttf": b"\0\1"},
            entries=[entry("fonts/f.ttf", license="OFL-1.1", extra=', exception = "   "')])
        code, out = self.check(package)
        self.assertEqual(code, 1)
        self.assertIn("license 'OFL-1.1' is not on the allowlist", out)

    def test_author_source_license_must_not_be_empty(self):
        package = self.make_package(
            files={"a.png": b"x", "b.png": b"x"},
            entries=[entry("a.png", author="", source=" "), '{ file = "b.png" }'])
        code, out = self.check(package)
        self.assertEqual(code, 1)
        self.assertIn("assets/base/a.png: credits entry has an empty or missing 'author'", out)
        self.assertIn("assets/base/a.png: credits entry has an empty or missing 'source'", out)
        for key in ("author", "source", "license"):
            self.assertIn(f"assets/base/b.png: credits entry has an empty or missing '{key}'", out)

    def test_malformed_entries(self):
        package = self.make_package(
            files={"a.png": b"x"},
            entries=[entry("a.png"), entry("a.png"), '"just a string"', "{ author = 'x' }",
                     entry("../outside.png")])
        code, out = self.check(package)
        self.assertEqual(code, 1)
        self.assertIn("assets/base/a.png: credited more than once", out)
        self.assertIn("entry #3 is not a table", out)
        self.assertIn("entry #4 has no 'file'", out)
        self.assertIn("entry #5: file '../outside.png' must be a path relative to the package", out)

    def test_missing_or_invalid_credits_file(self):
        self.write({"assets/a/package.lua": PACKAGE, "assets/a/x.png": b"x",
                    "assets/b/package.lua": PACKAGE, "assets/b/credits.lua": "return { { file = } }",
                    "assets/c/package.lua": PACKAGE, "assets/c/credits.lua": "return 'nope'"})
        code, out = run_main(check_credits.main, "--root", self.root)
        self.assertEqual(code, 1)
        self.assertIn("assets/a/credits.lua: missing credits file (1 asset file(s) need entries)", out)
        self.assertIn("assets/b/credits.lua: cannot parse credits:", out)
        self.assertIn("assets/c/credits.lua: cannot parse credits: credits.lua must return a list", out)

    def test_whole_repository_checks_assets_and_mods(self):
        self.make_package("assets/base", files={"a.png": b"x"}, entries=[entry("a.png")])
        self.make_package("mods/dragons", files={"d.glb": b"glTF"})
        self.write({"assets/README.png.txt": "not a package file\n"})
        code, out = run_main(check_credits.main, "--root", self.root)
        self.assertEqual(code, 1)
        self.assertIn("assets/base: 1 credited asset(s), OK", out)
        self.assertIn("mods/dragons/d.glb: asset file has no credits entry", out)

    def test_nested_package_assets_belong_to_the_nested_package(self):
        self.make_package("mods/outer", files={"o.png": b"x"}, entries=[entry("o.png")])
        self.make_package("mods/outer/inner", files={"i.png": b"x"}, entries=[entry("i.png")])
        self.write({"mods/outer/.hidden.png": b"x"})
        code, out = run_main(check_credits.main, "--root", self.root)
        self.assertEqual(code, 0, out)
        self.assertIn("mods/outer/inner: 1 credited asset(s), OK", out)

    def test_usage_errors(self):
        code, _ = run_main(check_credits.main, "--root", self.root)  # no assets/ or mods/
        self.assertEqual(code, 2)
        code, _ = run_main(check_credits.main, "--root", self.root, "--package", self.root / "nope")
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
# REUSE-IgnoreEnd

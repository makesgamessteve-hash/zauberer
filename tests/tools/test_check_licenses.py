# SPDX-License-Identifier: MIT
"""Tests for tools/check_licenses.py."""
import shutil
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase, run_main  # noqa: E402

import check_licenses  # noqa: E402

REUSE_TOML = """# SPDX-License-Identifier: MIT
version = 1

[[annotations]]
path = ["assets/**", "docs/**"]
SPDX-License-Identifier = "CC-BY-4.0"

[[annotations]]
path = "third_party/lua/**"
SPDX-License-Identifier = "MIT"

[[annotations]]
path = "third_party/stb/**"
SPDX-License-Identifier = ["MIT", "Unlicense"]
"""

PNG = b"\x89PNG\r\n\x1a\n\0\0\0\rIHDR"


class CheckLicensesTest(TempDirTestCase):
    def setUp(self):
        super().setUp()
        self.write({
            "REUSE.toml": REUSE_TOML,
            "LICENSE": "MIT License ...\n",
            "LICENSES/MIT.txt": "MIT License ...\n",
            "CMakeLists.txt": "# SPDX-License-Identifier: MIT\nproject(x)\n",
            "cmake/ZbTesting.cmake": "# SPDX-License-Identifier: MIT\n",
            "src/core/base.c": "/* SPDX-License-Identifier: MIT */\nint x;\n",
            "tools/tool.py": "#!/usr/bin/env python3\n# SPDX-License-Identifier: MIT\n",
            "tests/unit/test_a.c": "// SPDX-License-Identifier: MIT\n",
            "web/index.html": "<!DOCTYPE html>\n<!-- SPDX-License-Identifier: MIT -->\n",
            "assets/base/package.lua": "-- SPDX-License-Identifier: CC-BY-4.0\nreturn { id = 'base' }\n",
            "assets/base/scripts/main.lua": "-- SPDX-License-Identifier: MIT\n",
            "assets/base/shaders/model.vert": "// SPDX-License-Identifier: MIT\n",
            "assets/base/sprites/mote.png": PNG,
            "assets/base/sounds/chime.wav": b"RIFF....WAVE",
            "docs/plan.md": "# Plan\n",
            "third_party/lua/lapi.c": "/* no header */\n",
            "third_party/stb/stb_image.h": "/* no header */\n",
            "third_party/cgltf/cgltf.h": "/* SPDX-License-Identifier: MIT */\n",
            "build/generated.c": "int generated;\n",
            ".git/config": "[core]\n",
            "openspec/.gitkeep": "",
        })

    def run_tool(self, *args):
        return run_main(check_licenses.main, "--root", self.root, *args)

    def test_correctly_licensed_tree_passes(self):
        code, out = self.run_tool()
        self.assertEqual(code, 0, out)
        self.assertIn("15 files checked", out)

    def test_file_without_license_information(self):
        # spec: content-licensing: File without license information
        self.write({"tools/notes.txt": "no header here\n", "misc/data.bin": b"\0\1\2"})
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("tools/notes.txt: no license information", out)
        self.assertIn("misc/data.bin: no license information", out)
        self.assertIn("2 problem(s)", out)

    def test_asset_declared_with_the_wrong_license(self):
        # spec: content-licensing: Asset declared with the wrong license
        self.write({"REUSE.toml": REUSE_TOML + '\n[[annotations]]\npath = "assets/base/sprites/*.png"\n'
                                                'SPDX-License-Identifier = "MIT"\n'})
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("assets/base/sprites/mote.png: declared MIT (REUSE.toml), expected CC-BY-4.0", out)

    def test_code_declared_with_the_wrong_license(self):
        self.write({"src/core/base.c": "/* SPDX-License-Identifier: CC-BY-4.0 */\n",
                    "assets/base/scripts/main.lua": "-- SPDX-License-Identifier: CC0-1.0\n",
                    "cmake/x.cmake": "# SPDX-License-Identifier: Apache-2.0\n",
                    "docs/CMakeLists.txt": "# SPDX-License-Identifier: CC-BY-4.0\n"})
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("src/core/base.c: declared CC-BY-4.0 (SPDX header), expected MIT", out)
        self.assertIn("assets/base/scripts/main.lua: declared CC0-1.0 (SPDX header), expected MIT", out)
        self.assertIn("cmake/x.cmake: declared Apache-2.0 (SPDX header), expected MIT", out)
        self.assertIn("docs/CMakeLists.txt: declared CC-BY-4.0 (SPDX header), expected MIT", out)

    def test_header_must_be_in_the_first_five_lines(self):
        self.write({"src/late.c": "\n\n\n\n\n/* SPDX-License-Identifier: MIT */\n"})
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("src/late.c: no license information", out)

    def test_last_matching_annotation_wins(self):
        self.write({"REUSE.toml": REUSE_TOML + '\n[[annotations]]\npath = "docs/**"\n'
                                                'SPDX-License-Identifier = "MIT"\n',
                    "src/x.png": PNG})
        info = check_licenses.license_of(self.root, "docs/plan.md", check_licenses.load_reuse_toml(self.root))
        self.assertEqual((info.license, info.source), ("MIT", "REUSE.toml"))
        code, out = self.run_tool()
        self.assertEqual(code, 1)  # src/x.png has no annotation
        self.assertIn("src/x.png: no license information", out)

    def test_header_wins_unless_annotation_overrides(self):
        self.write({"docs/own.md": "<!-- SPDX-License-Identifier: MIT -->\n"})
        annotations = check_licenses.load_reuse_toml(self.root)
        self.assertEqual(check_licenses.license_of(self.root, "docs/own.md", annotations).license, "MIT")
        self.write({"REUSE.toml": REUSE_TOML + '\n[[annotations]]\npath = "docs/own.md"\n'
                                                'precedence = "override"\nSPDX-License-Identifier = "CC0-1.0"\n'})
        annotations = check_licenses.load_reuse_toml(self.root)
        self.assertEqual(check_licenses.license_of(self.root, "docs/own.md", annotations).license, "CC0-1.0")

    def test_sidecar_license_file(self):
        self.write({"src/icon.png": PNG, "src/icon.png.license": "SPDX-License-Identifier: MIT\n"})
        code, out = self.run_tool()
        self.assertEqual(code, 0, out)

    def test_third_party_is_exempt_from_the_split_but_needs_metadata(self):
        self.write({"third_party/lua/lapi.c": "/* SPDX-License-Identifier: GPL-2.0-only */\n",
                    "third_party/new/thing.c": "int x;\n",
                    "third_party/new/thing.png": PNG})
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertNotIn("third_party/lua/lapi.c", out)
        self.assertIn("third_party/new/thing.c: no license information", out)
        self.assertIn("third_party/new/thing.png: no license information", out)

    def test_credited_third_party_asset_uses_the_credited_license(self):
        self.write({
            "assets/base/credits.lua": "-- SPDX-License-Identifier: CC-BY-4.0\nreturn {\n"
                                       "  { file = 'sounds/chime.wav', author = 'A', source = 'B', license = 'CC0-1.0' },\n"
                                       "}\n",
        })
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("assets/base/sounds/chime.wav: declared CC-BY-4.0 (REUSE.toml), expected CC0-1.0 "
                      "(as credited in assets/base/credits.lua)", out)
        self.write({"REUSE.toml": REUSE_TOML + '\n[[annotations]]\npath = "assets/base/sounds/chime.wav"\n'
                                                'SPDX-License-Identifier = "CC0-1.0"\n'})
        code, out = self.run_tool()
        self.assertEqual(code, 0, out)

    def test_git_file_list(self):
        if shutil.which("git") is None:
            self.skipTest("git is not installed")
        shutil.rmtree(self.root / ".git")
        self.write({".gitignore": "# SPDX-License-Identifier: MIT\n/ignored.txt\n/build/\n",
                    "ignored.txt": "no header\n", "untracked.txt": "no header\n"})
        subprocess.run(["git", "init", "-q"], cwd=self.root, check=True)
        subprocess.run(["git", "add", "-A"], cwd=self.root, check=True)
        self.write({"untracked.txt": "no header\n"})
        code, out = self.run_tool()
        self.assertEqual(code, 1)
        self.assertIn("untracked.txt: no license information", out)
        self.assertNotIn("ignored.txt", out)
        self.assertIn("17 files", out)
        files = check_licenses.list_files(self.root)
        self.assertIn(".gitignore", files)
        self.assertNotIn("build/generated.c", files)

    def test_spdx_value_extraction(self):
        cases = {
            "/* SPDX-License-Identifier: MIT */": "MIT",
            "<!-- SPDX-License-Identifier: CC-BY-4.0 -->": "CC-BY-4.0",
            "-- SPDX-License-Identifier: MIT OR Apache-2.0": "MIT OR Apache-2.0",
            '{"license": "SPDX-License-Identifier: MIT",': "MIT",
            "# SPDX-License-Identifier:": None,
        }
        for text, expected in cases.items():
            with self.subTest(text=text):
                self.assertEqual(check_licenses.spdx_from_text(text), expected)

    def test_reuse_glob_semantics(self):
        annotation = check_licenses.Annotation(("assets/*/scripts/**", "a/*.png"), "MIT", "closest")
        self.assertTrue(annotation.matches("assets/base/scripts/x/y.lua"))
        self.assertFalse(annotation.matches("assets/base/sub/scripts/y.lua"))
        self.assertTrue(annotation.matches("a/b.png"))
        self.assertFalse(annotation.matches("a/b/c.png"))

    def test_invalid_reuse_toml_is_an_error(self):
        for text in ("[[annotations]\n", '[[annotations]]\nSPDX-License-Identifier = "MIT"\n',
                     '[[annotations]]\npath = "x"\nprecedence = "sometimes"\n'):
            self.write({"REUSE.toml": text})
            code, out = self.run_tool()
            self.assertEqual(code, 2, text)
            self.assertIn("REUSE.toml", out)


if __name__ == "__main__":
    unittest.main()

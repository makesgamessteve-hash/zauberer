# SPDX-License-Identifier: MIT
# REUSE-IgnoreStart
"""Tests for tools/check_includes.py."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import TempDirTestCase, run_main  # noqa: E402

import check_includes  # noqa: E402

# The dependency rules from design.md section 2 / src/CMakeLists.txt.
EXPECTED_ALLOWED = {
    "core": {"core"},
    "platform": {"core", "platform"},
    "vfs": {"core", "vfs"},
    "input": {"core", "input"},
    "render": {"core", "vfs", "render"},
    "audio": {"core", "vfs", "audio"},
    "script": {"core", "vfs", "input", "render", "audio", "script"},
    "app": {"core", "platform", "vfs", "input", "render", "audio", "script", "app"},
}


class CheckTextTest(unittest.TestCase):
    def test_dependency_matrix(self):
        for module, allowed in EXPECTED_ALLOWED.items():
            for target in EXPECTED_ALLOWED:
                text = f'#include "{target}/{target}.h"\n'
                violations = check_includes.check_text(module, text, "x.c")
                with self.subTest(module=module, target=target):
                    self.assertEqual(not violations, target in allowed)

    def test_violation_message_names_file_line_module_and_header(self):
        text = '/* SPDX-License-Identifier: MIT */\n#include "core/base.h"\n\n#include "render/render.h"\n'
        violations = check_includes.check_text("core", text, "src/core/a.c")
        self.assertEqual(len(violations), 1)
        self.assertEqual(violations[0].line, 4)
        self.assertTrue(str(violations[0]).startswith(
            'src/core/a.c:4: module core must not include "render/render.h"'))

    def test_sdl_and_emscripten_only_in_platform(self):
        text = "#include <SDL3/SDL.h>\n#include <SDL3/SDL_main.h>\n#include <emscripten.h>\n" \
               "#include <emscripten/html5.h>\n"
        self.assertEqual(check_includes.check_text("platform", text), [])
        for module in ("core", "render", "app", "script"):
            violations = check_includes.check_text(module, text)
            self.assertEqual([v.line for v in violations], [1, 2, 3, 4], module)
            self.assertIn("only platform may use SDL", violations[0].message)
            self.assertIn("<SDL3/SDL.h>", violations[0].message)

    def test_lua_headers_only_in_script(self):
        text = '#include "lua.h"\n#include "lauxlib.h"\n#include "lualib.h"\n#include <lua.h>\n'
        self.assertEqual(check_includes.check_text("script", text), [])
        violations = check_includes.check_text("app", text)
        self.assertEqual(len(violations), 4)
        self.assertIn('module app must not include "lua.h"', str(violations[0]))

    def test_system_and_third_party_headers_are_allowed(self):
        text = '#include <stdio.h>\n#include <sys/stat.h>\n#include "stb_image.h"\n#include "cgltf.h"\n'
        self.assertEqual(check_includes.check_text("core", text), [])

    def test_comments_are_ignored_and_lines_kept(self):
        text = ('// #include "render/render.h"\n'
                '/* old:\n#include "audio/audio.h"\n*/\n'
                'static const char *s = "/* not a comment";\n'
                '#  include "input/input.h" // trailing comment\n')
        violations = check_includes.check_text("vfs", text, "v.c")
        self.assertEqual([(v.line, v.message.split(" (")[0]) for v in violations],
                         [(6, 'module vfs must not include "input/input.h"')])

    def test_unknown_module_is_rejected(self):
        with self.assertRaises(ValueError):
            check_includes.check_text("network", "")


class CheckTreeTest(TempDirTestCase):
    def test_clean_tree_passes(self):
        self.write({
            "src/CMakeLists.txt": "# not a source file\n",
            "src/core/base.h": "#pragma once\n",
            "src/render/render.c": '#include "render/render.h"\n#include "vfs/vfs.h"\n',
            "src/platform/sdl.c": "#include <SDL3/SDL.h>\n",
        })
        code, out = run_main(check_includes.main, "--root", self.root)
        self.assertEqual(code, 0, out)
        self.assertIn("3 files checked, no violations", out)

    def test_violations_fail_with_file_and_line(self):
        self.write({
            "src/core/log.c": '#include "core/log.h"\n#include "platform/platform.h"\n',
            "src/render/backend_gl.c": "#include <SDL3/SDL.h>\n",
            "src/weird/x.c": "int x;\n",
        })
        code, out = run_main(check_includes.main, "--root", self.root)
        self.assertEqual(code, 1)
        self.assertIn('src/core/log.c:2: module core must not include "platform/platform.h"', out)
        self.assertIn("src/render/backend_gl.c:1: module render must not include <SDL3/SDL.h>", out)
        self.assertIn("src/weird/x.c:1: unknown module 'weird'", out)
        self.assertIn("3 violation(s)", out)

    def test_missing_source_directory_is_an_error(self):
        code, _ = run_main(check_includes.main, "--root", self.root)
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
# REUSE-IgnoreEnd

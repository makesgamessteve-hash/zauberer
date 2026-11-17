# SPDX-License-Identifier: MIT
"""Tests for tools/lua_literal.py (the parser behind check_credits.py)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import toolsupport  # noqa: E402,F401  (puts tools/ on sys.path)

import lua_literal  # noqa: E402
from lua_literal import LuaSyntaxError, parse  # noqa: E402


class LuaLiteralTest(unittest.TestCase):
    def test_credits_style_list(self):
        text = """-- SPDX-License-Identifier: CC-BY-4.0
        -- comment
        return {
          { file = "a.png", author = 'Someone', license = "CC0-1.0" },
          { ["file"] = "b.wav"; exception = [[long
rationale]] },
        }
        """
        self.assertEqual(parse(text), [
            {"file": "a.png", "author": "Someone", "license": "CC0-1.0"},
            {"file": "b.wav", "exception": "long\nrationale"},
        ])

    def test_scalars(self):
        self.assertEqual(parse("return 42"), 42)
        self.assertEqual(parse("-1.5e2"), -150.0)
        self.assertEqual(parse("0x1F"), 31)
        self.assertEqual(parse("0x.8p1"), 1.0)
        self.assertEqual(parse(".5"), 0.5)
        self.assertIs(parse("true"), True)
        self.assertIs(parse("false"), False)
        self.assertIsNone(parse("nil"))

    def test_string_escapes(self):
        self.assertEqual(parse(r'"a\tb\n\"q\" \\ \x41\65\u{48}\z   c"'), 'a\tb\n"q" \\ AAHc')
        self.assertEqual(parse("'it\\'s'"), "it's")
        self.assertEqual(parse('"line\\\nnext"'), "line\nnext")
        self.assertEqual(parse("[==[a]]b]==]"), "a]]b")
        self.assertEqual(parse("[[\nfirst newline skipped]]"), "first newline skipped")
        self.assertEqual(parse('"caf\\195\\169"'), "café")

    def test_tables(self):
        self.assertEqual(parse("{}"), [])
        self.assertEqual(parse("{ 1, 2, 3, }"), [1, 2, 3])
        self.assertEqual(parse("{ [2] = 'b', [1] = 'a' }"), ["a", "b"])
        self.assertEqual(parse("{ 'a', x = 1 }"), {1: "a", "x": 1})
        self.assertEqual(parse("{ x = nil, y = 2 }"), {"y": 2})
        self.assertEqual(parse("{ [1.0] = 'one' }"), ["one"])
        self.assertEqual(parse("{ [true] = 1 }"), {True: 1})
        self.assertEqual(parse("{ a = { b = { 'c' } } }"), {"a": {"b": ["c"]}})

    def test_comments(self):
        self.assertEqual(parse("--[[ block\ncomment ]] { --[==[ x ]==] 1 -- tail\n }"), [1])
        self.assertEqual(parse("#!/usr/bin/lua\nreturn 1"), 1)

    def test_errors_report_line(self):
        cases = ["{ x = }", "{ 1 2 }", "'unterminated", "{ a = b }", "return 1 2",
                 "[[open", '"bad \\q escape"', "{ [nil] = 1 }", "3x", "\n\n{ x = os.exit() }"]
        for text in cases:
            with self.subTest(text=text), self.assertRaises(LuaSyntaxError):
                parse(text)
        with self.assertRaises(LuaSyntaxError) as context:
            parse("{\n  a = 1,\n  b = c,\n}", "credits.lua")
        self.assertEqual(context.exception.line, 3)
        self.assertIn("credits.lua:3:", str(context.exception))

    def test_parse_file(self):
        path = Path(toolsupport.REPO_ROOT) / "assets" / "base" / "package.lua"
        if not path.is_file():
            self.skipTest("assets/base/package.lua not present")
        data = lua_literal.parse_file(path)
        self.assertIsInstance(data, dict)
        self.assertIn("id", data)


if __name__ == "__main__":
    unittest.main()

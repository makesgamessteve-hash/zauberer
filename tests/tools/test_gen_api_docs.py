# SPDX-License-Identifier: MIT
"""Tests for tools/gen_api_docs.py."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import REPO_ROOT, TempDirTestCase, run_main  # noqa: E402

import gen_api_docs  # noqa: E402

SOURCE = """
/* @lua render.model(path: string): integer
 * Loads a model.
 * Returns its handle. */
/* @lua engine.log(level: "info"|"warn", message: string) */
/* @lua print(...: any)
 * Prints. */
/* @lua load(chunk: string, env?: table): function?, string?
 * Compiles. */
/* @luaclass Info
 * @field name string The name
 * @field size integer?
 *
 * @luavar engine.version: string
 * The version.
 * @luavar package_id: string
 * The package.
 */
/* an ordinary comment */
"""


class GenApiDocsTest(TempDirTestCase):
    def test_parse_source(self):
        functions, classes, variables = gen_api_docs.parse_source(SOURCE, "x.c")
        names = [f.name for f in functions]
        self.assertEqual(names, ["render.model", "engine.log", "print", "load"])
        model = functions[0]
        self.assertEqual(model.params, [("path", "string", False)])
        self.assertEqual(model.returns, ["integer"])
        self.assertEqual(model.doc, "Loads a model. Returns its handle.")
        self.assertEqual(model.module, "render")
        self.assertEqual(functions[3].params, [("chunk", "string", False), ("env", "table", True)])
        self.assertEqual(functions[3].returns, ["function?", "string?"])
        self.assertEqual(functions[1].params[0], ("level", '"info"|"warn"', False))
        self.assertEqual(classes[0].fields, [("name", "string", "The name"), ("size", "integer?", "")])
        self.assertEqual([(v.name, v.type, v.doc) for v in variables],
                         [("engine.version", "string", "The version."), ("package_id", "string", "The package.")])

    def test_render_outputs(self):
        functions, classes, variables = gen_api_docs.parse_source(SOURCE, "x.c")
        md = gen_api_docs.render_markdown(functions, classes, variables)
        self.assertIn("### `render.model(path: string): integer`", md)
        self.assertIn("| `name` | `string` | The name |", md)
        self.assertIn("### `engine.version: string`", md)
        lua = gen_api_docs.render_stubs(functions, classes, variables)
        self.assertIn("---@param path string\n---@return integer\nfunction render.model(path) end", lua)
        self.assertIn("---@param env? table", lua)
        self.assertIn("---@param ... any\nfunction print(...) end", lua)
        self.assertIn("---@class Info", lua)
        self.assertIn("render = {}", lua)
        self.assertIn("---@type string\npackage_id = nil", lua)

    def test_bad_annotations(self):
        for bad in ("/* @lua broken( */", "/* @lua f(x) */", "/* @luavar nothing */",
                    "/* @luaclass C\n * @field onlyname */"):
            with self.assertRaises(gen_api_docs.ToolError):
                gen_api_docs.parse_source(bad, "x.c")

    def test_split_top(self):
        self.assertEqual(gen_api_docs.split_top("a: fun(x: int, y: int), b: { n: int, m: int }"),
                         ["a: fun(x: int, y: int)", "b: { n: int, m: int }"])

    def test_write_and_check(self):
        src = self.root / "src" / "script"
        src.mkdir(parents=True)
        for name in ("script.c", "script_api.c"):
            (src / name).write_text("", encoding="utf-8")
        (src / "script_test.c").write_text(SOURCE, encoding="utf-8")
        self.assertEqual(run_main(gen_api_docs.main, "--root", self.root, "--check")[0], 1)
        code, out = run_main(gen_api_docs.main, "--root", self.root)
        self.assertEqual(code, 0)
        self.assertIn("wrote", out)
        self.assertEqual(run_main(gen_api_docs.main, "--root", self.root, "--check")[0], 0)
        (src / "script_test.c").write_text(SOURCE + "/* @lua print(x: any) */", encoding="utf-8")
        code, out = run_main(gen_api_docs.main, "--root", self.root)
        self.assertEqual(code, 2)
        self.assertIn("documented more than once: print", out)
        for path in src.glob("*.c"):
            path.unlink()
        code, out = run_main(gen_api_docs.main, "--root", self.root)
        self.assertEqual(code, 2)
        self.assertIn("no C sources", out)

    def test_repository_docs_are_current(self):
        self.assertEqual(run_main(gen_api_docs.main, "--root", REPO_ROOT, "--check")[0], 0,
                         "run tools/gen_api_docs.py")


if __name__ == "__main__":
    unittest.main()

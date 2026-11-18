# SPDX-License-Identifier: MIT
"""Generates the Lua API reference from annotations in the C bindings.

    gen_api_docs.py [--root DIR] [--check]

Annotations live in C comments of src/script/*.c (all files, in name order):

    /* @lua render.model(path: string): integer
     * Starts loading a glTF model; returns its handle. */

    @luaclass Name            followed by lines '@field name type description'
    @luavar name: type        followed by description lines

Outputs docs/api/README.md (reference) and docs/api/zauberer.d.lua (type
stubs for the Lua language server). With --check nothing is written and the
exit code is 1 when the files are out of date.

Exit codes: 0 = up to date / written, 1 = out of date, 2 = usage or parse error.
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

from tool_common import EXIT_ERROR, EXIT_FAIL, EXIT_OK, REPO_ROOT, ToolError

SOURCE_DIR = "src/script"
OUT_MD = "docs/api/README.md"
OUT_LUA = "docs/api/zauberer.d.lua"
SPDX_TAG = "SPDX-" + "License-Identifier:"  # split so license scanners skip it
MODULE_ORDER = ("engine", "vfs", "input", "render", "audio", "save", "world", "net", "test", "math", "")
MODULE_TITLES = {
    "engine": "engine: lifecycle, time and state",
    "vfs": "vfs: package files",
    "input": "input: actions and bindings",
    "render": "render: models, sprites, text and camera",
    "audio": "audio: sounds and music",
    "save": "save: player data",
    "world": "world: planet coordinates and terrain",
    "net": "net: co-op sessions",
    "test": "test: Lua tests",
    "math": "math: deterministic randomness",
    "": "Globals",
}

_COMMENT_RE = re.compile(r"/\*(.*?)\*/", re.S)
_SIG_RE = re.compile(r"^(?P<name>[A-Za-z_][\w.]*)\((?P<params>.*)\)(?:\s*:\s*(?P<ret>.+))?$")


@dataclass
class Function:
    name: str
    params: list[tuple[str, str, bool]]  # (name, type, optional)
    returns: list[str]
    doc: str
    signature: str

    @property
    def module(self) -> str:
        return self.name.rsplit(".", 1)[0] if "." in self.name else ""


@dataclass
class LuaClass:
    name: str
    fields: list[tuple[str, str, str]] = field(default_factory=list)  # (name, type, doc)


@dataclass
class Variable:
    name: str
    type: str
    doc: str


def _comment_lines(body: str) -> list[str]:
    lines = []
    for line in body.splitlines():
        line = line.strip()
        if line.startswith("*"):
            line = line[1:].strip()
        lines.append(line)
    return lines


def split_top(text: str, sep: str = ",") -> list[str]:
    """Splits at sep outside (), {} and []."""
    parts, depth, cur = [], 0, ""
    for ch in text:
        if ch in "({[":
            depth += 1
        elif ch in ")}]":
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur.strip())
    return parts


def parse_signature(sig: str, where: str) -> tuple[str, list[tuple[str, str, bool]], list[str]]:
    m = _SIG_RE.match(sig.strip())
    if not m:
        raise ToolError(f"{where}: cannot parse signature '{sig}'")
    params = []
    for p in split_top(m.group("params")):
        if ":" not in p:
            raise ToolError(f"{where}: parameter '{p}' lacks a type in '{sig}'")
        pname, ptype = (x.strip() for x in p.split(":", 1))
        optional = pname.endswith("?")
        params.append((pname.rstrip("?"), ptype, optional))
    returns = split_top(m.group("ret")) if m.group("ret") else []
    return m.group("name"), params, returns


def parse_source(text: str, path: str) -> tuple[list[Function], list[LuaClass], list[Variable]]:
    functions, classes, variables = [], [], []
    for cm in _COMMENT_RE.finditer(text):
        lines = _comment_lines(cm.group(1))
        line_no = text.count("\n", 0, cm.start()) + 1
        i = 0
        while i < len(lines):
            line = lines[i]
            where = f"{path}:{line_no + i}"
            if line.startswith("@lua "):
                sig = line[5:].strip()
                doc = []
                i += 1
                while i < len(lines) and lines[i] and not lines[i].startswith("@"):
                    doc.append(lines[i])
                    i += 1
                name, params, returns = parse_signature(sig, where)
                functions.append(Function(name, params, returns, " ".join(doc), sig))
                continue
            if line.startswith("@luaclass "):
                cls = LuaClass(line.split(None, 1)[1].strip())
                i += 1
                while i < len(lines) and lines[i].startswith("@field "):
                    parts = lines[i].split(None, 3)
                    if len(parts) < 3:
                        raise ToolError(f"{path}:{line_no + i}: '@field name type [description]' expected")
                    cls.fields.append((parts[1], parts[2], parts[3] if len(parts) > 3 else ""))
                    i += 1
                classes.append(cls)
                continue
            if line.startswith("@luavar "):
                decl = line[8:]
                if ":" not in decl:
                    raise ToolError(f"{where}: '@luavar name: type' expected")
                vname, vtype = (x.strip() for x in decl.split(":", 1))
                doc = []
                i += 1
                while i < len(lines) and lines[i] and not lines[i].startswith("@"):
                    doc.append(lines[i])
                    i += 1
                variables.append(Variable(vname, vtype, " ".join(doc)))
                continue
            i += 1
    return functions, classes, variables


def _module_key(name: str) -> tuple[int, str]:
    return (MODULE_ORDER.index(name) if name in MODULE_ORDER else len(MODULE_ORDER), name)


def render_markdown(functions: list[Function], classes: list[LuaClass], variables: list[Variable]) -> str:
    out = ["<!-- " + SPDX_TAG + " MIT -->",
           "<!-- Generated by tools/gen_api_docs.py from src/script/*.c. Do not edit. -->",
           "# Zauberer Lua API", "",
           "Package scripts run in a sandbox (no `io`, `os`, `dofile`, `loadfile` or binary chunks). "
           "Every package has its own environment; `require` loads modules from the package overlay. "
           "Functions marked *Test API* exist only in Lua tests (`--test`) and scenarios (`--scenario`).", ""]
    modules = sorted({f.module for f in functions}, key=_module_key)
    for mod in modules:
        out += [f"## {MODULE_TITLES.get(mod, mod)}", ""]
        for v in sorted((v for v in variables if v.name.rsplit(".", 1)[0] == mod or
                         (mod == "" and "." not in v.name)), key=lambda v: v.name):
            out += [f"### `{v.name}: {v.type}`", "", v.doc, ""]
        for f in sorted((f for f in functions if f.module == mod), key=lambda f: f.name):
            out += [f"### `{f.signature}`", "", f.doc, ""]
    out += ["## Types", ""]
    for cls in sorted(classes, key=lambda c: c.name):
        out += [f"### {cls.name}", "", "| Field | Type | Description |", "| --- | --- | --- |"]
        out += [f"| `{n}` | `{t.replace('|', ' or ')}` | {d} |" for n, t, d in cls.fields]
        out.append("")
    return "\n".join(out).rstrip() + "\n"


def _lua_type(t: str) -> str:
    return t.replace("function", "function")


def render_stubs(functions: list[Function], classes: list[LuaClass], variables: list[Variable]) -> str:
    out = ["-- " + SPDX_TAG + " MIT",
           "-- Generated by tools/gen_api_docs.py from src/script/*.c. Do not edit.",
           "---@meta", ""]
    for cls in sorted(classes, key=lambda c: c.name):
        out.append(f"---@class {cls.name}")
        out += [f"---@field {n} {t} {d}".rstrip() for n, t, d in cls.fields]
        out.append("")
    modules = sorted({f.module for f in functions} | {v.name.rsplit(".", 1)[0] for v in variables if "." in v.name},
                     key=_module_key)
    for mod in modules:
        if mod and mod != "math":
            out += [f"{mod} = {{}}", ""]
        for v in sorted((v for v in variables if v.name.rsplit(".", 1)[0] == mod and "." in v.name),
                        key=lambda v: v.name):
            out += [f"---{v.doc}", f"---@type {v.type}", f"{v.name} = nil", ""]
        for f in sorted((f for f in functions if f.module == mod), key=lambda f: f.name):
            out.append(f"---{f.doc}")
            for pname, ptype, optional in f.params:
                if pname == "...":
                    out.append(f"---@param ... {ptype}")
                else:
                    out.append(f"---@param {pname}{'?' if optional else ''} {_lua_type(ptype)}")
            out += [f"---@return {r}" for r in f.returns]
            out += [f"function {f.name}({', '.join(p[0] for p in f.params)}) end", ""]
    for v in sorted((v for v in variables if "." not in v.name), key=lambda v: v.name):
        out += [f"---{v.doc}", f"---@type {v.type}", f"{v.name} = nil", ""]
    return "\n".join(out).rstrip() + "\n"


def generate(root: Path) -> dict[str, str]:
    functions, classes, variables = [], [], []
    sources = sorted((root / SOURCE_DIR).glob("*.c"))
    if not sources:
        raise ToolError(f"no C sources in {SOURCE_DIR}")
    for path in sources:
        rel = f"{SOURCE_DIR}/{path.name}"
        try:
            text = path.read_text(encoding="utf-8")
        except OSError as error:  # pragma: no cover - race between glob and read
            raise ToolError(f"cannot read {rel}: {error}") from error
        f, c, v = parse_source(text, rel)
        functions += f
        classes += c
        variables += v
    names = [f.name for f in functions]
    duplicates = sorted({n for n in names if names.count(n) > 1})
    if duplicates:
        raise ToolError("documented more than once: " + ", ".join(duplicates))
    return {OUT_MD: render_markdown(functions, classes, variables), OUT_LUA: render_stubs(functions, classes, variables)}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=str(REPO_ROOT))
    parser.add_argument("--check", action="store_true", help="fail if the generated files are out of date")
    args = parser.parse_args(argv)
    root = Path(args.root)
    try:
        outputs = generate(root)
    except ToolError as error:
        print(f"gen_api_docs: error: {error}", file=sys.stderr)
        return EXIT_ERROR
    stale = []
    for rel, content in outputs.items():
        path = root / rel
        current = path.read_text(encoding="utf-8") if path.is_file() else None
        if current != content:
            stale.append(rel)
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
    if args.check and stale:
        print("gen_api_docs: out of date (run tools/gen_api_docs.py): " + ", ".join(stale), file=sys.stderr)
        return EXIT_FAIL
    print("gen_api_docs: " + ("wrote " + ", ".join(stale) if stale else "up to date"))
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())

# SPDX-License-Identifier: MIT
"""Parser for the literal subset of Lua used by data files such as
credits.lua and package.lua.

Supported: an optional leading 'return', table constructors with
'key = value', '[key] = value' and positional values (separated by ',' or
';'), strings in single or double quotes with the standard escapes, long
strings '[[...]]' / '[==[...]==]', numbers (decimal, hexadecimal, floats),
true, false, nil, unary minus on numbers, and '--' / '--[[ ]]' comments.

Tables become Python lists when their keys are exactly 1..n (an empty table
is an empty list) and dicts otherwise. nil values are dropped, as in Lua.
"""
from __future__ import annotations

import re
from pathlib import Path

_NUMBER_RE = re.compile(
    r"0[xX](?:[0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?|\.[0-9a-fA-F]+)(?:[pP][+-]?\d+)?"
    r"|(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?")
_NAME_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
_SIMPLE_ESCAPES = {"a": "\a", "b": "\b", "f": "\f", "n": "\n", "r": "\r", "t": "\t",
                   "v": "\v", "\\": "\\", '"': '"', "'": "'", "\n": "\n", "\r": "\n"}
_KEYWORD_VALUES = {"true": True, "false": False, "nil": None}


class LuaSyntaxError(ValueError):
    """A syntax error, with the 1-based line number in .line."""

    def __init__(self, message: str, line: int, name: str = "<string>"):
        super().__init__(f"{name}:{line}: {message}")
        self.line = line


class _Parser:
    def __init__(self, text: str, name: str):
        self.text = text
        self.name = name
        self.pos = 0

    # -- helpers ---------------------------------------------------------
    def error(self, message: str, pos: int | None = None) -> LuaSyntaxError:
        at = self.pos if pos is None else pos
        return LuaSyntaxError(message, self.text.count("\n", 0, at) + 1, self.name)

    def peek(self, offset: int = 0) -> str:
        index = self.pos + offset
        return self.text[index] if index < len(self.text) else ""

    def expect(self, char: str) -> None:
        self.skip()
        if self.peek() != char:
            found = repr(self.peek()) if self.peek() else "end of file"
            raise self.error(f"expected '{char}', found {found}")
        self.pos += 1

    def long_bracket_level(self, pos: int) -> int | None:
        """Level of a long bracket '[' '='* '[' starting at pos, or None."""
        text = self.text
        if pos >= len(text) or text[pos] != "[":
            return None
        end = pos + 1
        while end < len(text) and text[end] == "=":
            end += 1
        if end < len(text) and text[end] == "[":
            return end - pos - 1
        return None

    def read_long_bracket(self, level: int) -> str:
        start = self.pos
        body = self.pos + level + 2
        close = "]" + "=" * level + "]"
        end = self.text.find(close, body)
        if end < 0:
            raise self.error("unfinished long string or comment", start)
        content = self.text[body:end]
        self.pos = end + len(close)
        for newline in ("\r\n", "\n\r", "\n", "\r"):
            if content.startswith(newline):
                return content[len(newline):]
        return content

    def skip(self) -> None:
        """Skips whitespace and comments."""
        text = self.text
        while self.pos < len(text):
            char = text[self.pos]
            if char in " \t\r\n\f\v":
                self.pos += 1
            elif text.startswith("--", self.pos):
                self.pos += 2
                level = self.long_bracket_level(self.pos)
                if level is not None:
                    self.read_long_bracket(level)
                else:
                    newline = text.find("\n", self.pos)
                    self.pos = len(text) if newline < 0 else newline + 1
            else:
                break

    # -- values ----------------------------------------------------------
    def value(self):
        self.skip()
        char = self.peek()
        if char == "{":
            return self.table()
        if char in ("'", '"'):
            return self.quoted_string()
        if char == "[":
            level = self.long_bracket_level(self.pos)
            if level is None:
                raise self.error("unexpected '['")
            return self.read_long_bracket(level)
        if char == "-":
            self.pos += 1
            self.skip()
            number = self.number()
            if number is None:
                raise self.error("expected a number after '-'")
            return -number
        number = self.number()
        if number is not None:
            return number
        match = _NAME_RE.match(self.text, self.pos)
        if match and match.group(0) in _KEYWORD_VALUES:
            self.pos = match.end()
            return _KEYWORD_VALUES[match.group(0)]
        if match:
            raise self.error(f"unsupported identifier '{match.group(0)}' (only literals are allowed)")
        raise self.error(f"unexpected {repr(char) if char else 'end of file'}")

    def number(self):
        match = _NUMBER_RE.match(self.text, self.pos)
        if not match:
            return None
        if _NAME_RE.match(self.text, match.end()):
            raise self.error("malformed number")
        self.pos = match.end()
        token = match.group(0)
        if token[:2] in ("0x", "0X"):
            if any(c in token for c in ".pP"):
                return float.fromhex(token)
            return int(token, 16)
        if any(c in token for c in ".eE"):
            return float(token)
        return int(token)

    def quoted_string(self) -> str:
        text = self.text
        quote = text[self.pos]
        start = self.pos
        self.pos += 1
        out = bytearray()
        while True:
            if self.pos >= len(text):
                raise self.error("unfinished string", start)
            char = text[self.pos]
            if char == quote:
                self.pos += 1
                return out.decode("utf-8", errors="replace")
            if char in "\r\n":
                raise self.error("unfinished string", start)
            if char != "\\":
                out += char.encode("utf-8")
                self.pos += 1
                continue
            self.pos += 1
            esc = self.peek()
            if esc in _SIMPLE_ESCAPES:
                self.pos += 1
                if esc in "\r\n" and self.peek() in "\r\n" and self.peek() != esc:
                    self.pos += 1
                out += _SIMPLE_ESCAPES[esc].encode("utf-8")
            elif esc == "x":
                digits = text[self.pos + 1:self.pos + 3]
                if not re.fullmatch(r"[0-9a-fA-F]{2}", digits):
                    raise self.error("hexadecimal digit expected in '\\x' escape")
                out.append(int(digits, 16))
                self.pos += 3
            elif esc == "z":
                self.pos += 1
                while self.peek() and self.peek() in " \t\r\n\f\v":
                    self.pos += 1
            elif esc.isdigit():
                match = re.match(r"\d{1,3}", text[self.pos:])
                code = int(match.group(0))
                if code > 255:
                    raise self.error("decimal escape too large")
                out.append(code)
                self.pos += len(match.group(0))
            elif esc == "u":
                match = re.match(r"u\{([0-9a-fA-F]+)\}", text[self.pos:])
                if not match or int(match.group(1), 16) > 0x10FFFF:
                    raise self.error("invalid '\\u{XXX}' escape")
                out += chr(int(match.group(1), 16)).encode("utf-8", errors="surrogatepass")
                self.pos += len(match.group(0))
            else:
                raise self.error(f"invalid escape sequence '\\{esc}'")

    def table(self):
        self.pos += 1  # '{'
        entries: dict = {}
        index = 1
        while True:
            self.skip()
            char = self.peek()
            if char == "}":
                self.pos += 1
                break
            if char == "[" and self.long_bracket_level(self.pos) is None:
                self.pos += 1
                key = self.value()
                self.expect("]")
                self.expect("=")
                if key is None:
                    raise self.error("table index is nil")
                if isinstance(key, float) and key.is_integer():
                    key = int(key)
                value = self.value()
                if value is not None:
                    entries[key] = value
                else:
                    entries.pop(key, None)
            else:
                name = _NAME_RE.match(self.text, self.pos)
                after = name.end() if name else self.pos
                while after < len(self.text) and self.text[after] in " \t\r\n\f\v":
                    after += 1
                if (name and self.text.startswith("=", after)
                        and not self.text.startswith("==", after)):
                    self.pos = after + 1
                    value = self.value()
                    if value is not None:
                        entries[name.group(0)] = value
                    else:
                        entries.pop(name.group(0), None)
                else:
                    value = self.value()
                    if value is not None:
                        entries[index] = value
                    index += 1
            self.skip()
            char = self.peek()
            if char in (",", ";"):
                self.pos += 1
            elif char == "}":
                self.pos += 1
                break
            else:
                raise self.error(f"expected ',' or '}}' in table, found "
                                 f"{repr(char) if char else 'end of file'}")
        count = len(entries)
        if all(type(key) is int for key in entries) and set(entries) == set(range(1, count + 1)):
            return [entries[i] for i in range(1, count + 1)]
        return entries

    def chunk(self):
        if self.text.startswith("#"):
            newline = self.text.find("\n")
            self.pos = len(self.text) if newline < 0 else newline
        self.skip()
        match = _NAME_RE.match(self.text, self.pos)
        if match and match.group(0) == "return":
            self.pos = match.end()
        result = self.value()
        self.skip()
        if self.peek() == ";":
            self.pos += 1
            self.skip()
        if self.pos < len(self.text):
            raise self.error("unexpected text after the value")
        return result


def parse(text: str, name: str = "<string>"):
    """Parses a Lua literal chunk and returns the Python value."""
    return _Parser(text, name).chunk()


def parse_file(path: Path):
    """Parses a Lua literal file (UTF-8). Raises OSError or LuaSyntaxError."""
    return parse(Path(path).read_text(encoding="utf-8", errors="replace"), str(path))

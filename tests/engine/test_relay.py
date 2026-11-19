# SPDX-License-Identifier: MIT
"""Process-level tests of zauberer-relay: the program starts, hands out an
invite code, forwards between a host and a client and stops on a signal.

Environment: ZB_RELAY_BIN (the relay executable; default: next to ZB_BIN).
"""
from __future__ import annotations

import base64
import os
import re
import signal
import socket
import struct
import subprocess
import sys
import unittest
from pathlib import Path

ALPHABET = "23456789ABCDEFGHJKMNPQRSTUVWXYZ"


def relay_binary() -> Path:
    value = os.environ.get("ZB_RELAY_BIN")
    if not value and os.environ.get("ZB_BIN"):
        name = "zauberer-relay.exe" if sys.platform == "win32" else "zauberer-relay"
        value = str(Path(os.environ["ZB_BIN"]).with_name(name))
    if not value or not Path(value).exists():
        raise unittest.SkipTest("the relay executable is not available")
    if os.environ.get("ZB_RUNNER"):
        raise unittest.SkipTest("the relay is a native program")
    return Path(value)


class WebSocket:
    """A minimal WebSocket client (binary messages only)."""

    def __init__(self, port: int) -> None:
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall((f"GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
        self.buf = b""
        while b"\r\n\r\n" not in self.buf:
            self.buf += self._recv()
        head, self.buf = self.buf.split(b"\r\n\r\n", 1)
        if b" 101 " not in head:
            raise AssertionError(head.decode(errors="replace"))

    def _recv(self) -> bytes:
        data = self.sock.recv(65536)
        if not data:
            raise ConnectionError("closed")
        return data

    def send(self, payload: bytes) -> None:
        mask = os.urandom(4)
        n = len(payload)
        head = bytes([0x82]) + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n))
        self.sock.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def _need(self, n: int) -> None:
        while len(self.buf) < n:
            self.buf += self._recv()

    def recv(self) -> bytes:
        """The next data message (server frames are not masked)."""
        while True:
            self._need(2)
            opcode, n = self.buf[0] & 0x0F, self.buf[1] & 0x7F
            start = 2
            if n == 126:
                self._need(4)
                n, start = struct.unpack(">H", self.buf[2:4])[0], 4
            elif n == 127:
                self._need(10)
                n, start = struct.unpack(">Q", self.buf[2:10])[0], 10
            self._need(start + n)
            payload, self.buf = self.buf[start:start + n], self.buf[start + n:]
            if opcode == 8:
                raise ConnectionError("closed by the relay")
            if opcode in (1, 2):
                return payload

    def close(self) -> None:
        self.sock.close()


class RelayProcessTest(unittest.TestCase):
    def setUp(self) -> None:
        self.bin = relay_binary()

    def test_help_and_usage_errors(self):
        done = subprocess.run([str(self.bin), "--help"], capture_output=True, text=True, timeout=30)
        self.assertEqual(done.returncode, 0)
        self.assertIn("usage: zauberer-relay", done.stdout)
        done = subprocess.run([str(self.bin), "--bogus"], capture_output=True, text=True, timeout=30)
        self.assertEqual(done.returncode, 2)
        self.assertIn("unknown option --bogus", done.stdout)

    # spec: coop-sessions: Relay and invite codes
    # spec: coop-sessions: Joining through the relay
    def test_forwards_between_host_and_client(self):
        proc = subprocess.Popen([str(self.bin), "--port", "0", "--seed", "7"], stdout=subprocess.PIPE, text=True)
        try:
            line = proc.stdout.readline()
            m = re.search(r"listening on port (\d+)", line)
            self.assertIsNotNone(m, line)
            port = int(m.group(1))
            host = WebSocket(port)
            host.send(bytes([1, 1]))  # register, protocol 1
            reply = host.recv()
            self.assertEqual(reply[0], 0x81)
            code = reply[1:].decode()
            self.assertEqual(len(code), 6)
            self.assertTrue(all(c in ALPHABET for c in code), code)
            client = WebSocket(port)
            client.send(bytes([2, 1]) + code.lower().encode())  # join with the code, typed in lower case
            self.assertEqual(client.recv(), bytes([0x82]))
            opened = host.recv()
            self.assertEqual(opened[0], 0x84)
            cid = opened[1:5]
            client.send(b"hello host")
            self.assertEqual(host.recv(), bytes([0x85]) + cid + b"hello host")
            host.send(bytes([3]) + cid + b"hello client")
            self.assertEqual(client.recv(), b"hello client")
            stranger = WebSocket(port)
            stranger.send(bytes([2, 1]) + b"ZZZZZZ")
            refusal = stranger.recv()
            self.assertEqual(refusal[0], 0x83)
            self.assertIn(b"invite code", refusal)
            for ws in (host, client, stranger):
                ws.close()
        finally:
            if sys.platform == "win32":
                proc.terminate()
            else:
                proc.send_signal(signal.SIGINT)
            out, _ = proc.communicate(timeout=30)
        if sys.platform != "win32":
            self.assertEqual(proc.returncode, 0, out)
            self.assertIn("zauberer-relay stopped", out)


if __name__ == "__main__":
    unittest.main()

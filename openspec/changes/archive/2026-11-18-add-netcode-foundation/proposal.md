# Proposal: Netcode Foundation

## Why

Milestone **M1 (First Flight)** locks in the co-op architecture early (implementation
plan §1.11, decisions D17 and D18): its exit demo has two carpets, a desktop host and a
browser client, flying together over the same terrain, which each side generates
locally. Every later gameplay change states its co-op behaviour and builds on the
transport, sessions and replication pattern introduced here.

## What Changes

- **Transport interface** (`net/transport`): connections, messages and events behind
  one interface, with a **loopback** implementation for tests and a **WebSocket**
  (RFC 6455) implementation: a server and a client over native sockets (POSIX and
  Winsock) and a client over the browser's WebSocket (`web/lib_net.js`).
- **Sessions** (`net/session`): host (player 0) and client roles, up to four players,
  a handshake that checks protocol version, world seed and package set, refusals with a
  reason, player indices (lowest free), joined and left events, named channels to one
  player or everyone (relayed by the host, which keeps the sender), pings with round-trip
  times and a 10 s silence timeout.
- **Lua API** `net.host`, `net.join`, `net.leave`, `net.status`, `net.send`,
  `net.receive`, with a binary codec for nil, booleans, numbers, strings and tables
  (16 levels, 256 KiB); command-line options `--host [port]` and `--join ws://...`.
- **Co-op flight** in the base package (`flight/coop.lua`): clients upload one input per
  tick; the host simulates every carpet and sends snapshots with the last applied input
  (20 per second); clients predict their own carpet with the deterministic flight model
  and reconcile, and dead-reckon the others. The host sends terrain chunk hashes every
  two seconds, which clients compare with their own generation.
- Two-process tests: native host and native client (headless), and native host with a
  Chromium client (Playwright).

## Capabilities

### New Capabilities

- `netcode`: transports, sessions and player indices, channels and the value codec,
  host-authoritative replication with client prediction, and replicated-state checks.

### Modified Capabilities

None.

## Impact

- New module `src/net/` (depends on `core` only), `src/core/sha1.{h,c}` (SHA-1 and
  Base64 for the WebSocket handshake), `src/script/script_net.c`, `web/lib_net.js`,
  `src/app/engine.c` (session creation, `zb_net_update` before the script tick),
  `src/core/cli.{h,c}`, `src/platform` (`zb_platform_sleep_ms` for scenarios),
  `assets/base/scripts/flight/coop.lua`, `assets/base/scripts/main.lua`.
- Platforms:
  - **Desktop** (Windows, Linux) can host and join.
  - **Web** can only join, because browsers cannot listen for connections; a page served
    over HTTPS can only reach `wss://` addresses, so browser play beyond `localhost`
    waits for the relay with TLS (M9, `add-coop-sessions`).
  - **Mobile web** joins like the desktop browser.
  - **macOS** is out of scope for now.
- Co-op: this change is the co-op foundation. Up to four heirs fly together; the host is
  the authority for every carpet; each peer streams terrain around its own heirs.

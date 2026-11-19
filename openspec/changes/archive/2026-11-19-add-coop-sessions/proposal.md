# Proposal: Co-op sessions: relay, invite codes, LAN, joining and the world bundle

## Why

M9 (Together) completes co-op for up to four heirs (concept 11.6): players meet through a small
relay by invite code or on the LAN, cross-play between desktop and browsers, and joiners get the
world once and keep it.

## What Changes

- **Relay** (`src/net/relay.c`, `zauberer-relay`, MIT, self-hostable): invite codes from an
  unambiguous alphabet, forwarding only, sessions of up to three clients, idle time-outs.
- **Relay transports**: hosting through the relay (also in the browser, which cannot listen) and
  joining by code; `net.host_relay`, `net.join_code`.
- **LAN discovery** (`src/net/lan.c`, UDP, desktop only): `net.lan_announce`, `net.lan_scan`.
- **Protocol 2**: refused joins list the differing packages; blobs (the world bundle) in
  flow-controlled chunks checked by SHA-1, kept in a cache in the user folder
  (`net.send_blob`, `net.blob_*`); `world.export_bundle`, `world.play_bundle`.
- **Game side** (`coop/session.lua`, `coop/play.lua`): hosting and joining, the world sent to
  joiners, the session's events; the exit demo `tests/engine/test_together.py`.
- Web: relay hosting and joining only (no LAN, no listening); the relay must sit behind a TLS
  proxy for https pages. M9.

## Impact

- New capability `coop-sessions`. Protocol version 2 (older builds are refused). New executable.

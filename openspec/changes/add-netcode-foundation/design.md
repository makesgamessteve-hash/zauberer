# Design: Netcode Foundation

## Context

The plan (§1.11) fixes the topology (host-authoritative, up to four heirs), the
transport (WebSocket everywhere, a relay later) and what travels (inputs, entity states,
deltas and hashes, never the generated world). M1 needs the smallest slice of this that
already has the final shape: the session layer, one replicated entity type (carpets),
prediction and a world check.

## Goals / Non-Goals

**Goals:**

- One protocol for desktop and browser peers, testable without a network.
- Player indices and per-player input from the start; no "the player" in the engine.
- Exact client prediction thanks to the deterministic flight model.
- A replicated-state check (terrain hashes) that catches desync between peers.

**Non-Goals:**

- The relay, invite codes, LAN discovery, TLS and reconnecting (M9, `add-coop-sessions`).
- Interest management, delta compression, bandwidth budgets and network-condition
  simulation (later changes, when there is more than carpets to replicate).
- WebRTC (a later transport behind the same interface).

## Decisions

### Transport interface with three implementations

`zb_transport` is a small vtable: `update`, `next` (open, message, close events per
connection id), `send`, `close`, `destroy`. Implementations:

- **Loopback**: an in-memory network with one listener; used by unit tests, so session
  logic is tested deterministically with a fake clock.
- **WebSocket over sockets**: a non-blocking server (accepts, handshakes, frames) and
  client. The protocol code (`net/ws`) is a pure state machine fed with bytes, so
  handshake, framing, fragmentation, masking, pings and close are unit-tested without
  sockets. Only binary messages are used; messages are capped at 1 MiB.
- **Browser WebSocket** (`web/lib_net.js`): the browser does the protocol; messages are
  queued in JavaScript and copied into the heap when polled.

Alternative: SDL_net. It does not do WebSocket, and the socket layer we need is small.

### Session protocol

Every message starts with a type byte. A client sends `HELLO` (protocol version, world
seed, package-set hash); the host answers `WELCOME` (player index, players present) or
`REJECT` (reason text) and tells everyone `JOINED`/`LEFT`. `DATA` carries a channel
name, a destination (player or everyone) and the payload; the host forwards client
messages addressed to others and stamps the real sender. `PING`/`PONG` every second
measure round trips; ten seconds without any message end the session.

Clients and host must generate the same world, so the seed and the package set (ids and
versions in load order) are part of the handshake rather than discovered by a desync.

### Replication in the game layer

The session carries opaque messages; what travels is decided in Lua (`flight/coop.lua`),
like all game rules. The Lua codec is binary, typed (integers and floats stay distinct)
and bounded. M1 replicates carpets:

- Clients send each tick's input with a sequence number. TCP delivers in order, so no
  redundancy is needed yet; unreliable transports will add it.
- The host applies at most two queued inputs per carpet per tick (catching up without
  bursts) and sends snapshots of all carpets every third tick with the last applied
  input per player.
- A client keeps the predicted snapshot per input. When a host snapshot acknowledges an
  input, the prediction for that input must equal it exactly (the model is
  deterministic), which is counted; the client then restores the host state and
  replays its unacknowledged inputs.
- Other carpets are dead-reckoned with their last known input between snapshots.

### Replicated-state check

Every two seconds the host sends the hash of the chunk under its carpet
(`world.chunk_hash`, heights generated at stage 1); clients generate the same chunk
locally and compare. A mismatch is counted and logged; resynchronisation arrives with
deltas in later milestones.

### Engine integration

The engine creates the session object at start (`--host`/`--join` start a session
before the scripts run) and updates it before each script tick, so messages are
processed at a fixed point of the tick. Headless runs drive sessions too, which is how
the two-process tests run.

## Risks / Trade-offs

- [TCP head-of-line blocking at high loss] → acceptable for co-op (D18); WebRTC later.
- [A slow client's inputs pile up at the host] → catch-up limit of two per tick; inputs
  beyond that wait, which shows as latency, not as a desync.
- [Browsers cannot host, and HTTPS pages need `wss://`] → browser play beyond
  `localhost` needs the relay with TLS (M9); documented in the proposal.
- [Floating-point differences between platforms would break exact prediction] → the
  flight model uses the deterministic math library; the browser test checks a native
  host against a WebAssembly client.

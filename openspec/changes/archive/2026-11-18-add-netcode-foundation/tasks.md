# Tasks

## 1. Transport

- [x] 1.1 Add SHA-1 and Base64 to core and the WebSocket protocol state machine (`net/ws`); verify "netcode: WebSocket protocol" with C unit tests
- [x] 1.2 Add the transport interface with loopback, native socket server and client, and the browser WebSocket client (`web/lib_net.js`); verify "netcode: Real sockets" with a C unit test over localhost

## 2. Sessions

- [x] 2.1 Implement the session protocol: handshake checks, player indices, joined and left, routing, pings and timeouts; verify "netcode: Player indices", "netcode: Refused joins" and "netcode: Leaving and silence" with loopback unit tests
- [x] 2.2 Create the session in the engine, update it every tick, and add `--host` and `--join`

## 3. Scripts

- [x] 3.1 Add the `net` Lua API and the value codec; verify "netcode: Values on channels" and "netcode: Scripts host and join" with C unit tests
- [x] 3.2 Add `sleep` to scenarios for multi-process tests, and regenerate the Lua API reference

## 4. Co-op flight

- [x] 4.1 Add host-authoritative carpets with client prediction and reconciliation (`flight/coop.lua`) and wire them into the base package
- [x] 4.2 Add the terrain hash check
- [x] 4.3 Verify "netcode: Host and client", "netcode: Host authority", "netcode: Prediction matches the host" and "netcode: Same terrain on both sides" with a two-process headless test
- [x] 4.4 Verify "netcode: Desktop host and browser client" with a Playwright test against a native host, built in the web CI job
- [x] 4.5 Map every scenario to a tagged test and keep coverage ≥ 95 %

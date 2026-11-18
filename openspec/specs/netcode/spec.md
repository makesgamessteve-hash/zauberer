# netcode Specification

## Purpose
Defines how games connect for co-op: transports, sessions and player indices, channels
that carry script values, host-authoritative replication with client prediction, and
checks that every peer generates the same world.

## Requirements

### Requirement: Transports
The engine SHALL exchange messages through one transport interface with a loopback implementation for tests and a WebSocket (RFC 6455, binary messages up to 1 MiB) implementation: server and client over native sockets on desktop, and a client over the browser's WebSocket on the web, where hosting is unavailable.

#### Scenario: WebSocket protocol
- **WHEN** a client and a server exchange the opening handshake, messages of every length class, a fragmented masked message with a ping in between, malformed frames, oversized messages and a close
- **THEN** messages arrive intact, the ping is answered, malformed or oversized input ends the connection with a reason, and both sides end up closed

#### Scenario: Real sockets
- **WHEN** a WebSocket server listens on a free localhost port and a client connects, exchanges a message each way and closes
- **THEN** both sides see the connection open, the messages arrive, the server reports the close, and connecting to a bad address or a port in use fails with a reason

### Requirement: Sessions and player indices
A host SHALL be player 0 and accept up to three clients that run the same protocol version, world seed and package set, giving each the lowest free player index; other joins SHALL be refused with a reason. Every peer SHALL learn who joined and left; when the host leaves or stays silent for ten seconds, the clients' session SHALL end with a reason.

#### Scenario: Player indices
- **WHEN** clients join a host one after another
- **THEN** the first is player 1, the second player 2, and the host lists players 0, 1 and 2

#### Scenario: Refused joins
- **WHEN** a client with a different world seed joins, and a fourth client joins a session with three
- **THEN** the first is refused with "the host plays a different world (seed)" and the fourth with "the session is full", and neither gets a player index

#### Scenario: Leaving and silence
- **WHEN** a client leaves, then the host stops answering, and in another session the host leaves
- **THEN** the remaining peers learn that the client left, the clients' sessions end with "the host stopped answering" and "the host ended the session"

### Requirement: Channels and values
Peers SHALL send messages on named channels to one player or to everyone; the host SHALL forward clients' messages and keep their sender. Scripts SHALL send nil, booleans, integers, floats, strings and tables of them (up to 16 levels and 256 KiB) and receive them unchanged; other values SHALL be refused with a reason and malformed data SHALL arrive without a value.

#### Scenario: Values on channels
- **WHEN** nested tables with integer, float, boolean and string keys and values are encoded and decoded, and functions, table keys, deep nesting, oversized values and malformed bytes are tried
- **THEN** the values come back equal with integers still integers, and the others fail with a reason or do not decode

#### Scenario: Scripts host and join
- **WHEN** a script hosts a session, a client joins, the script sends a table to the client and receives a malformed message from it
- **THEN** the script sees the joined event and both players in the status, the client decodes the table, and the malformed message arrives without a value

### Requirement: Host-authoritative flight
The host SHALL simulate every heir's carpet from the inputs its client sends each tick and send snapshots of all carpets with the last applied input of each player at least 20 times per second. A client SHALL predict its own carpet with the same flight model, reconcile with each snapshot by replaying unacknowledged inputs, and continue the other carpets between snapshots.

#### Scenario: Host and client
- **WHEN** a headless host runs and a second process joins it with `--join`
- **THEN** the client becomes player 1 and gets a carpet beside the host's, and both runs end successfully

#### Scenario: Host authority
- **WHEN** the client steers its carpet for five seconds
- **THEN** the host moves the client's carpet more than 100 m from the client's inputs

#### Scenario: Prediction matches the host
- **WHEN** at least 20 host snapshots acknowledge inputs the client predicted
- **THEN** every acknowledged prediction equals the host's state exactly

### Requirement: Same world everywhere
Peers SHALL generate the world locally from the seed and packages; the host SHALL send hashes of terrain it generates at least every two seconds, and clients SHALL compare them with their own generation and count mismatches.

#### Scenario: Same terrain on both sides
- **WHEN** a client receives terrain hashes from the host during a flight
- **THEN** each equals the hash of the client's own generation of that chunk

### Requirement: Every platform plays together
A desktop host SHALL accept clients running natively and in a browser, with the same protocol and results; sessions SHALL be started from scripts or with the command-line options `--host [port]` and `--join <ws-url>`.

#### Scenario: Desktop host and browser client
- **WHEN** a native headless host runs and the WebAssembly build in Chromium joins it with `--join`
- **THEN** the browser client flies with zero mispredictions and matching terrain, and the host's run passes

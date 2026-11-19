# coop-sessions Specification

## Purpose
Co-op sessions for up to four heirs: the relay and invite codes, LAN discovery, joining, package checks, and the world sent to joiners and cached; the M9 exit demo.

## Requirements

### Requirement: Relay and invite codes
The relay SHALL give a registering host a six-character invite code from an alphabet without 0, O, 1, I and L, unique among its sessions; refuse unknown codes and a fourth client; forward opaque frames between the host and each client; end a session's clients when its host leaves and time out idle sessions.

#### Scenario: Relay and invite codes
- **WHEN** hosts register, clients join by codes good and bad, and hosts leave
- **THEN** codes are as described, bad ones and full sessions are refused, frames pass both ways and sessions end with their hosts

### Requirement: Joining through the relay
A host SHALL host through the relay (on the web too) and its invite code appear in its status; a client SHALL join with the code (any case, spaces and dashes) and play as if joined directly.

#### Scenario: Joining through the relay
- **WHEN** a host and clients play a session through a relay
- **THEN** they get player indices and their messages travel as in a direct session

### Requirement: LAN discovery
A desktop host SHALL answer LAN queries with its session's name, players and port; a scan SHALL list the sessions heard, dropping those silent for three seconds; in the browser both SHALL fail with "unavailable".

#### Scenario: LAN discovery
- **WHEN** a host announces and a client scans
- **THEN** the session is listed with its details and drops out when the host stops

### Requirement: Package mismatches are listed
A join refused for different packages SHALL list each package that differs with its version on both sides.

#### Scenario: Package mismatches are listed
- **WHEN** a client with other packages joins
- **THEN** the join is refused and the differences are listed

### Requirement: World bundle transfer and cache
Large blobs SHALL travel in chunks with progress, flow-controlled so channels keep flowing, checked by their hash; the joiner SHALL keep them in a cache by hash, damaged files being removed.

#### Scenario: World bundle transfer and cache
- **WHEN** a bundle is sent, cached and loaded again
- **THEN** it arrives whole with progress, is cached and loads without a transfer

### Requirement: The world sent to joiners
A created world SHALL be sent as its bundle's bytes and played from them; joiners SHALL get the world's seed and name, then the bundle and history as blobs, using the cache when they have the bundle already.

#### Scenario: The world sent to joiners
- **WHEN** a created world is exported and played again, and a joiner receives the world twice
- **THEN** the bytes play the same world, other bytes are refused, and the second time the cache is used

### Requirement: Hosting and joining
The game SHALL host through the relay or on the LAN and join by code or address, telling refusals and the invite code, and follow joins, leaves and the session's end.

#### Scenario: Hosting and joining
- **WHEN** the game hosts and joins in every way, with failures
- **THEN** it shows the code, tells why a join failed and follows the session

### Requirement: The M9 exit demo
Four heirs SHALL join one world by invite code, ride the whole Weave, split up into two regions, reunite, pitch the tent and vote to sleep; the host saves and quits, and the next day all rejoin the saved world with their heirs.

#### Scenario: The M9 exit demo
- **WHEN** the demo is played by a relay, a host and three clients
- **THEN** each step happens for all of them and the second day finds the heirs and the world as left

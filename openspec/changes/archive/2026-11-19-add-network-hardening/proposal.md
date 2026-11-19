# Proposal: Network hardening

## Why

Real networks have latency, drops and limited bandwidth (implementation plan 1.11): 64 kbit/s per
client on average, 256 kbit/s peak, up to 150 ms latency.

## What Changes

- C: loopback impairment for tests (latency, jitter, cut connections), resume tokens (rejoin
  within 30 s keeps the player index), bandwidth budgets per connection with "latest" channels,
  per-player traffic stats.
- `coop/hardening.lua`: interpolation of others in the past, reconnecting with growing waits,
  interest by distance, state hashes and desync detection, channel classes.
- Platform: none beyond the relay's (see coop-sessions). M9.

## Impact

- New capability `network-hardening`. Co-op: the host's state wins a desync.

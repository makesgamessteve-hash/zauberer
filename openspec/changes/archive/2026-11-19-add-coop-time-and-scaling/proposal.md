# Proposal: Time, votes, scaling, friendly fire and revival

## Why

Playing together changes time and danger (concept 11.4, 11.5).

## What Changes

- `coop/together.lua`: sleep and time-skip votes (all in the home, or all agree; a wait request
  refuses), joint rituals, creature and mage scaling (not resources), friendly fire on by default
  with a softening option, revival with life magic or medicine before the carpet carries the
  downed heir home.
- `coop/play.lua`: votes asked and cast by message (sleep in co-op opens or agrees to a vote), the
  host's clock sent to all and followed by the clients.
- Platform: none. M9.

## Impact

- New capability `coop-time-and-scaling`. Co-op: the host's clock and votes rule.

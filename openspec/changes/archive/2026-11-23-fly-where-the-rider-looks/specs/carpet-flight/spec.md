## MODIFIED Requirements

### Requirement: Handling
The carpet SHALL fly at a speed set by the forward input (60 m/s at full forward) while forward is held, boost to 150 m/s and ease back after a boost, slow quickly (40 m/s²) when forward is let go and harder (60 m/s²) when braking, turn at up to 70° per second (less at speed) while banking into the turn, and climb or dive with the climb input.

#### Scenario: Speeds
- **WHEN** full forward is held for ten seconds, then boost for five, then forward alone for ten, then the brake
- **THEN** the speed reaches 60 m/s, then 150 m/s, eases back towards 60 m/s, and drops to zero

#### Scenario: Letting go
- **WHEN** the carpet flies at 60 m/s and forward is let go, and again with half forward held
- **THEN** it stops within 1.6 s of letting go, and with half forward it settles at 30 m/s

#### Scenario: Turning
- **WHEN** full right steering is held for one second at rest, and again at top speed
- **THEN** the heading turns 70° at rest, less than 40° per second at top speed, and the carpet banks into the turn

### Requirement: Looking around from the seat
The rider SHALL look around up to 115° to either side and from 57° down to 40° up, moving the mouse or the stick up looking up (unless the look is inverted in the settings), the mouse's view staying where it was put and a stick's or finger's drifting back ahead 1.5 s after the last look; casting SHALL aim where the rider looks; mounting, a revival and a teleport SHALL look ahead again.

#### Scenario: Looking around
- **WHEN** the rider looks with a stick and with the mouse beyond the limits, moves the mouse up, lets go for a while, looks fully down, and remounts
- **THEN** the look stops at the limits, moving up looks up, the mouse's view stays, the stick's returns ahead, it goes no lower than the limit, and after remounting the view is ahead

## ADDED Requirements

### Requirement: Flying where the rider looks
While the carpet moves or is urged forward it SHALL turn towards where the rider looks (at the full turn rate 30° or more off its heading) and climb or dive with the look's pitch (level within 12° of the horizon, full climb or dive at 35°), the view staying where the rider put it while the carpet turns under it; hovering, the rider SHALL look around without turning the carpet; the steering and climb inputs SHALL still add to the look's.

#### Scenario: Turning to the look
- **WHEN** the rider holds forward and looks 60° to the right, and then looks 30° up
- **THEN** the carpet turns right until it flies where the rider looks, the view staying on the same bearing, and then climbs

#### Scenario: Looking around while hovering
- **WHEN** the carpet hovers and the rider looks 90° to the left
- **THEN** the carpet does not turn

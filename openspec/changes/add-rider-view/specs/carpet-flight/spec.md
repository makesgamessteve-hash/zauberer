## ADDED Requirements

### Requirement: The rider's view
In flight the view SHALL be out of the heir's eyes, seated at the back of the carpet 1 m above it, looking ahead and a little down, so that only the carpet's front is ever on screen; the view SHALL lean with 40 % of the carpet's bank.

#### Scenario: The rider's view
- **WHEN** the view is taken over a carpet flying north, level and then banked 30°
- **THEN** the eye is above the back of the carpet, the carpet's front edge is in view and its back edge is not, the view is level and then leans less than the carpet

### Requirement: Looking around from the seat
The rider SHALL look around up to 115° to either side and from 57° down to 40° up, the mouse's view staying where it was put and a stick's or finger's drifting back ahead 1.5 s after the last look; casting SHALL aim where the rider looks; mounting, a revival and a teleport SHALL look ahead again.

#### Scenario: Looking around
- **WHEN** the rider looks with a stick and with the mouse beyond the limits, lets go for a while, looks fully down, and remounts
- **THEN** the look stops at the limits, the mouse's view stays, the stick's returns ahead, it goes no lower than the limit, and after remounting the view is ahead

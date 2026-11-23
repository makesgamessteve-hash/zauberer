# Proposal: Fly where the rider looks

Milestone: UI redesign (follows add-rider-view). Asked for after playing the rider's view.

## Why

Seated on the carpet, the player turned the view with the mouse but steered the carpet with the
keys, so the two pulled apart: the heir looked one way and flew another. Moving the mouse up looked
down. Once the carpet was going, it kept cruising with no key held, and it took a held brake to stop
it. A carpet that flies where its rider looks, goes while the rider urges it on and settles when
they stop, is easier to fly and closer to riding a living thing.

## What Changes

- **Up is up.** Moving the mouse or the right stick up looks up, in flight and on foot. The
  "Invert look" setting still turns it round for those who want it.
- **The carpet flies where the rider looks.** While it is moving or urged forward, it turns towards
  where the rider looks (full turn rate 30° or more off its heading) and climbs or dives with the
  look's pitch: within 12° of the horizon (the resting view among them) it stays level, and the
  climb is full at 35° up or down. The view stays where the rider put it while the carpet swings
  round under them.
  - Hovering, the rider looks around freely without turning the carpet.
  - The keys and the left stick still turn and climb, on top of the look. Touch steers as before
    (there is no look stick). In VR the reins and the head steer as before.
- **Forward goes, letting go settles.** Holding forward flies at a speed set by how far the stick is
  pushed (keys: full speed); letting go slows the carpet quickly (40 m/s², so from cruise to a halt
  in 1.5 s). Back brakes harder (60 m/s²). Boost still flies by itself (the touch button) and eases
  back to cruise when let go while forward is held.
- The help and the first flight hint say so.

## Platforms

All the same. Mouse and gamepad steer by looking; touch keeps its steering stick.

## Co-op

The look becomes turn and climb input before the carpet moves, so hosts and clients fly the same
flight from the same input as before.

## Impact

- `flight/camera.lua` (look sign, steering from the look, the view held while the carpet turns),
  `flight/carpet.lua` (speed held only while urged), `player/walker.lua` (look sign), `main.lua`
  (the look read before the flight), help texts and their German.
- The replay fixture is recorded again (the flight changes).
- Scenarios that flew on a single push of the throttle hold it now.

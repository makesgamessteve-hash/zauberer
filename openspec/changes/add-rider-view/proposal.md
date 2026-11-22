# Proposal: Out of the rider's eyes

Milestone: UI redesign. Comes before the six UI changes (docs/design/ui/README.md): every screen of
the redesign is drawn over this view.

## Why

The game is about one person riding a living carpet. The camera, though, hung 9 m behind and 3 m
above the carpet, so the player watched a rug fly instead of riding it, and the whole carpet sat in
the middle of every picture. The new UI keeps the middle of the screen for the world, and that world
should be the one the heir sees.

## What Changes

- In flight the view is out of the heir's eyes, seated at the back of the carpet, 1 m above it,
  looking ahead and a little down.
  - Only the carpet's front shows: its border and tassels low in the picture, cut by the screen's
    sides. The carpet is never wholly on screen.
- The rider looks around within a seated person's reach: up to 115° to either side, from 57° down
  to 40° up.
  - The mouse keeps the view where it was put. A stick's or a finger's view drifts back ahead
    1.5 s after the last look.
- The view leans with the carpet's bank (40 % of it), so turns are felt without tipping the horizon.
- Casting aims where the rider looks, and the Codex observes what the rider looks at.
- Mounting, a revival and a teleport look ahead again.
- On foot the view stays out of the eyes as before. In VR the seat is the session's origin, as
  before.
- The cinematic views of the social media footage (`tools/reels`) still circle the carpet; they are
  marketing shots, not the game's view.

## Platforms

All the same. The FOV setting applies.

## Co-op

Each heir sees out of their own eyes; the others' carpets are seen whole as they fly by.

## Impact

Modified capability `carpet-flight` (a requirement added). `flight/camera.lua` rewritten from the
chase camera into the rider's seat; `main.lua` (aim, the Codex's view, remounting); the camera's unit
test.

# Design: Carpet Flight

## Context

M0 shipped a demo with an orbiting camera around a hovering carpet. M1 needs real
flight over a 2,000 km planet, at up to 150 m/s, for at least ten minutes in any
direction, and the flight must be deterministic because replays, desync checks and
co-op prediction depend on it (concept §11.7, implementation plan §1.11).

## Decisions

### 1. Flight in Lua, world math in C

The flight model is a pure Lua module (state table + `update(c, input, dt, env)`), so it
is moddable and unit-testable with a flat fake world. Everything that touches the planet
is a C function exposed through `world`: `world.move(pos, heading, forward, right, up)`
moves along the tangent plane in double precision and carries the heading into the new
tangent plane (parallel transport), so straight flight follows a great circle and
crossing a pole does not flip the controls; `world.height(pos)` gives the analytic
terrain height for the collision floor (deterministic, unlike the streamed heightfield
whose presence depends on timing).

### 2. Deterministic script math

Lua's math library calls the platform's libm, whose transcendental functions differ in
the last bits between platforms. The sandbox replaces `math.sin`, `cos`, `tan`, `asin`,
`acos`, `atan` (one or two arguments), `exp` and `log` (with an optional base) with
`core/dmath`, and the Lua sources are compiled with `luai_numpow` routed to `zb_dpow` (a
force-included header), so `x ^ y` is deterministic too. `sqrt`, `floor`, `fmod` and
arithmetic are exact IEEE operations already.

### 3. Handling

| Quantity | Value |
| --- | --- |
| Cruise / boost top speed | 60 / 150 m/s |
| Acceleration (throttle / boost) / braking | 18 / 45 / 35 m/s² |
| Turn rate | 70°/s at rest, 45 % less at top speed |
| Climb | 22 m/s at full input, plus half the speed's pitch component; pitch up to 28° |
| Clearance / ceiling | 2.5 m above ground or sea / 6,000 m above sea level |

### 4. Camera, spawn, HUD

The chase camera sits 9 m behind and 3.2 m above, looks 12 m ahead, smooths its offset
from the carpet (so moves of the render origin need no handling) and orbits with the
look action. The spawn walks a deterministic spiral of probes from the centre of face 0
until the terrain is more than 15 m above sea level. The HUD shows speed (km/h), boost,
heading (degrees and compass), altitude above sea and ground, distance flown and the
controls for the input mode.

### 5. Modes

`demo.game.set_mode("flight" | "showcase")` switches between the flight (default) and
the M0 showcase, which keeps its scenarios and golden images valid. The state hash
contains the mode, the showcase state and the carpet snapshot.

## Risks / Trade-offs

- [The analytic floor differs from the drawn mesh by up to a metre at coarse levels] →
  2.5 m clearance; near terrain is drawn from the heightfield at 4 m.
- [Lua per-tick cost with four carpets] → a carpet update is a handful of C calls; well
  under 0.1 ms each.

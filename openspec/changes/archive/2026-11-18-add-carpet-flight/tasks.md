# Tasks

## 1. Engine support

- [x] 1.1 Replace the sandbox's transcendental math with core/dmath and route `^` through `zb_dpow`; verify "carpet-flight: Deterministic math"
- [x] 1.2 Add `world.move` (parallel-transported heading) to the world bindings and regenerate the API reference

## 2. The flight game

- [x] 2.1 Write the flight model; verify "Speeds", "Turning", "Climb and ground" and "Snapshot" with Lua unit tests
- [x] 2.2 Write the chase camera, spawn and HUD with Lua unit tests; switch the base package between flight and showcase and keep the M0 scenarios and goldens on the showcase
- [x] 2.3 Verify "Flying from input" and "Ten minutes in any direction" with headless scenarios
- [x] 2.4 Re-record the replay fixture with flight input; verify "Same flight on every platform" natively, in WebAssembly and on the Windows CI job
- [x] 2.5 Keep C and Lua coverage ≥ 95 % and map every scenario to a tagged test

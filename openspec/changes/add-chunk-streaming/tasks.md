# Tasks

## 1. Stream

- [x] 1.1 Implement ring scans, want/keep/target and refreshes; verify "chunk-streaming: Settled rings", "Higher means farther" and "Around a cube corner"
- [x] 1.2 Implement priorities, job slots, double-buffered results and inline budgets; verify "Ahead first", "In view first", "Job limit" and "Inline time budget"
- [x] 1.3 Implement hysteresis, cancellation, demotion and LRU eviction; verify "Flying along a ring boundary", "Teleport" and "Long flight with a small cap"
- [x] 1.4 Verify "Thirty seconds at full speed", "Profile values", "Two focus points" and "Same content as direct generation"
- [ ] 1.5 Add the release perf test "Ring refresh cost"

## 2. World object and integration

- [x] 2.1 Implement `zb_world` (planet, terrain, stream, focus points) and create and update it in the engine with the platform's profile
- [x] 2.2 Expose `world.set_focus`, `world.clear_focus`, `world.ground_height` and `world.stream_stats`; verify "Streaming from Lua" with a scenario; regenerate the API reference
- [x] 2.3 Keep C coverage ≥ 95 % and map every scenario to a tagged test

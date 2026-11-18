-- SPDX-License-Identifier: MIT
-- spec: chunk-streaming: Streaming from Lua
scenario { name = "terrain streams around a focus" }
local pos = world.position(0, 5000, 6000, 100, 100, 50)
expect_eq(world.chunk_stage(pos), -1, "nothing streams without a focus")
world.set_focus(1, pos, { 0, 0, 0 }, { 0, 0, -1 })
local ready = false
for _ = 1, 3000 do
  step(1)
  if world.chunk_stage(pos) == 2 and world.stream_stats().stage2 > 20 then
    ready = true
    break
  end
end
expect(ready, "the chunks around the focus reached stage 2")
local st = world.stream_stats()
expect(st.stage0 + st.stage2 > 20 and st.bytes > 0 and st.generated > 20)
expect(st.wanted > 1000, "the far ring is wanted")
expect_near(world.ground_height(pos), world.height(pos), 0.5, "loaded heightfield matches the terrain")
expect_error(function() world.set_focus(5, pos) end, "focus index")
world.clear_focus(1)
step(2)
expect_eq(world.stream_stats().wanted, 0)

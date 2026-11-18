-- SPDX-License-Identifier: MIT
-- Picks a starting place on land, deterministically from the world.
local M = {}

M.MIN_HEIGHT = 15 -- metres above sea level
M.STEP = 37 -- chunks between probes

-- height(pos) -> metres; n: chunks per face edge. Walks a spiral of probes from
-- the centre of face 0 and returns the first position on land.
function M.find(height, n)
  local cx, cy = n // 2, n // 2
  for ring = 0, 200 do
    for i = -ring, ring do
      for _, d in ipairs({ { i, -ring }, { i, ring }, { -ring, i }, { ring, i } }) do
        local x, y = cx + d[1] * M.STEP, cy + d[2] * M.STEP
        if x >= 0 and y >= 0 and x < n and y < n then
          local pos = { face = 0, x = x, y = y, lx = 128, ly = 128, alt = 0 }
          if height(pos) > M.MIN_HEIGHT then
            return pos
          end
        end
      end
    end
  end
  return { face = 0, x = cx, y = cy, lx = 128, ly = 128, alt = 0 }
end

return M

-- SPDX-License-Identifier: MIT
engine.on("hash", function()
  if engine.tick() >= 300 then
    return "drifted"
  end
  return nil
end)

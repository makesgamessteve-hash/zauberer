-- SPDX-License-Identifier: MIT
-- The new-world screen: deep time as a turning globe whose continents drift
-- frame by frame, then the finished world, ready to fly.
local M = {}

M.PIECES = 2   -- pieces of work per step (a deep-time step, an erosion pass, a month of climate)
M.SPIN = 0.25  -- globe turns per second, in radians

function M.new(options)
  local ok, err = world.create(options)
  return { ok = ok, err = err, spin = 0, progress = nil, finished = false }
end

function M.tick(s, dt)
  if not s.ok or s.finished then return end
  s.spin = s.spin + dt * M.SPIN
  if not (s.progress and s.progress.done) then
    s.progress = world.create_step(M.PIECES) or s.progress
  end
end

function M.ready(s)
  return s.ok and not s.finished and s.progress ~= nil and s.progress.done
end

function M.finish(s, name)
  local ok, err = world.create_finish(name)
  s.finished = ok
  s.err = err
  return ok, err
end

function M.cancel(s)
  if s.ok and not s.finished then
    world.create_cancel()
  end
  s.ok = false
end

function M.lines(s)
  local lines = { "A new world" }
  if not s.ok then
    lines[#lines + 1] = "Cannot create it: " .. tostring(s.err)
    return lines
  end
  local p = s.progress
  if not p then
    lines[#lines + 1] = "Starting..."
    return lines
  end
  lines[#lines + 1] = string.format("%s  %d%%", p.label, math.floor(p.fraction * 100 + 0.5))
  if p.stage == "deep time" then
    lines[#lines + 1] = string.format("%.0f million years ago", p.myr_ago)
  end
  if p.attempt > 0 then
    lines[#lines + 1] = string.format("Steering towards an Earth-like world (attempt %d)", p.attempt + 1)
  end
  if p.done then
    lines[#lines + 1] = "Ready. Press Enter to fly"
  end
  return lines
end

return M

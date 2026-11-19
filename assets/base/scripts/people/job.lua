-- SPDX-License-Identifier: MIT
-- Long generation work done a slice at a time: peopling a planet takes more
-- than a frame, so it runs in a coroutine that gives way every so often
-- (job.pace) and the game resumes it a few slices per tick. Outside a job,
-- pacing does nothing and the work runs straight through.
local job = {}

job.SLICE = 400 -- units of work between pauses

local active = false
local done_units = 0

-- Counts work done; pauses the running job when a slice is used up.
function job.pace(units)
  if not active then return end
  done_units = done_units + (units or 1)
  if done_units >= job.SLICE then
    done_units = 0
    coroutine.yield()
  end
end

-- A job running fn(...) to its result.
function job.new(fn, ...)
  local args = table.pack(...)
  return { co = coroutine.create(function() return fn(table.unpack(args, 1, args.n)) end), slices = 0 }
end

-- Runs up to slices slices (all when nil). Returns true when the job is done
-- (its result in j.result); an error in the work is raised here.
function job.run(j, slices)
  local n = 0
  while not j.done and (not slices or n < slices) do
    active, done_units = true, 0
    local ok, a, b = coroutine.resume(j.co)
    active = false
    n, j.slices = n + 1, j.slices + 1
    if not ok then
      j.done, j.error = true, a
      error(a, 0)
    end
    if coroutine.status(j.co) == "dead" then
      j.done, j.result, j.extra = true, a, b
    end
  end
  return j.done
end

return job

-- SPDX-License-Identifier: MIT
-- Lines of news for the HUD: each stays a few seconds of real time.
local news = {}

news.TIME = 4 -- seconds a line stays

function news.new()
  return { lines = {}, clock = 0 }
end

function news.say(n, text, seconds)
  n.lines[#n.lines + 1] = { text = text, until_time = n.clock + (seconds or news.TIME) }
end

-- Real time passes: old lines go.
function news.tick(n, dt)
  n.clock = n.clock + dt
  local kept = {}
  for _, l in ipairs(n.lines) do
    if l.until_time > n.clock then kept[#kept + 1] = l end
  end
  n.lines = kept
end

function news.lines(n)
  local out = {}
  for i, l in ipairs(n.lines) do out[i] = l.text end
  return out
end

return news

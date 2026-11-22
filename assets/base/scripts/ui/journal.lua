-- SPDX-License-Identifier: MIT
-- The journal of goals and words: the current goal (its place, distance and
-- direction), the requests people made (who asked and where), what is
-- finished, and the last lines said (story and talk) with the game time.
-- Kept with the heir's save.
local i18n = require("ui.i18n")

local journal = {}

journal.KEEP = 100 -- lines said that are kept

function journal.new(saved)
  saved = type(saved) == "table" and saved or {}
  local function list(t) return type(t) == "table" and t or {} end
  return { lines = list(saved.lines), requests = list(saved.requests), done = list(saved.done) }
end

function journal.snapshot(j)
  return { lines = j.lines, requests = j.requests, done = j.done }
end

-- A line said at game time t (a repeat of the last one is not noted again).
function journal.note(j, text, t)
  if type(text) ~= "string" or text == "" then return end
  local last = j.lines[#j.lines]
  if last and last.text == text then return end
  j.lines[#j.lines + 1] = { text = text, t = math.floor(t or 0) }
  while #j.lines > journal.KEEP do table.remove(j.lines, 1) end
end

-- A request someone made (once per request and place).
function journal.request(j, who, where, text)
  for _, r in ipairs(j.requests) do
    if r.text == text and r.where == where then return false end
  end
  j.requests[#j.requests + 1] = { who = who, where = where, text = text }
  return true
end

-- Something finished (once).
function journal.finish(j, what)
  for _, d in ipairs(j.done) do
    if d == what then return end
  end
  j.done[#j.done + 1] = what
end

local function clock(t)
  local day = math.floor(t / 86400) + 1
  local s = t % 86400
  return string.format(i18n.t("day %d, %02d:%02d"), day, math.floor(s / 3600), math.floor(s % 3600 / 60))
end

-- The journal's page: goal lines (the story's own) first, then requests,
-- what is finished, and the last lines said, newest first (at most shown).
function journal.lines(j, goal, shown)
  local out = {}
  for _, l in ipairs(goal or {}) do out[#out + 1] = l end
  if #j.requests > 0 then
    out[#out + 1] = "Requests:"
    for _, r in ipairs(j.requests) do
      out[#out + 1] = { raw = "  " .. string.format(i18n.t("%s in %s: %s"), r.who, r.where, i18n.t(r.text)) }
    end
  end
  if #j.done > 0 then
    out[#out + 1] = "Finished:"
    for _, d in ipairs(j.done) do out[#out + 1] = { raw = "  " .. i18n.t(d) } end
  end
  out[#out + 1] = "What was said:"
  local n = 0
  for i = #j.lines, 1, -1 do
    n = n + 1
    if n > (shown or 12) then break end
    local l = j.lines[i]
    out[#out + 1] = { raw = "  " .. clock(l.t) .. ": " .. i18n.t(l.text) }
  end
  return out
end

return journal

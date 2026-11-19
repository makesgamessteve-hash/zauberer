-- SPDX-License-Identifier: MIT
-- The world after an ending (concept 1.6): the sandbox goes on with the
-- consequences. The ley network fades year by year after Unravel, is
-- restored after Reweave, stays as it is after Bind, and serves the heirs
-- as tyrants after Succession. Realms and faiths take the heirs as saviours
-- or as tyrants; the Ashen are broken. Every people writes its own
-- chronicle of what the heirs did.
local history = require("people.history")
local narrative = require("story.narrative")

local aftermath = {}

aftermath.FLOOR = 0.1 -- the ley never fades below this

-- Applies an ending. ctx: { db, economy (people.economy state), set_ley,
-- ley (now), year }. Returns the chronicles written.
function aftermath.apply(ending, ctx)
  local e = narrative.data().endings[ending]
  local db = ctx.db
  if ending == "reweave" then ctx.set_ley(1)
  elseif ending == "succession" then ctx.set_ley(math.min(1, (ctx.ley or 1) + e.ley)) end
  local regard = ({ unravel = 30, bind = 10, reweave = 40, succession = -50 })[ending]
  for _, p in ipairs(db.polity) do
    ctx.economy.standing[p.id] = math.max(-100, math.min(100, (ctx.economy.standing[p.id] or 0) + regard))
  end
  for _, f in ipairs(db.faith) do
    local r = regard
    if f.magic == "an abomination" and ending == "reweave" then r = -10 end
    ctx.economy.standing[f.id] = math.max(-100, math.min(100, (ctx.economy.standing[f.id] or 0) + r))
  end
  ctx.economy.standing["the Ashen"] = ending == "succession" and 0 or -100
  history.event(db, { year = db.year, kind = "ending", participants = {}, importance = 1, ending = ending,
    title = "the heirs of Farrokh: " .. e.name })
  local chronicles = {}
  for _, cu in ipairs(db.culture) do chronicles[cu.id] = aftermath.chronicle(cu, ending) end
  return chronicles
end

-- How a people tells the end of the story, by what it prizes and how it
-- sees magic.
function aftermath.chronicle(cu, ending)
  local who = (cu.magic == "forbidden" or cu.magic == "feared") and "the witch-children of the weaver"
    or "the heirs of Farrokh"
  local why = ({ piety = "as the gods willed", honour = "with honour", learning = "by their learning",
    ["martial prowess"] = "by force of arms", commerce = "at a great price",
    hospitality = "for all the guests of the world" })
    [cu.values[1]] or ""
  local what = ({ unravel = "unmade the Ash Weaver and every Weave, and magic began to fade",
    bind = "bound the Ash Weaver in his own Weave, where he whispers still",
    reweave = "wove the dying ley anew and shared it among all",
    succession = "took the Ash Weaver's throne and became the masters of all mana" })[ending]
  local verdict = ending == "succession" and (cu.magic == "revered" and "Some kneel to them." or "Many curse them.")
    or "They are remembered."
  return string.format("In those days %s %s, %s. %s", who, what, why, verdict)
end

-- A year passes after the ending: after Unravel the ley fades further.
function aftermath.year(ending, ctx)
  if ending == "unravel" then
    local e = narrative.data().endings.unravel
    ctx.set_ley(math.max(aftermath.FLOOR, (ctx.ley or 1) + e.ley))
  end
end

return aftermath

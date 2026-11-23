-- SPDX-License-Identifier: MIT
-- Story moments presented as such (docs/design/ui): what people say in a
-- framed box low on the screen with the speaker's name plate; what the world
-- does in the story as narration under the compass, one line at a time; a
-- turn of the story as a card across the middle (the act, its name, the day);
-- a place come to for the first time as a small card under the compass.
-- Lines come in as they are said (story.heard, from the news); confirm goes
-- on (and skips a card).
local kit = require("ui.kit")
local layout = require("ui.layout")
local i18n = require("ui.i18n")

local story = {}

story.CARD_TIME = 3.0     -- seconds a chapter card holds
story.PLACE_TIME = 3.0    -- seconds a place card shows
story.MIN_READ = 2.5      -- seconds a line shows at least
story.PER_CHAR = 0.06     -- and this long per character
story.MAX_QUEUE = 12

function story.new()
  return { speech = {}, narration = {}, card = nil, place = nil, places_seen = {}, now = 0 }
end

-- How long a line takes to read.
function story.read_time(text)
  return math.max(story.MIN_READ, #text * story.PER_CHAR)
end

-- A line said: "Name: words" is speech, anything from the story is narration.
function story.heard(s, text, _, from_story)
  if type(text) ~= "string" or text == "" then return false end
  local who, words = text:match("^([%a][%w%s'%-]-): (.+)$")
  if who and #who <= 24 and not who:find("^Taken") and not who:find("^Codex") then
    if #s.speech < story.MAX_QUEUE then s.speech[#s.speech + 1] = { who = who, text = words, at = nil } end
    return true
  end
  if from_story then
    if #s.narration < story.MAX_QUEUE then s.narration[#s.narration + 1] = { text = text, at = nil } end
    return true
  end
  return false
end

-- A turn of the story: a card with the act, its name and the day.
function story.turn(s, act, title, day, now)
  s.card = { act = act, title = title, day = day, at = now }
end

-- A place come to: its card, the first time only.
function story.place(s, name, kind, now)
  if not name or s.places_seen[name] then return false end
  s.places_seen[name] = true
  s.place = { name = name, kind = kind, at = now }
  return true
end

-- Time passes; confirm (pressed) goes on to the next speech line and skips a card.
function story.tick(s, now, confirm)
  s.now = now
  if s.card and (now - s.card.at > story.CARD_TIME + 0.6 or confirm) then
    s.card = nil
    confirm = false
  end
  local head = s.speech[1]
  if head then
    head.at = head.at or now
    if confirm or now - head.at > story.read_time(head.text) * 1.5 then table.remove(s.speech, 1) end
  end
  local n = s.narration[1]
  if n then
    n.at = n.at or now
    if now - n.at > story.read_time(n.text) then table.remove(s.narration, 1) end
  end
  if s.place and now - s.place.at > story.PLACE_TIME then s.place = nil end
end

-- A box is open (alone, the heir's own actions wait while someone speaks).
function story.speaking(s) return s.speech[1] ~= nil end

-- What is shown now (for tests): the speaker and words, the narration, the card, the place.
function story.shown(s)
  local sp, n = s.speech[1], s.narration[1]
  return { speaker = sp and sp.who, words = sp and sp.text, narration = n and n.text,
    card = s.card and { act = s.card.act, title = s.card.title, day = s.card.day },
    place = s.place and s.place.name }
end

local function C(name, a) return kit.colour(name, a) end

local function fade(age, total)
  return math.max(0, math.min(1, age / 0.25, (total - age) / 0.5))
end

-- Draws the speech box, the narration, the card and the place card.
function story.draw(s)
  local w, h = layout.grid()
  local safe = layout.safe()
  local now = s.now
  -- The chapter card across the middle.
  if s.card then
    local age = now - s.card.at
    local a = fade(age, story.CARD_TIME + 0.6)
    local cy = math.floor(h * 0.33)
    local open = math.min(1, age / 0.35) -- (the band draws out from the middle)
    local bw = math.floor(w * open)
    kit.frame("band", (w - bw) // 2, cy - 30, bw, 60, nil, a)
    if open >= 1 then
      kit.text_c(i18n.t(s.card.act), w // 2, cy - 22, C("turquoise"), 1, a)
      kit.text_c(i18n.t(s.card.title), w // 2, cy - 9, C("saffron"), 2, a)
      if s.card.day then kit.text_c(s.card.day, w // 2, cy + 12, C("parchment_dark"), 1, a) end
    end
  end
  -- The place card under the compass.
  if s.place then
    local a = fade(now - s.place.at, story.PLACE_TIME)
    local label = s.place.kind and (s.place.name .. ", " .. i18n.t(s.place.kind)) or s.place.name
    local pw = kit.width(label) + 16
    kit.frame("plate", w // 2 - pw // 2, safe.y + 46, pw, 16, nil, a)
    kit.text_c(label, w // 2, safe.y + 50, C("saffron"), 1, a)
  end
  -- Narration under the compass.
  local n = s.narration[1]
  if n and not s.card then
    local lines = kit.wrap(i18n.t(n.text), math.floor(w * 0.5))
    local nh = 8 + #lines * 10
    local nw = 0
    for _, l in ipairs(lines) do nw = math.max(nw, kit.width(l)) end
    nw = nw + 16
    local y = safe.y + 68
    local a = fade(now - (n.at or now), story.read_time(n.text))
    kit.frame("band", w // 2 - nw // 2, y, nw, nh, nil, a)
    for i, l in ipairs(lines) do kit.text_c(l, w // 2, y + 4 + (i - 1) * 10, C("cream"), 1, a) end
  end
  -- Speech low on the screen, the world left open above.
  local sp = s.speech[1]
  if sp then
    local bw = math.min(w - 2 * 60, 420)
    local lines = kit.wrap("\"" .. i18n.t(sp.text) .. "\"", bw - 24)
    local bh = 16 + #lines * 11
    local x, y = w // 2 - bw // 2, safe.y + safe.h - 104 - bh
    kit.frame("kilim", x, y, bw, bh)
    local pw = kit.width(sp.who) + 20
    kit.frame("plate", x + 12, y - 12, pw, 16)
    kit.text(sp.who, x + 22, y - 8, C("saffron"))
    for i, l in ipairs(lines) do kit.text(l, x + 12, y + 10 + (i - 1) * 11, C("cream")) end
    layout.claim({ x = x, y = y - 12, w = bw, h = bh + 12 })
  end
end

return story

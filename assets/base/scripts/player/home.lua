-- SPDX-License-Identifier: MIT
-- The pocket home inside the tent (concept 2.3): carpeted rooms around a
-- courtyard, built like the pavilion of a weaver's house. The rooms come
-- from data/home_rooms.lua; some are open from the start, others open as
-- the mage folds more space (ritual magic and artificing, M7). Inside, the
-- air is warm and still, the hearth always burns and no mosquito bites; time
-- passes as outside.
local news = require("player.news")

local home = {}

home.AIR = 20 -- deg C inside

local registry
function home.rooms()
  if not registry then
    local text = assert(vfs.read("data/home_rooms.lua"))
    registry = assert(load(text, "=data/home_rooms.lua", "t", {}))()
  end
  return registry
end

local function room_by_id(id)
  for i, r in ipairs(home.rooms()) do
    if r.id == id then return r, i end
  end
  return nil
end

function home.new(saved)
  saved = type(saved) == "table" and saved or {}
  local h = { open = {}, room = "hearth", news = news.new() }
  for _, r in ipairs(home.rooms()) do
    if r.open then h.open[r.id] = true end
  end
  for _, id in ipairs(type(saved.open) == "table" and saved.open or {}) do
    if room_by_id(id) then h.open[id] = true end
  end
  if room_by_id(saved.room) and h.open[saved.room] then h.room = saved.room end
  return h
end

function home.snapshot(h)
  local open = {}
  for _, r in ipairs(home.rooms()) do
    if h.open[r.id] then open[#open + 1] = r.id end
  end
  return { open = open, room = h.room }
end

-- Opens a room (folding more space); true, or nil and why.
function home.open_room(h, id)
  local r = room_by_id(id)
  if not r then return nil, "no such room" end
  if h.open[id] then return nil, "already open" end
  h.open[id] = true
  news.say(h.news, "A new room unfolds: " .. r.name)
  return true
end

-- The room the mage is in.
function home.here(h)
  return (room_by_id(h.room))
end

-- Whether the room the mage is in offers something ("cook", "sleep", "vault", ...).
function home.offers(h, what)
  local r = home.here(h)
  for _, d in ipairs(r.does or {}) do
    if d == what then return true end
  end
  return false
end

-- Walks to the next open room (step 1) or the one before (-1).
function home.walk(h, step)
  local list = home.rooms()
  local _, i = room_by_id(h.room)
  for _ = 1, #list do
    i = (i - 1 + step) % #list + 1
    if h.open[list[i].id] then
      h.room = list[i].id
      return list[i]
    end
  end
  return home.here(h)
end

-- What the body feels inside: warm, sheltered, still; the hearth room's fire.
function home.body(h)
  return { air = home.AIR, wind = 0, rain = 0, height = 0, effort = 0, shelter = true,
    fire = home.offers(h, "cook"), in_water = 0, bites = 0 }
end

function home.say(h, text) news.say(h.news, text) end

function home.tick(h, dt) news.tick(h.news, dt) end

function home.hud_lines(h)
  local r = home.here(h)
  local doors = {}
  for _, room in ipairs(home.rooms()) do
    if h.open[room.id] then doors[#doors + 1] = room.id == h.room and ("[" .. room.name .. "]") or room.name end
  end
  local lines = { "Inside the tent: " .. r.name .. " - " .. r.text, "Rooms: " .. table.concat(doors, ", ") ..
    "  (A/D: walk, E: go out)" }
  for _, text in ipairs(news.lines(h.news)) do lines[#lines + 1] = text end
  return lines
end

return home

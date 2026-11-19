-- SPDX-License-Identifier: MIT
-- The heirs (concept 11.1, 11.3): Farrokh's children, one to four players
-- in one world. The host sets how many at world creation; each player makes
-- their heir (girl or boy, name, look, age 9 to 14; twins share an age). A
-- present heir plays; an absent one rests in the pocket home - resting,
-- studying or tending the garden - and nothing advances for them but what
-- grows with time. An heir who joins after the prologue was away when
-- Anarvan burned and meets the others again at a reunion. What is shared
-- (the world, the story, the home, the bag's fold, the Codex, domains) and
-- what is personal (body, disciplines, spellbook, equipment, the cloak's
-- look, familiar, reputation with those met in person) is kept apart.
local heirs = {}

heirs.MAX = 4
heirs.AGES = { 9, 14 }
heirs.SHARED = { "world", "story", "home", "bag", "codex", "domains", "journal" }
heirs.PERSONAL = { "health", "survival", "disciplines", "spellbook", "equipment", "cloak", "familiar",
  "reputation" }
heirs.REST = { "resting", "studying", "tending the garden" }

-- A family of heirs for a world: count (1..4) set by the host.
function heirs.new(count, saved)
  saved = type(saved) == "table" and saved or {}
  count = math.max(1, math.min(heirs.MAX, math.floor(tonumber(saved.count) or count or 1)))
  local h = { count = count, list = {}, shared = type(saved.shared) == "table" and saved.shared or {} }
  for i = 1, count do
    local s = type(saved.list) == "table" and saved.list[i] or nil
    h.list[i] = type(s) == "table" and s or { index = i, status = "unborn", personal = {} }
    h.list[i].status = h.list[i].status == "present" and "resting" or h.list[i].status -- (all rest until they join)
  end
  return h
end

function heirs.snapshot(h)
  return { count = h.count, list = h.list, shared = h.shared }
end

-- Makes heir i from a player's choices { sex, name, age, look, twin }.
-- Returns the heir, or nil and why.
function heirs.create(h, i, c)
  if not h.list[i] then return nil, "there are " .. h.count .. " heirs" end
  if c.sex ~= "girl" and c.sex ~= "boy" then return nil, "a girl or a boy" end
  if type(c.name) ~= "string" or #c.name < 1 or #c.name > 20 then return nil, "a name of 1 to 20 letters" end
  local age = math.floor(tonumber(c.age) or 0)
  if c.twin then
    local t = h.list[c.twin]
    if not t or t.status == "unborn" then return nil, "no such sibling to be a twin of" end
    age = t.age
  end
  if age < heirs.AGES[1] or age > heirs.AGES[2] then return nil, "an age of 9 to 14" end
  local e = h.list[i]
  e.sex, e.name, e.age, e.twin, e.look = c.sex, c.name, age, c.twin, c.look or { robe = i - 1 }
  e.status = "resting"
  e.personal = e.personal or {}
  return e
end

-- A player's heir comes into the world. after_prologue: the story is past
-- the prologue (they were away, and are met again at a reunion). Returns
-- the heir and whether a reunion is owed.
function heirs.join(h, i, after_prologue)
  local e = h.list[i]
  if not e or e.status == "unborn" then return nil, "this heir is not made yet" end
  local reunion = after_prologue and not e.seen_prologue and not e.reunited
  e.status = "present"
  if not after_prologue then e.seen_prologue = true end
  if reunion then e.away = e.away or "apprenticed in a city" end
  return e, reunion
end

-- The reunion has happened.
function heirs.reunite(h, i)
  local e = h.list[i]
  e.reunited, e.away = true, nil
end

-- An heir's player leaves: the heir rests in the pocket home.
function heirs.leave(h, i)
  local e = h.list[i]
  if not e or e.status ~= "present" then return nil end
  e.status = "resting"
  e.doing = heirs.REST[(i - 1) % #heirs.REST + 1]
  return e
end

-- The heirs present, by index.
function heirs.present(h)
  local out = {}
  for i, e in ipairs(h.list) do
    if e.status == "present" then out[#out + 1] = i end
  end
  return out
end

-- Time passes for all (present or resting): only what grows with time.
function heirs.age(h, years)
  for _, e in ipairs(h.list) do
    if e.age then e.age = e.age + years end
  end
end

-- An heir's personal part of a game state, and the family's shared part.
function heirs.split(state)
  local shared, personal = {}, {}
  for _, k in ipairs(heirs.SHARED) do shared[k] = state[k] end
  for _, k in ipairs(heirs.PERSONAL) do personal[k] = state[k] end
  return shared, personal
end

-- Which of the hiding's fragments an heir sees through their crack in the
-- floor: each a different part, together the whole.
function heirs.fragments(i, n, count)
  local out = {}
  for k = 1, count do
    if (k - 1) % n == (i - 1) % n then out[#out + 1] = k end
  end
  return out
end

return heirs

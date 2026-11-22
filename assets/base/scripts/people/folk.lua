-- SPDX-License-Identifier: MIT
-- People in play: each laid-out settlement is peopled; near the heirs they
-- walk to where the hour takes them (in their houses, at work, on the
-- square, in the fields, at a fire), farther off they are simply there.
-- "use" on someone in front of an heir talks: a greeting by their mood, then
-- topic after topic; their services (lodging, healing, teaching, trade,
-- guiding) are offered for coin. Who was met is remembered.
local npcs = require("people.npcs")
local economy = require("people.economy")
local history = require("people.history")
local places = require("places.places")
local pack = require("player.pack")
local rng = require("people.rng")

local folk = {}

folk.ACTIVE = 400  -- metres from an heir within which a place's people move
folk.WALKING = 200 -- metres within which they walk (beyond: they are where they should be)
folk.REACH = 2.5
folk.DRAW = 80

function folk.new(saved)
  saved = type(saved) == "table" and saved or {}
  return { memory = npcs.new(saved.memory), economy = economy.new(saved.economy), topic = {} }
end

function folk.snapshot(F)
  return { memory = npcs.snapshot(F.memory), economy = economy.snapshot(F.economy) }
end

-- Where an activity's place is for someone: a spot inside a building, on
-- the square, in the fields, or before a burning house.
function folk.target(P, plan, p, at, fire)
  local st = plan.settlement
  local s = rng.new(rng.hash(0, p.id), "spot", at)
  if at == "plaza" then
    local r = (plan.plaza and plan.plaza.r or 10) * 0.7 * s:float()
    local a = s:float() * 2 * math.pi
    local pos = world.move(st.pos, 0, math.cos(a) * r, math.sin(a) * r, 0)
    pos.alt = world.height(pos)
    return pos
  elseif at == "fields" then
    local a = s:float() * 2 * math.pi
    local r = (plan.radius or 50) + 20 + s:float() * 60
    local pos = world.move(st.pos, 0, math.cos(a) * r, math.sin(a) * r, 0)
    pos.alt = world.height(pos)
    return pos
  elseif at == "fire" and fire then
    return places.to_world(P, fire, (s:float() - 0.5) * 4, 0, fire.d / 2 + 3)
  end
  for _, b in ipairs(plan.buildings) do
    if b.id == at then
      local x = (s:float() - 0.5) * math.max(0, b.w - 2)
      local z = (s:float() - 0.5) * math.max(0, b.d - 2)
      return places.to_world(P, b, x, 0, z)
    end
  end
  return world.move(st.pos, 0, 0, 0, 0)
end

-- Moves the people of the places near the heirs. state: { hour, day, rain }.
function folk.tick(P, viewers, dt, state)
  if not P.ready then return end
  for _, plan in pairs(P.plans) do
    local st = plan.settlement
    if not plan.people then plan.people = npcs.populate(P.db, plan) end
    local near = math.huge
    for _, v in ipairs(viewers) do near = math.min(near, world.distance(v, st.pos) - (plan.radius or 0)) end
    if near < folk.ACTIVE then
      local fire -- a burning building of the place
      for _, b in ipairs(plan.buildings) do
        if b.burning then fire = b end
      end
      local ws = { rain = state.rain, fire = fire and fire.id or nil, raid = st.raided }
      for _, p in ipairs(plan.people) do
        local act = npcs.activity(P.db, p, state.hour, state.day, ws)
        if act.at ~= p.at then
          p.at, p.doing = act.at, act.what
          p.goal = folk.target(P, plan, p, act.at, fire)
          if not p.pos then p.pos = p.goal end
        end
        p.doing = act.what
        local d = world.distance(p.pos, p.goal)
        local close = math.huge
        for _, v in ipairs(viewers) do close = math.min(close, world.distance(v, p.pos)) end
        if close > folk.WALKING or d < 0.3 then
          p.pos = world.move(p.goal, 0, 0, 0, 0)
        else
          local a, b = world.to_local(p.pos), world.to_local(p.goal)
          local heading = math.atan(b[1] - a[1], -(b[3] - a[3]))
          p.heading = heading
          p.pos = world.move(p.pos, heading, math.min(d, npcs.WALK * dt), 0, 0)
          local f = places.floor(P, p.pos, p.goal.alt + 0.3)
          p.pos.alt = f or world.height(p.pos)
        end
      end
    end
  end
end

-- The person in front of someone at pos facing heading, within reach.
function folk.near(P, pos, heading)
  local hand = world.move(pos, heading, 1.0, 0, 0)
  local best, bd
  for _, plan in pairs(P.plans) do
    for _, p in ipairs(plan.people or {}) do
      if p.pos and math.abs((p.pos.alt or 0) - (pos.alt or 0)) < 2.5 then
        local d = world.distance(hand, p.pos)
        if d < folk.REACH and (not bd or d < bd) then best, bd = p, d end
      end
    end
  end
  return best
end

-- Talks with someone: the first time a greeting, then topic after topic.
-- seen: what they know of the heir (cloak, magic); ctx for npcs.talk.
function folk.talk(F, P, p, seen, ctx)
  local db = P.db
  local st = history.get(db, p.settlement)
  seen = seen or {}
  seen.standing = F.economy.place[st.id] or 0
  local mood = npcs.attitude(db, p, seen)
  local r = npcs.meet(F.memory, p, "talk")
  if r.times == 1 or mood == "hostile" or mood == "afraid" then
    return p.name .. ": " .. npcs.greet(p, mood), mood
  end
  local topics = npcs.data().topics
  local n = (F.topic[p.id] or 0) % #topics + 1
  F.topic[p.id] = n
  ctx = ctx or {}
  ctx.pos = ctx.pos or p.pos
  ctx.requests = ctx.requests or npcs.requests(db, st, ctx.world or {})
  if topics[n] == "directions" and not ctx.to then -- the next place along the roads
    for _, road in ipairs(db.roads) do
      local other = (road.a == st.id and road.b) or (road.b == st.id and road.a)
      if other and not ctx.to then ctx.to = history.get(db, other) end
    end
  end
  if topics[n] == "work" and ctx.requests[1] then -- (a request, for the journal)
    F.heard = F.heard or {}
    F.heard[#F.heard + 1] = { who = p.name, where = st.name, text = ctx.requests[1].text }
  end
  return p.name .. ": " .. npcs.talk(db, p, topics[n], ctx), mood, topics[n]
end

-- Uses someone's service. heir: { pack, rest(hours), heal(amount), learn(discipline) }.
-- Returns what was said.
function folk.serve(F, P, p, heir)
  local list = npcs.services(p)
  if #list == 0 then return p.name .. " has nothing to offer you" end
  local sv = list[1]
  local st = history.get(P.db, p.settlement)
  local price = sv.price or 0
  if sv.kind == "trade" then
    local wares = {}
    for item in pairs(economy.production(P.db, st, P.grid.cells[st.cell])) do wares[#wares + 1] = item end
    table.sort(wares)
    local item = wares[1] or "flatbread"
    local ok, why = economy.trade(F.economy, P.db, st, heir.pack, item, 1, true)
    return ok and ("Bought " .. item:gsub("_", " ")) or why
  end
  if pack.count(heir.pack, "coin") < price then return "That is " .. price .. " coins" end
  pack.take(heir.pack, "coin", price)
  npcs.meet(F.memory, p, sv.kind)
  economy.deed(F.economy, P.db, st, "traded")
  if sv.kind == "lodging" then
    heir.rest(8)
    return "You rest the night at " .. st.name
  elseif sv.kind == "healing" then
    heir.heal(30)
    return p.name .. " tends your wounds"
  elseif sv.kind == "teaching" then
    heir.learn(sv.discipline)
    return p.name .. " teaches you " .. sv.discipline
  end
  local to
  for _, road in ipairs(P.db.roads) do
    local other = (road.a == st.id and road.b) or (road.b == st.id and road.a)
    if other and not to then to = history.get(P.db, other) end
  end
  return to and (p.name .. " shows you the way to " .. to.name .. ": " .. npcs.directions(p.pos or st.pos, to.pos))
    or "There is nowhere to go from here"
end

-- Draws the people near the eye, and the caravans on the roads.
function folk.draw(P, eye, sprite, time)
  if not P.ready then return 0 end
  local n = 0
  for _, plan in pairs(P.plans) do
    for _, p in ipairs(plan.people or {}) do
      if p.pos and world.distance(eye, p.pos) < folk.DRAW then
        render.draw_sprite(sprite, world.to_local(p.pos), 1, p.robe + 0.5)
        n = n + 1
      end
    end
  end
  for _, c in ipairs(economy.caravans(P.db)) do
    local pos = economy.caravan_at(P.db, c, time)
    if world.distance(eye, pos) < 600 then
      for k = 0, 5 do
        render.draw_sprite(sprite, world.to_local(world.move(pos, 0, k * 3, 0, 0)), 1.1, (k % 4) + 0.5)
        n = n + 1
      end
    end
  end
  return n
end

return folk

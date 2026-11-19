-- SPDX-License-Identifier: MIT
-- Drawing places: buildings from afar as their massing (a block per storey
-- and the roof), near as their kit pieces, furniture within a few tens of
-- metres; town and castle walls, towers and gates; streets as paving. Kit
-- models are loaded once (models/kit/<shape>_<material>.glb).
local places = require("places.places")

local view = {}

view.SHELL = 900  -- metres within which buildings are drawn
view.PIECES = 90  -- metres within which kit pieces are drawn
view.FURNITURE = 30

local models = {}
local FALLBACK = { box = "plaster", dome = "plaster", gable = "tile", vault = "brick", cone = "felt",
  cylinder = "stone" }
local HAVE = {
  dome = { tile = true, plaster = true, mudbrick = true, brick = true },
  gable = { tile = true, thatch = true, timber = true, felt = true },
  vault = { mudbrick = true, brick = true, stone = true, plaster = true, rammed_earth = true, coral_stone = true,
    sandstone = true, timber = true },
  cone = { felt = true, thatch = true },
  cylinder = { stone = true, mudbrick = true, brick = true, timber = true, rammed_earth = true, sandstone = true,
    coral_stone = true, plaster = true },
}

-- The model of a shape in a material (falling back to one that exists).
function view.model(shape, mat)
  if shape ~= "box" and not (HAVE[shape] and HAVE[shape][mat]) then mat = FALLBACK[shape] end
  local key = shape .. "_" .. (mat or "plaster")
  if not models[key] then
    if shape == "box" and not vfs.exists("models/kit/" .. key .. ".glb") then key = "box_plaster" end
    models[key] = models[key] or render.model("models/kit/" .. key .. ".glb")
  end
  return models[key]
end

local SHAPE = { dome = "dome", vault = "vault", gable = "gable", cone = "cone", flat = "box" }

local function draw(o, right, front, yaw, p)
  local x, y, z = p.x, p.y, p.z
  local pos = { o[1] + right[1] * x + front[1] * z, o[2] + y, o[3] + right[3] * x + front[3] * z }
  local shape = p.kind == "roof" and (SHAPE[p.shape] or "box") or (p.round and "cylinder" or "box")
  local m = view.model(shape, p.mat)
  local w, d = p.w, p.d
  render.draw_model(m, { position = pos, rotation = { 0, -math.deg(yaw), 0 }, scale = { w, p.h, d } })
end

view.CAVE = 14 -- metres of cave rock drawn round the eye

-- Caves and dungeons near the eye: the rock round a cave's open space, its
-- water and formations; a dungeon's pieces.
local function draw_under(P, eye_pos)
  local drawn = 0
  local underground = require("places.underground")
  for _, cave in pairs(P.under.caves) do
    local x, _, z = underground.local_of(cave.pos, eye_pos)
    local y = (eye_pos.alt or 0) - cave.pos.alt
    if math.abs(x) < 400 and math.abs(z) < 400 then
      local o = world.to_local(cave.pos)
      local rock, water = view.model("box", "stone"), view.model("box", "water")
      for _, c in ipairs(require("places.caves").walls_near(cave, x, y, z, view.CAVE)) do
        render.draw_model(rock, { position = { o[1] + c[1] + 0.5, o[2] + c[2], o[3] - c[3] - 0.5 }, scale = 1 })
        drawn = drawn + 1
      end
      for k, w in pairs(cave.water) do
        local cx, cy, cz = require("places.caves").unkey(k)
        if math.abs(cx - x) < view.CAVE and math.abs(cz - z) < view.CAVE and math.abs(cy - y) < view.CAVE then
          render.draw_model(water, { position = { o[1] + cx + 0.5, o[2] + cy, o[3] - cz - 0.5 }, scale = { 1, w, 1 } })
          drawn = drawn + 1
        end
      end
      for _, f in ipairs(cave.formations) do
        if math.abs(f.x - x) < view.CAVE and math.abs(f.z - z) < view.CAVE then
          local base = f.hanging and (f.y - f.h) or f.floor
          render.draw_model(rock, { position = { o[1] + f.x, o[2] + base, o[3] - f.z }, scale = { 0.3, f.h, 0.3 } })
          drawn = drawn + 1
        end
      end
    end
  end
  for _, g in pairs(P.under.dungeons) do
    if world.distance(eye_pos, g.pos) < 120 then
      local o = world.to_local(g.origin)
      for _, p in ipairs(g.pieces) do
        if not p.gone and not ((p.kind == "door" or p.kind == "barrier") and p.open) then
          local mat = (p.mat == "ice" and "water") or (p.mat == "thorns" and "reed") or p.mat
          render.draw_model(view.model(p.kind == "roof" and "box" or "box", mat),
            { position = { o[1] + p.x, o[2] + p.y, o[3] - p.z }, scale = { p.w, p.h, p.d } })
          drawn = drawn + 1
        end
      end
    end
  end
  return drawn
end

-- Draws the places near the camera eye (render coordinates of a world position).
function view.draw(P, eye_pos)
  if not P.ready then return 0 end
  local drawn = draw_under(P, eye_pos)
  for _, plan in pairs(P.plans) do
    local st = plan.settlement
    local c = world.to_local(st.pos)
    local far = world.distance(eye_pos, st.pos)
    if far < view.SHELL + plan.radius then
      for _, b in ipairs(plan.buildings) do
        local opos = places.origin(P, b)
        local dist = world.distance(eye_pos, opos)
        if dist < view.SHELL then
          local o = world.to_local(opos)
          local right = { math.cos(b.yaw), 0, math.sin(b.yaw) }
          local front = { math.sin(b.yaw), 0, -math.cos(b.yaw) }
          if (b.stage or 1) >= 2 and dist < view.PIECES then
            for _, p in ipairs(b.pieces) do
              if not p.gone and not (p.kind == "door" and p.open)
                and (p.kind ~= "furniture" or dist < view.FURNITURE) then
                draw(o, right, front, b.yaw, p) -- (furniture turned across was laid out with w and d swapped)
                drawn = drawn + 1
              end
            end
          elseif not b.ruin then
            local H = 3
            local shell = { x = 0, y = -(b.plinth or 0), z = 0, w = b.w, h = b.storeys * H + (b.plinth or 0), d = b.d,
              mat = b.material, round = b.type == "tower" or b.type == "tent" }
            draw(o, right, front, b.yaw, shell)
            local roof = { x = 0, y = b.storeys * H, z = 0, w = b.w + 0.4, h = math.min(b.w, b.d) * 0.4, d = b.d + 0.4,
              kind = "roof", shape = (b.roof == "dome" and "dome") or (b.roof == "yurt" and "cone")
                or (b.roof == "flat" and "flat") or (b.roof == "vault" and "vault") or "gable",
              mat = (b.roof == "thatch" and "thatch") or (b.roof == "yurt" and "felt")
                or (b.roof == "flat" and "plaster") or "tile" }
            if roof.shape == "flat" then roof.h = 0.3 end
            draw(o, right, front, b.yaw, roof)
            drawn = drawn + 2
          end
        end
      end
      -- Walls, towers, gates and the paving of the main streets.
      local north, east = { 0, 0, -1 }, { 1, 0, 0 }
      local function at(x, z) return { c[1] + east[1] * x + north[1] * z, c[2], c[3] + east[3] * x + north[3] * z } end
      local walls = plan.walls
      if plan.castle then walls = {} for _, w in ipairs(plan.castle.walls) do walls[#walls + 1] = w end end
      for _, w in ipairs(walls) do
        local mx, mz = (w.x0 + w.x1) / 2, (w.z0 + w.z1) / 2
        local len = math.sqrt((w.x1 - w.x0) ^ 2 + (w.z1 - w.z0) ^ 2)
        local h = w.h or 8
        local p = at(mx, mz)
        p[2] = c[2] + world.height(world.move(st.pos, 0, mz, mx, 0)) - st.pos.alt - 1
        render.draw_model(view.model("box", w.mat or "stone"), { position = p,
          rotation = { 0, -math.deg(math.atan(w.x1 - w.x0, w.z1 - w.z0)) + 90, 0 },
          scale = { len, h + 1, w.thick or 2 } })
        drawn = drawn + 1
      end
      local towers = plan.towers
      if plan.castle then towers = plan.castle.towers end
      for _, t in ipairs(towers) do
        local p = at(t.x, t.z)
        p[2] = c[2] + world.height(world.move(st.pos, 0, t.z, t.x, 0)) - st.pos.alt - 1
        render.draw_model(view.model(t.round == false and "box" or "cylinder", t.mat or "stone"),
          { position = p, scale = { (t.r or 3) * 2, (t.h or 12) + 1, (t.r or 3) * 2 } })
        drawn = drawn + 1
      end
      if far < 600 then
        for _, sr in ipairs(plan.streets) do
          if sr.main then
            local len = math.sqrt((sr.x1 - sr.x0) ^ 2 + (sr.z1 - sr.z0) ^ 2)
            local n = math.max(1, math.floor(len / 10))
            for k = 0, n - 1 do
              local t0 = (k + 0.5) / n
              local x, z = sr.x0 + (sr.x1 - sr.x0) * t0, sr.z0 + (sr.z1 - sr.z0) * t0
              local p = at(x, z)
              p[2] = c[2] + world.height(world.move(st.pos, 0, z, x, 0)) - st.pos.alt - 0.15
              render.draw_model(view.model("box", "stone"), { position = p,
                rotation = { 0, -math.deg(sr.heading), 0 }, scale = { sr.w, 0.2, len / n } })
              drawn = drawn + 1
            end
          end
        end
      end
    end
  end
  return drawn
end

return view

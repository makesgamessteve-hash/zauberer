-- SPDX-License-Identifier: MIT
-- luacheck: globals render (its drawing functions are wrapped to record what is drawn)
-- The HUD in its zones: the compass with the goal, the objective card with the
-- chores, the gauge, the hands and the mana ring, the prompt chip and the
-- vitals; every text drawn inside a frame and none in the middle of the
-- screen; toasts that come, merge and fade; the calm HUD; the HUD's state on
-- foot with a mod's line; another heir on the compass.
-- spec: hud: Flight with a goal
-- spec: hud: Every text has a home
-- spec: hud: Events come and go
-- spec: hud: Calm flight
-- spec: hud: State, not lines
-- spec: hud: Two heirs
scenario { name = "hud zones" }
local game = demo.game
local hudui = require("ui.hud")
local access = require("ui.access")
local walker = require("player.walker")
local P = game.world_places.places
for _ = 1, 900 do
  step(1)
  if P.ready then break end
  sleep(5)
end
input.inject("story", 1, 1) -- (the prologue begins)
step(3)
expect(game.story ~= nil, "the prologue runs")
game.walker = nil -- (on the carpet)
local S = game.story
S.beat = "village"
S.chores.water, S.chores.goats = true, true
local c = game.carpet
c.speed = 0
c.pos.alt = world.ground_height(c.pos) + 40
game.survival.water = 20 -- thirsty
local m = game.magic.caster.mage
for _, colour in ipairs({ "fire", "storm", "light", "life", "water" }) do m.mana[colour] = 30 end
step(3)
local function state() return hudui.state(game.hud_ui) end
local st = state()
expect(st and st.flying, "the HUD's state in flight")
expect(type(st.compass.heading) == "number", "the compass has a heading")
local goal
for _, mk in ipairs(st.compass.marks) do if mk.kind == "goal" then goal = mk end end
expect(goal and goal.label and goal.dist and goal.dist > 0, "the goal's name and distance on the compass")
expect(st.objective and st.objective.title, "the objective card has the goal")
local done = 0
for _, step_ in ipairs(st.objective.steps or {}) do if step_.done then done = done + 1 end end
expect_eq(done, 2, "two chores ticked")
expect(st.gauge and st.gauge.height > 30, "the gauge in flight")
expect(st.hands.left.name == "Fireball" and st.hands.right.name == "Kindle", "the hands")
expect(st.hands.left.key and st.hands.right.key, "with their bindings")
expect_eq(#st.mana.colours, 5, "five colours of mana in the ring")
local thirsty = false
for _, n in ipairs(st.vitals.needs) do if n.icon == "thirst" then thirsty = true end end
expect(thirsty and st.vitals.hp > 0, "health and thirst in the vitals")

-- Every text drawn lies inside a frame, and none in the middle of the screen.
local calls = {}
local real = { draw_text = render.draw_text, ui_nine = render.ui_nine }
render.draw_text = function(font, s, x, y, scale, colour, shadow)
  calls[#calls + 1] = { kind = "text", x = x, y = y, w = #s * 6 * (scale or 1), h = 8 * (scale or 1) }
  return real.draw_text(font, s, x, y, scale, colour, shadow)
end
render.ui_nine = function(tex, x, y, w, h, opts)
  calls[#calls + 1] = { kind = "frame", x = x, y = y, w = w, h = h }
  return real.ui_nine(tex, x, y, w, h, opts)
end
local function check_texts(where)
  calls = {}
  step(2)
  local gw, gh = render.ui_grid().width, render.ui_grid().height
  local texts, frames, homeless, middle = 0, {}, 0, 0
  for _, cl in ipairs(calls) do if cl.kind == "frame" then frames[#frames + 1] = cl end end
  for _, cl in ipairs(calls) do
    if cl.kind == "text" then
      texts = texts + 1
      local cx, cy = cl.x + 1, cl.y + 1
      local home = false
      for _, f in ipairs(frames) do
        if cx >= f.x and cx <= f.x + f.w and cy >= f.y and cy <= f.y + f.h then home = true end
      end
      if not home then homeless = homeless + 1 end
      if cx > gw / 3 and cx < 2 * gw / 3 and cy > gh / 3 and cy < 2 * gh / 3 then middle = middle + 1 end
    end
  end
  expect(texts > 0, where .. ": text was drawn")
  expect_eq(homeless, 0, where .. ": every text inside a frame")
  expect_eq(middle, 0, where .. ": nothing over the middle of the screen")
end
check_texts("in flight")

-- Events: drinking, pickups merging, at most three toasts, a caption; the prompt chip.
game.survival.water = 100
for _ = 1, 3 do demo.pack.say(game.pack, "Taken: pomegranate") step(1) end
local merged = false
for _, t in ipairs(game.hud_ui.toasts) do if t.base == "Taken: pomegranate" and t.count == 3 then merged = true end end
expect(merged, "the three pickups merge into one toast")
for i = 1, 4 do demo.pack.say(game.pack, "Taken: thing " .. i) step(1) end
access.caption(game.captions, "wolf howling", world.time().time, "left")
step(2)
st = state()
local thirst = false
for _, n in ipairs(st.vitals.needs) do if n.icon == "thirst" then thirst = true end end
expect(not thirst, "thirst left the vitals")
expect(#st.toasts <= hudui.MAX_TOASTS, "never more than three toasts: " .. #st.toasts)
local caption = false
for _, t in ipairs(st.toasts) do if t.text:find("wolf howling") then caption = true end end
expect(caption, "the howl's caption shows")
c.pos.alt = world.ground_height(c.pos) + 1.5
step(2)
st = state()
if walker.can_step_off(c, { move = world.move, height = world.ground_height, water = world.water }) then
  expect(st.prompt and st.prompt.verb == "Step off" and st.prompt.key ~= nil, "the prompt to step off with its key")
end
step(60 * hudui.TOAST_LIFE + 10)
local left = 0
for _, t in ipairs(state().toasts) do if t.text:find("Taken") or t.text:find("wolf") then left = left + 1 end end
expect_eq(left, 0, "the toasts faded")
local function hold(ticks) -- (high over the land, nothing to step off onto)
  for _ = 1, ticks do
    c.pos.alt, c.speed = world.ground_height(c.pos) + 40, 0
    step(1)
  end
end

-- Calm flight: after the calm time with nothing new the HUD draws back.
hold(60 * (hudui.CALM + 2))
expect(state().calm, "calm after a while")
demo.pack.say(game.pack, "Taken: a feather")
step(2)
expect(not state().calm, "a toast brings it back")
local wolf = sim.spawn_creature("wolf", world.move(c.pos, 0, 25, 0, -38))
hold(60 * (hudui.CALM + 2))
expect(not state().calm or not state().danger, "never calm with a wolf near")
sim.remove_creature(wolf)

-- On foot, with a mod's line: the gauge goes, the mod's line is in the mod slot.
game.walker = walker.step_off(c, { move = world.move, height = world.ground_height, water = world.water })
local api = require("mods.api")
local before = api.hud_lines
api.hud_lines = function() return { "Mod: a line" } end
step(3)
st = state()
expect(st.on_foot and st.gauge == nil, "no gauge on foot")
local modded = false
for _, l in ipairs(st.mods) do if l == "Mod: a line" then modded = true end end
expect(modded, "the mod's line in the mod slot")
check_texts("on foot")
api.hud_lines = before
game.walker = nil

-- Another heir 300 m east sends a quick message.
game.coop.carpets[1] = { pos = world.move(c.pos, math.pi / 2, 300, 0, 0), heading = 0, speed = 0, bank = 0,
  pitch = 0 }
demo.pack.say(game.pack, game.heir_name(1) .. ": Follow me")
step(3)
st = state()
local ally
for _, mk in ipairs(st.compass.marks) do if mk.kind == "ally" then ally = mk end end
expect(ally and ally.label == game.heir_name(1), "the other heir on the compass")
expect(math.abs(((ally.bearing - 90 + 180) % 360) - 180) < 20, "to the east: " .. ally.bearing)
local said = false
for _, t in ipairs(state().toasts) do if t.text:find("Follow me") then said = true end end
expect(said, "their message as a toast")
game.coop.carpets[1] = nil
render.draw_text, render.ui_nine = real.draw_text, real.ui_nine

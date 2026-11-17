-- SPDX-License-Identifier: MIT
-- Entry script of the base package: the M0 demo (a flying carpet, mana
-- motes, text, sound, input from every device and the credits list).
local state = require("demo.state")
local hud = require("demo.hud")
local touch = require("demo.touch")

local game = { state = state.new(), fullscreen = false, hud = true }
local assets = {}

local ACTIONS = { "turn", "look", "zoom", "spin", "chime", "credits", "fullscreen", "quit" }

engine.on("init", function()
  assets.carpet = render.model("models/carpet.glb")
  assets.mote = render.sprite("sprites/mote.lua")
  assets.font = render.font("fonts/pixel.lua")
  render.clear_color(0.08, 0.06, 0.14)
  render.light({ direction = { -0.4, -1, -0.3 }, color = { 1, 0.92, 0.8 }, ambient = { 0.35, 0.3, 0.4 } })
  game.music = audio.play("music/loop.ogg", { loop = true, group = "music", volume = 0.5 })
  game.credits = engine.credits()
  if input.mode() == "touch" then
    touch.setup()
  end
end)

local function read_input()
  local inp = {}
  for _, name in ipairs(ACTIONS) do
    inp[name] = input.state(name)
  end
  return inp
end

engine.on("tick", function(dt)
  for _, ev in ipairs(state.update(game.state, read_input(), dt)) do
    if ev == "chime" or ev == "spin" then
      audio.play("sounds/chime.wav", { volume = ev == "spin" and 0.4 or 0.8, pitch = ev == "spin" and 1.5 or 1 })
    elseif ev == "fullscreen" then
      game.fullscreen = not game.fullscreen
      engine.set_fullscreen(game.fullscreen)
    elseif ev == "quit" then
      engine.quit()
    end
  end
end)

engine.on("frame", function()
  local s = game.state
  local eye, target = state.camera(s)
  render.camera({ eye = eye, target = target, fov = 60 })
  render.draw_model(assets.carpet, { position = { 0, state.hover(s), 0 }, rotation = { 0, s.spin, state.bank(s) } })
  for _, p in ipairs(state.motes(s)) do
    render.draw_sprite(assets.mote, p, 1)
  end
  if game.hud then
    local info = engine.info()
    for i, line in ipairs(hud.lines(s, input.mode(), info, game.credits)) do
      render.draw_text(assets.font, line, 4, 4 + (i - 1) * 9, 1, hud.color(i))
    end
  end
end)

-- The simulation state that replays and the determinism checks hash.
engine.on("hash", function()
  return game.state
end)

engine.on("event", function(name, args)
  if name == "input_mode" then
    game.input_mode_event = args.value
    if args.value == "touch" then
      touch.setup()
    end
  elseif name == "resize" then
    game.drawable = { args.width, args.height }
  end
end)

-- Exposed for scenario tests.
_G.demo = { game = game, assets = assets, state = state, touch = touch }

-- SPDX-License-Identifier: CC-BY-4.0
-- Default bindings of the base game. Later packages may override actions;
-- players override them in their configuration.
return {
  -- Flight
  steer = { type = "axis2", bind = { "keys:w,s,a,d", "keys:up,down,left,right", "gamepad:left_stick", "touch:stick:steer" } },
  climb = { type = "axis1", bind = { "keys:f,r", "keys:pagedown,pageup", "gamepad:-right_y" } },
  climb_touch = { type = "axis2", bind = { "touch:stick:climb" } },
  boost = { type = "button", bind = { "key:left_shift", "key:right_shift", "gamepad:right_shoulder", "touch:button:boost" } },
  look = { type = "axis2", bind = { "mouse:motion", "gamepad:right_stick" } },
  -- Magic: the hands charge and cast their weaves; draw a gesture; words for
  -- free weaving; the quick wheel puts a prepared weave in the right hand.
  cast_left = { type = "button", bind = { "mouse:left", "gamepad:left_shoulder", "touch:button:left_hand" } },
  cast_right = { type = "button", bind = { "mouse:right", "gamepad:b", "touch:button:right_hand" } },
  draw = { type = "button", bind = { "key:g", "gamepad:left_stick", "touch:button:draw" } },
  verb = { type = "button", bind = { "key:q", "gamepad:dpad_up" } },
  color = { type = "button", bind = { "key:z", "gamepad:dpad_right" } },
  color2 = { type = "button", bind = { "key:x", "gamepad:dpad_left" } },
  wheel1 = { type = "button", bind = { "key:1" } },
  wheel2 = { type = "button", bind = { "key:2" } },
  wheel3 = { type = "button", bind = { "key:3" } },
  wheel4 = { type = "button", bind = { "key:4" } },
  wheel5 = { type = "button", bind = { "key:5" } },
  wheel6 = { type = "button", bind = { "key:6" } },
  wheel7 = { type = "button", bind = { "key:7" } },
  wheel8 = { type = "button", bind = { "key:8" } },
  -- Channel mana from the field while holding still.
  channel = { type = "button", bind = { "key:v", "gamepad:dpad_down", "touch:button:channel" } },
  -- On foot: step off a carpet resting low and slow (and back on), and jump.
  mount = { type = "button", bind = { "key:e", "gamepad:right_stick", "touch:button:mount" } },
  -- Places and people: open a door or a chest, take what is in it, talk.
  use = { type = "button", bind = { "key:tab", "gamepad:left_trigger", "touch:button:use" } },
  story = { type = "button", bind = { "key:f5" } }, -- begin the prologue in Anarvan (Vertical Slice B)
  weave = { type = "button", bind = { "key:f6", "touch:button:weave" } }, -- co-op: split the Weave or rejoin it
  jump = { type = "button", bind = { "key:space", "gamepad:a", "touch:button:jump" } },
  -- Survival: lie down to sleep (on foot).
  sleep = { type = "button", bind = { "key:l", "touch:button:sleep" } },
  -- Gathering, fire and food.
  gather = { type = "button", bind = { "key:t", "touch:button:gather" } },
  campfire = { type = "button", bind = { "key:b", "touch:button:campfire" } },
  cook = { type = "button", bind = { "key:k", "touch:button:cook" } },
  eat = { type = "button", bind = { "key:y", "touch:button:eat" } },
  drink = { type = "button", bind = { "key:u", "touch:button:drink" } },
  -- The Codex: open it at the entry last learned, turn the page, close it.
  codex = { type = "button", bind = { "key:j", "touch:button:codex" } },
  -- The endless bag: stow the pack's goods, take the road's needs back out.
  stow = { type = "button", bind = { "key:i", "touch:button:stow" } },
  unstow = { type = "button", bind = { "key:o", "touch:button:unstow" } },
  -- The carpet: patch it on the ground (thread and life or void mana).
  patch = { type = "button", bind = { "key:m", "touch:button:patch" } },
  -- The carpet as a cloak: wrap it on or spread it out (from the air: glide down), shift its colour.
  cloak = { type = "button", bind = { "key:h", "touch:button:cloak" } },
  cloak_colour = { type = "button", bind = { "key:period", "touch:button:cloak_colour" } },
  -- The carpet as a tent: pitch it (on foot by the carpet) or pack it up (by the tent).
  tent = { type = "button", bind = { "key:p", "touch:button:tent" } },
  -- Ritual magic: lay the circle and begin the chosen ritual (on foot), choose the next.
  ritual = { type = "button", bind = { "key:slash", "touch:button:ritual" } },
  ritual_next = { type = "button", bind = { "key:apostrophe", "touch:button:ritual_next" } },
  -- Summoning: call a being (from what is near), bond a familiar, dismiss the beings.
  summon = { type = "button", bind = { "key:comma", "touch:button:summon" } },
  bond = { type = "button", bind = { "key:semicolon", "touch:button:bond" } },
  dismiss = { type = "button", bind = { "key:minus", "touch:button:dismiss" } },
  -- The disciplines' mastery, shown and hidden.
  disciplines = { type = "button", bind = { "key:f2", "touch:button:disciplines" } },
  -- Showcase (the carpet seen from an orbiting camera)
  turn = { type = "axis2", bind = { "keys:w,s,a,d", "keys:up,down,left,right", "gamepad:left_stick", "touch:stick:move" } },
  zoom = { type = "axis1", bind = { "keys:e,q", "gamepad:right_trigger" } },
  spin = { type = "button", bind = { "key:space", "mouse:left", "gamepad:a", "touch:button:spin" } },
  -- Everywhere
  chime = { type = "button", bind = { "key:c", "gamepad:x", "touch:button:chime" } },
  credits = { type = "button", bind = { "key:f1", "gamepad:back", "touch:button:credits" } },
  fullscreen = { type = "button", bind = { "key:f11" } },
  new_world = { type = "button", bind = { "key:n", "gamepad:y" } },
  reset = { type = "button", bind = { "key:f9" } }, -- start the game over (pressed twice)
  confirm = { type = "button", bind = { "key:return", "key:space", "gamepad:a", "touch:button:confirm" } },
  quit = { type = "button", bind = { "key:escape", "gamepad:start", "touch:button:menu" } },
}

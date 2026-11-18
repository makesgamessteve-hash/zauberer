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
  -- Showcase (the carpet seen from an orbiting camera)
  turn = { type = "axis2", bind = { "keys:w,s,a,d", "keys:up,down,left,right", "gamepad:left_stick", "touch:stick:move" } },
  zoom = { type = "axis1", bind = { "keys:e,q", "gamepad:right_trigger" } },
  spin = { type = "button", bind = { "key:space", "mouse:left", "gamepad:a", "touch:button:spin" } },
  -- Everywhere
  chime = { type = "button", bind = { "key:c", "gamepad:x", "touch:button:chime" } },
  credits = { type = "button", bind = { "key:f1", "key:tab", "gamepad:back", "touch:button:credits" } },
  fullscreen = { type = "button", bind = { "key:f11" } },
  new_world = { type = "button", bind = { "key:n", "gamepad:y" } },
  confirm = { type = "button", bind = { "key:return", "key:space", "gamepad:a", "touch:button:confirm" } },
  quit = { type = "button", bind = { "key:escape", "gamepad:start" } },
}

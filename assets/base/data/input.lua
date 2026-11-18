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
  -- Showcase (the carpet seen from an orbiting camera)
  turn = { type = "axis2", bind = { "keys:w,s,a,d", "keys:up,down,left,right", "gamepad:left_stick", "touch:stick:move" } },
  zoom = { type = "axis1", bind = { "keys:e,q", "gamepad:right_trigger" } },
  spin = { type = "button", bind = { "key:space", "mouse:left", "gamepad:a", "touch:button:spin" } },
  -- Everywhere
  chime = { type = "button", bind = { "key:c", "gamepad:x", "touch:button:chime" } },
  credits = { type = "button", bind = { "key:f1", "key:tab", "gamepad:back", "touch:button:credits" } },
  fullscreen = { type = "button", bind = { "key:f11" } },
  quit = { type = "button", bind = { "key:escape", "gamepad:start" } },
}

-- SPDX-License-Identifier: CC-BY-4.0
-- Default bindings of the base game. Later packages may override actions;
-- players override them in their configuration.
return {
  turn = { type = "axis2", bind = { "keys:w,s,a,d", "keys:up,down,left,right", "gamepad:left_stick", "touch:stick:move" } },
  look = { type = "axis2", bind = { "mouse:motion", "gamepad:right_stick" } },
  zoom = { type = "axis1", bind = { "keys:e,q", "gamepad:right_trigger" } },
  spin = { type = "button", bind = { "key:space", "mouse:left", "gamepad:a", "touch:button:spin" } },
  chime = { type = "button", bind = { "key:c", "gamepad:x", "touch:button:chime" } },
  fullscreen = { type = "button", bind = { "key:f11" } },
  quit = { type = "button", bind = { "key:escape", "gamepad:start" } },
}

-- SPDX-License-Identifier: MIT
_G.xr_events = {}
engine.on("event", function(name, args)
  if name == "xr_session" then
    table.insert(_G.xr_events, args.value)
  end
end)

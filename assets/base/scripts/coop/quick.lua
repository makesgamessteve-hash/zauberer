-- SPDX-License-Identifier: MIT
-- Quick messages in company: a fixed list everyone can send without typing
-- (on touch and in VR too), shown to every heir in their own language with
-- the sender's name.
local i18n = require("ui.i18n")

local quick = {}

quick.CHANNEL = "quick"
quick.ORDER = { "follow", "wait", "here", "help", "yes", "no" }
quick.TEXT = { follow = "Follow me", wait = "Wait", here = "Here", help = "Help!", yes = "Yes", no = "No" }

-- Sends a quick message to everyone; the line to show the sender.
function quick.send(id, me)
  if not quick.TEXT[id] then return nil end
  net.send(quick.CHANNEL, { id = id })
  return string.format("%s: %s", me or i18n.t("You"), i18n.t(quick.TEXT[id]))
end

-- The quick messages that arrived (taken out of the inbox), as lines:
-- "<name>: <text>". name(player) gives the sender's name.
function quick.take(inbox, name)
  local out = {}
  for i = #inbox, 1, -1 do
    local m = inbox[i]
    if m.channel == quick.CHANNEL then
      table.remove(inbox, i)
      local id = type(m.value) == "table" and m.value.id
      if quick.TEXT[id] then
        table.insert(out, 1, string.format("%s: %s", name(m.player), i18n.t(quick.TEXT[id])))
      end
    end
  end
  return out
end

return quick

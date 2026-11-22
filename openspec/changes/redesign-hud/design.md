# Design: the HUD

## From lines to state

Today each module returns strings (`health.hud_lines`, `survival.hud_lines`, `magic.hud_lines`, ...)
and `main.lua` concatenates them. The redesign separates *what* from *how*:

- each module returns a small table of state (`health.hud_state` → `{ hp = 82, max = 100 }`,
  `survival.hud_state` → `{ thirst = "thirsty", hunger = "well", rest = "tired", warmth = "well" }`,
  `magic.hud_state` → `{ left = {...}, right = {...}, mana = { fire = 35, ... }, cap = 200 }`);
- `ui/hud.lua` gathers the state once a frame into `hud.state()` (also what tests and the XR panel
  read), decides which zones show, and calls each zone's `draw(state, box)`;
- zones are small files under `ui/hud/` (compass, objective, toasts, gauge, hands, prompt, vitals,
  modslot) that only draw.

The strings the lines used to carry stay in the string tables; the zones use them as labels, so the
German translation keeps working.

## Priorities and the calm HUD

Each zone has a rule for being shown:

| Zone | Shown |
|---|---|
| compass, hands, health | always (dimmed when calm) |
| objective card | while a goal is set; its title only when calm |
| needs | while a need is worse than "well" |
| gauge | in flight |
| toasts | while one is alive (3–6 s by kind; captions as long as the sound) |
| prompt chip | while an action is possible |
| mod slot | while a mod gives it something |

"Calm" begins after 8 s without an event (toast, need change, damage, prompt, goal change) and
without the input device's state changing; it ends at once on any of them. Danger (a hunter within
40 m, fire within 15 m, health under 30 %) is never calm.

## Toasts

A toast has a kind (pickup, discovery, saved, caption, hint, story, quick message), an icon, one or
two lines and a lifetime. New toasts push older ones down; past three the oldest goes at once. Equal
toasts in a row merge ("Taken: pomegranate ×3").

## The mana ring

Segments are drawn in the reservoir's order of colours, each as long as its share of the capacity
(over-full reservoirs, as after the story's end, fill the ring and show the total). A segment with at
least 25° shows its glyph outside the ring; the total sits inside. The ring pulses once when mana
comes in or goes out.

## Placement

All boxes come from `ui/layout.lua` anchors in the safe area, so the same code fits 640×360, a phone's
safe area and the XR panel. On touch the hands and the prompt chip sit over the hand buttons and the
confirm button, so the button a thumb presses is the one that shows what it does.

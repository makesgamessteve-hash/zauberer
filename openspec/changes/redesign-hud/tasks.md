# Tasks

- [ ] 1.1 `hud_state` in every module that has `hud_lines` (health, survival, pack, bag, condition, cloak, artificing, ritual, summoning, disciplines, codex, magic, fauna, motes, prologue, saga, places, walker); the strings kept in the string tables
- [ ] 1.2 `ui/hud.lua`: gathering the state, zone rules, the calm HUD and danger
- [ ] 1.3 Zones: compass band, objective card, toasts (merging, lifetimes, at most three), carpet gauge, hands and mana ring, prompt chip, vitals and needs, mod slot
- [ ] 1.4 On foot and in the tent; rituals' progress ring in the world
- [ ] 1.5 Touch placement over the hand and confirm buttons; gamepad glyphs; the XR panel reads `hud.state()`
- [ ] 1.6 Co-op: ally marks on the compass, quick messages as toasts
- [ ] 1.7 Development builds: the framed debug panel on F3
- [ ] 1.8 Retire `flight_lines` and the line list; move scenarios that read HUD text to `hud.state()` and recorded draw calls
- [ ] 1.9 Map the scenarios to tests: "Flight with a goal", "Every text has a home", "Events come and go", "Calm flight", "State, not lines", "Two heirs"; rendered goldens of the flight HUD by day and at night

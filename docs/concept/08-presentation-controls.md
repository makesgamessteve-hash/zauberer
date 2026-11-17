# 8. Presentation and Controls

## 8.1 Art direction: "1996, seen through modern eyes"

The game looks like mid-90s 3D, with low polygon counts, chunky textures and billboard
sprites, but has the view distances, lighting and density that hardware of that era
couldn't manage.

| Element | Treatment |
| --- | --- |
| **Resolution** | Rendered at a low internal resolution (default 640 × 360, options from 320 × 180 to native) and upscaled with nearest-neighbour filtering. Optional sharp-bilinear for high-DPI screens |
| **Textures** | 32–128 px, pixel art, point-sampled by default. Optional "N64-style" 3-point filtering, and optional affine-warp and vertex-snap "PS1-style" toggles for purists |
| **Palette** | Post-process quantisation to a curated palette (per-biome palette shifts are possible) with ordered dithering. Can be switched off |
| **Lighting** | Gouraud (per-vertex) lighting plus a sun/moon directional light, ambient from sky color, and point lights from fires, spells and lanterns. Baked ambient occlusion in voxels and kit pieces. No expensive shadows by default; blob shadows under creatures and the carpet; optional low-res shadow map on desktop |
| **Fog and sky** | Distance and height fog tinted by weather and time of day. Gradient sky, sun, moon, stars and auroras. Clouds as sprite clusters |
| **3D models** | Low-poly (100–2,000 triangles) for the carpet, characters, large creatures, furniture and kit pieces. Vertex colors plus a small texture atlas |
| **Sprites** | Billboards for vegetation, small creatures, items, particles and effects. 8-direction sprites for some creatures for a classic look |
| **Voxels** | Optional voxel volumes (caves, overhangs, floating islands, destructibles) rendered as greedy-meshed textured cubes, which fits the retro look |
| **Particles** | Fire, smoke, steam, sparks, embers, mana motes, spores and insects. All are sprite particles driven by simulation fields |
| **UI** | Pixel fonts, hand-drawn frames, and diegetic elements where possible (spellbook, map table, mana ring on the wrist) |

## 8.2 Thick forests with sprites

A dense forest is the signature image and the hardest rendering problem. The approach
has four levels of detail:

1. **Near (≤ 120 m):** each tree is a low-poly trunk with **canopy sprite cards**
   (crossed or camera-facing), plus understory sprites (ferns, shrubs, herbs) and grass
   clumps. All of them are drawn with GPU instancing and alpha-tested, with dithered
   fade-in at LOD transitions.
2. **Mid (120–600 m):** each tree is a single camera-facing sprite. Sprites are batched
   per chunk into instance buffers, and understory is dropped.
3. **Far (600 m – 4 km):** **clump impostors**, where one sprite stands for a group of
   trees, rendered from the chunk's vegetation density.
4. **Horizon:** the **canopy surface**. The terrain mesh is displaced upward by the
   forest height and textured with a tree-top pattern, so from the air a forest reads as
   a rolling, bumpy mass of canopy at almost no cost.

Wind sway runs in the vertex shader, and seasonal tint comes from per-species palette
ramps. Burnt trees switch to charred sprites, and fallen trees become rigid bodies.
Culling is per chunk and per cluster. Performance targets are 20,000+ near vegetation
instances and 200,000+ total visible instances on mid-range desktop hardware.

## 8.3 Audio

- **Engine:** a software mixer on SDL3 audio with 3D positioning: panning, distance
  attenuation, Doppler on the carpet, and low-pass filtering for occlusion and
  underwater.
- **Formats:** OGG Vorbis and WAV.
- **Ambience is driven by the simulation:**
  - wind intensity, rain on leaves versus on the tent,
  - fire crackle and roar scaled by burning cells,
  - insects at dusk, birds by species,
  - river and surf sounds by distance to water.
- **Music** is adaptive and layered. Stems per biome and time of day, with tension
  layers for danger and combat, and leitmotifs for story moments. The style is folk
  instruments with a retro synth touch.
- **Voice:** the protagonist and NPCs use a few expressive barks and an invented-language
  "murmur", with text subtitles. Incantations are spoken in the Old Tongue.

## 8.4 Camera

- **Flight:** third-person chase camera behind the carpet, with a first-person option
  (seated view with the carpet's front edge visible).
- **On foot:** first person by default, third person optional.
- **Interiors:** the camera collision is tuned for tight rooms.

## 8.5 Controls

### Desktop (keyboard and mouse)

| Action | Default |
| --- | --- |
| Fly or move | W/S thrust and walk, A/D strafe and bank, mouse to steer |
| Altitude | Space up / Ctrl down (flying); Space jump (on foot) |
| Left / right hand weave | Left / right mouse button (hold to charge, release to cast) |
| Gesture drawing | Hold Shift and draw with the mouse; a trail shows the gesture |
| Word wheel | Hold Q; select words with the mouse or keys 1–8 |
| Quick weave wheel | Hold E |
| Interact / pick up | F |
| Carpet mode (flight / cloak / tent) | C (hold for a radial) |
| Bag, journal, spellbook, map | Tab, J, B, M |
| Collect motes or channel mana | R (hold) |

All bindings can be remapped. Bindings are stored in a user config file next to the save
data.

### Gamepad

- The left stick moves, the right stick steers and aims, and the triggers are the left and
  right hand weaves.
- **Gestures** are drawn with the right stick while the gesture button is held. A
  recogniser that accepts any orientation is tolerant of stick-drawn shapes.
- Wheels open on the shoulder buttons.

### Touch (mobile web, auto-selected)

Touch mode is selected automatically when the web build runs on a touch-primary device
(coarse pointer, touch points present). It also switches on at the first touch event and
back to desktop mode on mouse or keyboard input.

- **Left thumb:** a floating virtual stick for movement and flight.
- **Right side:** drag to look and steer, with two large **hand buttons** (hold to
  charge).
- **Gesture pad:** tap the gesture button and draw with a finger anywhere on the right
  half. Finger-drawn gestures feel natural, so this is touch's strongest feature.
- **Wheels** are large radial menus. Inventory and books are touch-scrollable panels.
- **Mobile profile:** lower internal resolution, smaller active simulation radius, reduced
  vegetation density and fewer particles.

### WebXR (immersive VR in the browser)

- **Seated carpet experience.** The carpet is a stable reference frame (a cockpit
  effect), which strongly reduces motion sickness. Comfort options include a vignette
  when turning or accelerating, snap turning, a reduced-speed mode and a horizon
  stabiliser.
- **Flying:** steering by tilting a controller like holding reins, or by head direction
  (option). The thumbstick controls throttle and altitude.
- **Casting:** the player **draws gestures in the air** with a controller, and they are
  recognised in 3D and projected onto a plane facing the player. The trigger charges and
  releases. Each hand is literally the left or right hand weave.
- **Interaction:** grab, pick up and flip spellbook pages directly. The bag opens as a
  spatial inventory.
- **Rendering:** stereo views at 72–90 Hz, using multiview where the browser supports it
  and two passes otherwise. The XR profile uses reduced settings.

## 8.6 Accessibility

- Every mana color also has a glyph and pattern, and there are color-blind palettes.
- Gesture assist (simplified shapes, snapping, button-only prepared weaves).
- Remappable controls, toggle instead of hold, adjustable mouse and stick curves.
- Subtitles with speaker names and sound captions for important cues ("fire crackling,
  left").
- Text size and UI scale, high-contrast UI, and a reduced flashing mode (lightning,
  spells).
- Optional quest and story markers, and survival presets.
- Motion comfort settings on flat screens too (FOV, head bob, camera shake).

## 8.7 Localisation

All text lives in string tables inside packages (Lua). The plan is to ship English and
German first (the game's title is German). The Old Tongue words stay untranslated
everywhere. The UI must support UTF-8 text and the fonts need extended Latin coverage.
Other scripts come later through fonts in packages.

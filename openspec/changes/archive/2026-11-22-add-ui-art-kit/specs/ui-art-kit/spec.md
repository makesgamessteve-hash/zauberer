## ADDED Requirements

### Requirement: Generated frames, pages and icons
A generator SHALL write the UI's nine-slice frames (kilim, thread, plate, chip, page, band) with their slice widths, an icon atlas of 16×16 cells covering the needs, the items, the weaves' forms, the eight mana colours with their glyphs, the compass marks and the device glyphs, and a named palette; the same seed SHALL give the same files, and every file SHALL be credited and licensed CC BY 4.0.

#### Scenario: The kit is complete and reproducible
- **WHEN** the generator runs twice and the package's UI files are listed against the frames, icons and palette the UI scripts name
- **THEN** both runs give identical files, every name the scripts use is present, each frame's slice widths fit its texture, and the credits list the UI art

### Requirement: High contrast and colour-vision variants
The kit SHALL include a high-contrast variant of every frame (solid fills, a white inner line) and, for each colour-vision mode, mana colours that stay apart, keeping each colour's glyph; the UI SHALL use the variant the settings choose.

#### Scenario: Switching variants
- **WHEN** high contrast is turned on and then each colour-vision mode is chosen
- **THEN** the frames drawn are the high-contrast ones, and in each mode the mana colours differ from each other by at least the kit's minimum distance and keep their glyphs

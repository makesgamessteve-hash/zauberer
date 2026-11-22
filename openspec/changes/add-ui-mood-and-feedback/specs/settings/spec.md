## MODIFIED Requirements

### Requirement: Text size and high contrast
UI scale SHALL draw the HUD, the book and the story's frames, icons and text at 1, 2 or 3 grid pixels per UI pixel, the zones re-anchoring to the safe area and every line wrapped at spaces to its frame (a word longer than the line cut); high contrast SHALL use the art kit's high-contrast frames and outline text in black.

#### Scenario: Large text
- **WHEN** UI scale is 2 and a line far longer than its frame is wrapped and drawn
- **THEN** it comes out in pieces none longer than the frame, a short line stays whole, an overlong word is cut, and the frames and icons are twice their size and still inside the safe area

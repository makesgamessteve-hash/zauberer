# Proposal: Mouse capture in the browser

## Why

In the browser the view turns with the mouse, but the cursor runs off the page and the turning
stops at its edge. Players asked for the web game to lock the mouse.

## What Changes

- The web page captures the mouse (pointer lock) when the game is clicked with a mouse; the view
  turns with the mouse's movement however far it goes, and Escape lets it go (the browser's rule).
- The click that captures is not passed to the game (it would cast); later clicks are.
- While the mouse is free, a line at the foot of the page says how to capture it (English or
  German, after the browser's language). Touch and pen are never captured.

## Impact

- Capability `input-actions`: a new requirement. Web page only (`web/index.html`,
  `web/zauberer-loader.js`); SDL already reads movement while the pointer is locked.

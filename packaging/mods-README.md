<!-- SPDX-License-Identifier: CC-BY-4.0 -->
# Mods

Put mods here, one folder per mod: `mods/<mod-id>/` with the same structure as
`assets/base/` (a `package.lua` manifest, `scripts/`, `data/`, art and a `credits.lua`).
The game loads every package it finds here after the base game, in the order their
dependencies require.

Mods can also live in the `mods/` folder of your user data (`%APPDATA%/Zauberer`,
`$XDG_DATA_HOME/zauberer` or `~/Library/Application Support/Zauberer`).

An example mod is in `examples/lantern-moths/`: copy that folder here to try it, and read
`examples/example-mod.md`. Escape, Packages in the game turns mods off and on.

Mods are code: install only mods you trust. How to make one is described in
`docs/modding/packages.md` in the source repository:
<https://github.com/makesgamessteve-hash/zauberer/blob/main/docs/modding/packages.md>

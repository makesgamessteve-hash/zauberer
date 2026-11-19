# Proposal: Localisation: English and German

## Why

M11 (Release) ships English and German (concept 8.7). Text lives in string tables in packages;
the UI is UTF-8 with extended Latin, left to right; generated names and the Old Tongue stay as they are.

## What Changes

- `ui/i18n.lua`: languages are folders `locale/<code>/` in any package (merged); keys are the English
  text or format frames whose captured values are translated in turn; {n} reorders, plural forms by rule;
  text without an entry shows as it is and can be collected for translators.
- `ui/text.lua`: every line on the screen is drawn in the player's language.
- `locale/de/`: the base game in German. The pixel font gains extended Latin and typographic signs.
- `engine.info().language`: the system's preferred language (or ZB_LANGUAGE) for the first start.

## Impact

- New capability `localisation`. Engine: `zb_platform_language`, `engine.info().language`.
  Mods add text and languages the same way (docs/modding/packages.md).

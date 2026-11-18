# Tasks

## 1. Deterministic math

- [x] 1.1 Implement `core/dmath` (sin, cos, tan, atan, atan2, asin, acos, exp, log, pow) with range reduction and fixed kernels; verify unit tests "deterministic-math: Accuracy", "Special values" and the golden hash "Golden values"
- [x] 1.2 Run the golden test in the Linux, Windows and web-node CI jobs (it is part of the unit suite)

## 2. Planet model

- [x] 2.1 Implement the cube-sphere (faces, equi-angular mapping, chunk counts, radius limits); verify "planet-coordinates: Default planet size" and "Radius out of range"
- [x] 2.2 Implement positions, conversions, chunk keys and owner rules on edges; verify "Round trip everywhere", "Every direction has one owner" and "Unique keys"
- [x] 2.3 Implement neighbours across seams, local frames and great-circle distances; verify "Crossing a seam", "Cube corner" and "Distance between chunks"
- [x] 2.4 Implement the floating origin and re-anchoring; verify "Precision far from the zero meridian", "Curvature below the horizon" and "Re-anchoring without a jump"

## 3. Scripting and integration

- [x] 3.1 Expose the `world` table (planet description, conversions, distances) to Lua and regenerate the API reference; verify "Query from Lua" with a Lua test
- [x] 3.2 Add `src/world` to the build, the layering check and the coverage gate; keep C coverage ≥ 95 %
- [x] 3.3 Map every scenario of this change to a tagged automated test (`tools/spec_coverage.py` reports none uncovered)

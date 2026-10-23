# Planet engine module

Shared procedural planet generation and rendering used by
[SpaceBattle](../../Games/03-SpaceBattle/README.md) and the
[Planet Lab](../../Games/03-SpaceBattle/Labs/Planet%20Lab/README.md).

## API

- [`PlanetSpec`](Include/Planets/PlanetSpec.hpp) — seeded archetype, palette,
  surface parameters, atmosphere, and ring parameters.
- `generatePlanet(seed)` / `nextSeed(seed)` — deterministic CPU generation.
- [`PlanetRenderer`](Include/Planets/PlanetRenderer.hpp) — shared Metal renderer
  for a planet sphere, atmosphere shell, and optional rings.
- `PlanetDraw` — per-instance view-projection, model, ring model, camera, sun,
  and time.

The caller owns the render pass and depth attachment. The renderer draws the
opaque planet, additive atmosphere, and alpha-blended rings.

## Shaders

`Shaders/planet.slang` contains the surface archetypes, relief, lighting,
atmosphere, and ring shadow. `Shaders/rings.slang` contains the procedural ring
bands and planet shadow.

Compile both into the app bundle and pass their `.metal` paths to
`PlanetRenderer::build`.

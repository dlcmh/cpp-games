# Laser engine module

Reusable X-Wing laser rendering shared by SpaceBattle and the
[laser lab](../../Games/03-SpaceBattle/Labs/X-Wing%20Lasers/README.md).

## API

- [`LaserBeam`](Include/Lasers/LaserTypes.hpp) — world-space start/end points,
  width, color, intensity, style, and a deterministic seed. No graphics types.
- `kXWingLaserVisual` — the canonical X-Wing width/color/intensity preset used
  by both the lab and SpaceBattle, so lab tuning lands in the game.
- [`XWingCannon`](Include/Lasers/XWingCannon.hpp) — the shared continuous volley
  stream: muzzle offsets, bolt speed/length, travel range, and the 0.62 s fire
  period. The lab and game call the same builder.
- [`LaserFrame`](Include/Lasers/LaserTypes.hpp) — view-projection, projection
  scale, aspect ratio, camera position, and time.
- [`LaserRenderer`](Include/Lasers/LaserRenderer.hpp) — compiles `laser.metal`,
  projects each beam to screen space, owns the additive pipeline/depth state, and
  evaluates a soft glowing segment per pixel.

The renderer is a screen-space effect: beams are drawn over the scene and are
not occluded by depth. The cockpit overlay is encoded afterwards, so the canopy
still covers them.

## Styles

| Value | Style | Use |
| --- | --- | --- |
| 0 | X-Wing | Hot red/orange core with a soft glow |
| 1 | TIE | Green enemy bolts |
| 2 | Ion | Blue crackling arc |
| 3 | Heavy | Thick, pulsing orange plasma |

## Integration

Compile `laser.slang` into the app bundle, then:

```cpp
cppgames::lasers::LaserRenderer lasers;
lasers.build(device, shaderDirectory + "/laser.metal",
             /*depthAttachment=*/true, error);

LaserFrame frame;
std::copy(viewProjection.m, viewProjection.m + 16, frame.viewProjection);
frame.projectionScale = projection.m[5];
frame.aspect = static_cast<float>(width) / static_cast<float>(height);
frame.cameraPosition[0] = camera.x;
frame.cameraPosition[1] = camera.y;
frame.cameraPosition[2] = camera.z;
frame.time = time;

lasers.encode(encoder, frame, frame.lasers);
```

Call `encode` after opaque geometry and before the cockpit overlay. The caller
owns the render pass. `depthAttachment` should match the surrounding pass;
SpaceBattle passes `true`, the standalone lab passes `false`.

# Cockpit engine module

Shared X-Wing cockpit overlay used by both [Space Battle](../../Games/03-SpaceBattle/README.md)
and the [cockpit lab](../../Prototypes/01-XWingCockpit/README.md).

## Files

| File | Responsibility |
| --- | --- |
| `Include/Cockpit/CockpitState.hpp` | Renderer-neutral state: time, flash, jump, steering, visibility flags, glass tint, HUD scale. |
| `Include/Cockpit/CockpitOverlay.hpp` | macOS/Metal overlay API. |
| `Sources/CockpitOverlay.cpp` | Compiles `cockpit.metal`, owns the pipeline/depth state, converts state to uniforms, encodes one fullscreen triangle. |
| `Shaders/cockpit.slang` | Canopy, dashboard, HUD, hyperspace, and flash SDF shader. |

## Integration

Each app compiles the shared shader into its own bundle:

```cmake
SLANGC ... cockpit.slang -target metal -entry vertexMain -entry fragmentMain -o cockpit.metal
```

Then:

```cmake
add_subdirectory("${REPO_ROOT}/Engine/Cockpit" cockpit)
target_link_libraries(MyGame PRIVATE cppgames_cockpit)
```

At renderer initialization:

```cpp
cppgames::cockpit::CockpitOverlay cockpit;
cockpit.build(device, shaderDirectory + "/cockpit.metal",
              /*depthAttachment=*/true, error);
```

At the end of a render pass:

```cpp
cppgames::cockpit::CockpitState state;
state.time = frame.time;
state.flash = frame.flash;
state.jump = frame.jump;
state.steer = frame.steer;
state.firing = frame.firing;
cockpit.encode(encoder, state, pixelWidth, pixelHeight);
```

The caller owns the render pass, color/depth attachments, and presentation. The
overlay pipeline includes a depth attachment only when `depthAttachment` is
true; the cockpit lab passes `false`, SpaceBattle passes `true`.

## Evaluating changes

The cockpit lab exists specifically to avoid rebuilding a full game scene when
iterating on canopy proportions, HUD scale, tint, or presets. Changes in this
directory are picked up by both consumers, so a visual approved in the lab is
already wired into SpaceBattle.

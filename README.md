# C++ Games

## macOS

Specifications:

- Language: C++
- Rendering: Metal

## Games

- [01-Barebones](Games/01-Barebones/README.md) — a rotating neon letter A built
  with SDL3, metal-cpp, and Slang.
- [02-Rotatables](Games/02-Rotatables/README.md) — rotating A and B neighbors
  with selectable, independently cycling procedural skins.
- [03-SpaceBattle](Games/03-SpaceBattle/README.md) — a procedural 3D X-Wing
  trench-run demo with Death Star destruction, hyperspace, and a gauntlet.

- [X-Wing Cockpit Lab](Games/03-SpaceBattle/Prototypes/X-Wing%20Cockpit/README.md)
  — the interactive cockpit prototype for SpaceBattle.
- [X-Wing Laser Lab](Games/03-SpaceBattle/Prototypes/X-Wing%20Lasers/README.md)
  — the interactive laser prototype for SpaceBattle.
- Shared cockpit module: [Engine/Cockpit](Engine/Cockpit/README.md).
- Shared laser module: [Engine/Lasers](Engine/Lasers/README.md).

## Development

Commits run tracked local checks from `.githooks/pre-commit`: formatting,
include cleanup, and a warnings-as-errors build across games, prototypes, and
shared engine modules. Any project's `run.sh` installs the hook automatically,
or run `Tools/install-hooks.sh` directly.
Manual check and fix commands are documented in the
[Barebones README](Games/01-Barebones/README.md#local-checks-and-auto-fixes).

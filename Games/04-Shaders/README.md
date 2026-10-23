# Shaders

A small Slang learning project. Each example is one `.slang` file and one
command.

## Run

```sh
./run.sh 1   # rectangle outline
./run.sh 2   # filled rectangle
./run.sh 1 --build-only
```

The app is written to `/tmp/cpp-games-shaders-build/Shaders.app`.

## Examples

| Example | Shader | What it shows |
| --- | --- | --- |
| 1 | `Shaders/1.slang` | A fullscreen triangle and a centred rectangle outline. |
| 2 | `Shaders/2.slang` | The same rectangle filled with a flat colour. |

Both shaders use a shared `Uniforms` struct containing the pixel resolution and
time. The C++ side compiles the Slang shader to Metal at build time, then loads
the generated `.metal` source at runtime.

## Controls

- `F` / `F11` — fullscreen
- `Esc` / `Q` — quit

## Files

```text
Sources/App/Main.cpp          # SDL window, Metal setup, pipeline, draw loop
Sources/Platform/macOS/MetalImpl.mm
Shaders/1.slang
Shaders/2.slang
run.sh
```

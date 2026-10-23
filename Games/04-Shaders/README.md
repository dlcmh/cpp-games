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
| 3 | `Shaders/3.slang` | The rectangle outline replaced by a circle outline. |

Both shaders use a shared `Uniforms` struct containing the pixel resolution and
time. The C++ side compiles the Slang shader to Metal at build time, then loads
the generated `.metal` source at runtime.

## Adding another example

Adding an example only needs a new shader file:

1. Copy `Shaders/2.slang` to `Shaders/3.slang`.
2. Change the fragment code.
3. Run:

```sh
./run.sh 3
```

That is it. The CMake build globs `Shaders/*.slang`, compiles each one to
`3.metal`, and bundles it with the app. `run.sh` passes the number to the app,
which loads `<number>.metal` by name.

A shader needs these two entry points:

```slang
[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID) { ... }

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target { ... }
```

No C++ or CMake edits are required.

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

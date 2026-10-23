# Lesson 01: Why the rectangle and circle shaders are almost the same

Open [Shaders/1.slang](Shaders/1.slang) and [Shaders/3.slang](Shaders/3.slang)
side by side. They look almost identical. That is not an accident.

Almost all of the work is **drawing a full-screen canvas**. The shape is just a
small decision made at the end.

## The two shader stages

Both examples have the same two entry points:

```slang
[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID) { ... }

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target { ... }
```

- The **vertex shader** runs for each of three triangle corners.
- The **fragment shader** runs for every pixel covered by that triangle.

We only draw three vertices. There is no rectangle or circle mesh anywhere.
Those three vertices are deliberately placed so the triangle covers the entire
window. Your shape is created later, one pixel at a time, in the fragment shader.

### The triangle is not the shape you see

The C++ draw call submits one triangle:

```cpp
encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, 0, 3);
```

That triangle is deliberately much bigger than the window, so every visible
pixel is covered. You do not see a triangular shape because the fragment shader
makes most of it transparent. The rectangle and circle are **pixel masks**, not
geometry: the fragment shader returns a colour and opacity for each pixel, and
only the pixels on the outline get alpha `1`.

Think of it like painting a wall:

- The vertex shader hangs a huge blank canvas.
- The fragment shader decides what colour each tiny patch of the wall gets.

## `uv` is the canvas coordinate

The vertex shader gives each pixel a coordinate:

```slang
output.uv = corner;
```

`uv` runs from `0` to `1` across the visible window:

- `(0, 0)` is one corner
- `(1, 1)` is the opposite corner

The triangle is clipped to the window, so the parts outside it do not matter.

The fragment shader turns that into a more useful coordinate system:

```slang
float2 p = input.uv * 2.0 - 1.0;
```

Now `p` runs from `-1` to `+1`, with `(0, 0)` in the centre. This is the
coordinate system both shapes are drawn in.

## Why there is an aspect correction

```slang
const float aspect = uniforms.resolution.x / max(uniforms.resolution.y, 1.0);
p.x *= aspect;
```

A window is usually wider than it is tall. Without this line, a circle would
stretch into an oval. Multiplying `x` by the aspect ratio makes one unit in `x`
cover the same physical distance as one unit in `y`.

This is why both shaders need the window resolution.

## The only real difference: the shape test

Both examples ask the same question for every pixel:

> How far is this pixel from the edge of my shape?

Then they use the same outline helper:

```slang
const float edge = 1.0 - smoothstep(0.0, 0.004, abs(distanceFromEdge));
```

`distanceFromEdge` is the key value.

### Rectangle

`1.slang` uses a box test:

```slang
const float2 halfSize = float2(0.34, 0.20);
const float2 d = abs(p) - halfSize;
const float distanceFromEdge = max(d.x, d.y);
```

- `abs(p)` mirrors the coordinates so the left and right halves behave the same.
- `halfSize` is how far the rectangle extends horizontally and vertically.
- `max(d.x, d.y)` is a simple way to find whether we are past the box edge.

### Circle

`3.slang` uses a radial test:

```slang
const float radius = length(p);
const float distanceFromEdge = radius - 0.28;
```

- `length(p)` is the straight-line distance from the centre.
- `radius - 0.28` is zero exactly on a circle of radius `0.28`.
- Inside the circle it is negative; outside it is positive.

That is the entire shape difference. One uses a box distance; the other uses a
radial distance.

## What `smoothstep` and `abs` are doing

The raw shape test gives us a hard, jagged edge:

```slang
abs(radius - 0.28) < 0.004
```

`abs(...)` turns both sides of the boundary into a positive distance. Then
`smoothstep(0.0, 0.004, ...)` fades from `1` inside the line to `0` just outside
it. That tiny fade is anti-aliasing: it stops the circle from looking like a
staircase of pixels.

The fragment shader returns that fade as the **alpha** channel:

```slang
return float4(0.90, 0.92, 0.96, edge);
```

The RGB part is the colour. The last value is opacity. The C++ pipeline has
alpha blending enabled, so the clear colour shows through the transparent
middle.

## Why the files are so similar

The repeated parts are repeated because they describe the shared setup:

1. Define the uniform data.
2. Pass through the full-screen triangle.
3. Convert `uv` to centred `p`.
4. Apply the aspect correction.
5. Build an outline mask.
6. Return a colour and alpha.

Only step 5 changed between `1.slang` and `3.slang`.

As you add more examples, expect this pattern:

- **Small shader changes** often mean changing one formula.
- **The surrounding setup** stays the same.

## Try it yourself

Run an example with:

```sh
./run.sh 1
./run.sh 3
```

Then try these edits:

1. Change the circle radius from `0.28` to `0.15`.
2. Change the rectangle's `halfSize` from `float2(0.34, 0.20)` to
   `float2(0.12, 0.30)`.
3. Make a diamond in a copy of `3.slang` by replacing `length(p)` with
   `abs(p.x) + abs(p.y)`.
4. Make the outline thicker by changing `0.004` to `0.012`.

Each edit changes only the shape maths. The window, vertex shader, and blending
setup stay exactly where they are.

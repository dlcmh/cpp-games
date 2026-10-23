# Roller Coaster: how the path is generated

The example lives in
[`Sources/RollerCoaster/Main.cpp`](Sources/RollerCoaster/Main.cpp). The relevant
functions are:

- `buildTrack()` — generates the path samples.
- `catmullRom()` — smooth interpolation between control points.
- `buildTrackMesh()` — turns samples into rails, ties, and supports.
- `sampleAt()` — looks up a position at a given distance.

The camera does not use a separate animation. It follows the same generated
path as the track.

## Step 1: choose control points

The only hand-authored path data is a small list of 3D points:

```cpp
const std::array<Vec3, 10> controlPoints = {{
    {0, 20, 0},
    {0, 16, 26},
    ...
}};
```

These are the peaks, valleys, and turn positions. They do not form a smooth
track yet.

## Step 2: smooth with Catmull-Rom

A closed Catmull-Rom spline runs through every control point. For each segment,
it uses the point before and after the segment to decide how to curve:

```cpp
Vec3 catmullRom(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, float t);
```

`p1` and `p2` are the segment endpoints; `p0` and `p3` shape the curve around
them.

The result is sampled into a dense polyline. It is smooth, but its points are
not evenly spaced — some parts of the curve get more samples than others.

## Step 3: resample at equal distances

Camera speed should be in metres per second, not “control points per second,” so
the path is resampled:

1. Walk the dense polyline and measure its total length.
2. Pick 700 new points at equal distances around the loop.
3. Store each point’s position.

This is what makes a constant speed feel constant. Without this step, the camera
would speed up and slow down based on how densely the spline happened to be
sampled.

## Step 4: build a frame at every sample

At each point we need to know which way is forward, right, and up:

- **Tangent** — the direction of travel, from the next point minus the previous
  point.
- **Right** — perpendicular to tangent and world up.
- **Up** — perpendicular to both.

```cpp
tangent = normalize(next.position - previous.position);
right = normalize(cross(tangent, worldUp));
up = normalize(cross(right, tangent));
```

Then the frame is banked into turns by rotating `right` and `up` around the
tangent. The example uses the change in tangent between samples as a cheap
curvature signal.

## Step 5: move the camera

The camera keeps a scalar `distance` around the loop:

```cpp
distance = fmod(distance + speed * delta, track.length);
```

`speed` comes from a simple energy model:

```cpp
speed = sqrt(2 * 9.81 * (startHeight - currentHeight));
```

So the camera is faster in valleys and slower over crests.

The camera position is the path point plus a small offset along `up`. The
look-at target is a few metres along `tangent`. No camera keys or animation
curves are involved.

## Step 6: build the track from the same samples

The rails use each sample’s `right` vector to sit on either side of the
centreline. The ties use the tangent for their length. Supports drop from the
track down to `y = 0`.

Because the mesh and the camera consume the same `PathSample` list, they cannot
drift apart when you edit the control points.

## Things to try

- Move or add a control point and watch the whole ride change.
- Change the number of samples in `buildTrack()`.
- Change the bank multiplier to make turns lean more or less.
- Change the speed formula to a constant value.
- Change the camera eye offset to sit higher above the track.

## Why not just follow the control points?

Control points are corners and markers, not a ride. The spline gives smooth
turns. Arc-length resampling gives honest speed. The frame gives the camera a
stable orientation. Those three steps turn a rough list of points into a
roller-coaster path.

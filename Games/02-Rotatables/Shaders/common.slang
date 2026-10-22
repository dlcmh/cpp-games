// Shared uniforms, procedural noise, glyph distance fields, and palette
// helpers. Keep SceneUniforms in sync with SceneUniforms.hpp.

struct SceneUniforms {
    float2 resolution;
    float time;
    float angleA;
    float angleB;
    float selectedIndex;
    float skinA;
    float skinB;
};

[[vk::binding(0, 0)]]
ConstantBuffer<SceneUniforms> uniforms;

float2 rotate2(float2 p, float angle)
{
    float cosine = cos(angle);
    float sine = sin(angle);
    return float2(cosine * p.x - sine * p.y, sine * p.x + cosine * p.y);
}

float hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float valueNoise(float2 p)
{
    float2 cell = floor(p);
    float2 local = frac(p);
    local = local * local * (3.0 - 2.0 * local);

    float a = hash21(cell);
    float b = hash21(cell + float2(1.0, 0.0));
    float c = hash21(cell + float2(0.0, 1.0));
    float d = hash21(cell + float2(1.0, 1.0));

    return lerp(lerp(a, b, local.x), lerp(c, d, local.x), local.y);
}

float fbm(float2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 4; ++octave) {
        value += amplitude * valueNoise(p);
        p *= 2.03;
        amplitude *= 0.5;
    }
    return value;
}

float sdSegment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h);
}

float sdLetterA(float2 p)
{
    float leftLeg = sdSegment(p, float2(-0.43, -0.58), float2(0.0, 0.60));
    float rightLeg = sdSegment(p, float2(0.43, -0.58), float2(0.0, 0.60));
    float crossbar = sdSegment(p, float2(-0.265, -0.075), float2(0.265, -0.075));
    return min(leftLeg, min(rightLeg, crossbar)) - 0.085;
}

float sdLetterB(float2 p)
{
    float stem = sdSegment(p, float2(-0.34, -0.56), float2(-0.34, 0.56));

    // Two tangent rings form the rounded bowls of the B.
    float upperBowl = abs(length(p - float2(-0.02, 0.29)) - 0.305);
    float lowerBowl = abs(length(p - float2(-0.02, -0.29)) - 0.305);
    return min(stem, min(upperBowl, lowerBowl)) - 0.080;
}

float sdRoundBox(float2 p, float2 halfSize, float radius)
{
    float2 q = abs(p) - halfSize + radius;
    return length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

float3 accentColor(int skin)
{
    if (skin == 0) {
        return float3(0.18, 0.95, 0.70);
    }
    if (skin == 1) {
        return float3(1.00, 0.42, 0.08);
    }
    if (skin == 2) {
        return float3(0.25, 0.75, 1.00);
    }
    if (skin == 3) {
        return float3(0.60, 0.92, 1.00);
    }
    return float3(1.00, 0.40, 0.78);
}

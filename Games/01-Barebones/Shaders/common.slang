// Shared math and data layout for Barebones.
// Slang is the master shader source; the build emits Metal Shading Language
// from this file. Keep this struct byte-for-byte in sync with Uniforms in
// MetalRenderer.cpp.

struct Uniforms {
    float2 resolution;
    float time;
    float angle;
};

[[vk::binding(0, 0)]]
ConstantBuffer<Uniforms> uniforms;

float3 background(float2 uv, float2 p)
{
    float2 centered = uv * 2.0 - 1.0;
    centered.x *= uniforms.resolution.x / uniforms.resolution.y;

    float radial = saturate(1.0 - length(centered) * 0.55);
    float3 deep = float3(0.012, 0.018, 0.045);
    float3 lift = float3(0.030, 0.075, 0.125);
    float3 color = lerp(deep, lift, radial * radial);

    // A very quiet horizon glow gives the letter some depth.
    color += float3(0.02, 0.09, 0.10) * exp(-abs(p.y) * 2.4) * 0.35;
    return color;
}

// Signed distance from p to a rounded line segment.
float sdSegment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h);
}

// The letter A as the union of two legs and a crossbar.
float sdLetterA(float2 p)
{
    float leftLeg = sdSegment(p, float2(-0.43, -0.58), float2(0.0, 0.60));
    float rightLeg = sdSegment(p, float2(0.43, -0.58), float2(0.0, 0.60));
    float crossbar = sdSegment(p, float2(-0.265, -0.075), float2(0.265, -0.075));

    float outline = min(leftLeg, min(rightLeg, crossbar));
    return outline - 0.088;
}

float hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

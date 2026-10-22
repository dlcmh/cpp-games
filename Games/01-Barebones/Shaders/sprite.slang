#include "common.slang"

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// Fullscreen triangle: no vertex buffer, just three vertex IDs.
[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 corner = float2((vertexID << 1) & 2, vertexID & 2);
    output.uv = corner;
    output.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float aspect = uniforms.resolution.x / max(uniforms.resolution.y, 1.0);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    // Rotate the letter, not the camera.
    float cosine = cos(uniforms.angle);
    float sine = sin(uniforms.angle);
    float2 rotated = float2(cosine * p.x - sine * p.y,
                            sine * p.x + cosine * p.y);

    float2 letterPoint = rotated * 1.22;
    float distance = sdLetterA(letterPoint);

    float3 color = background(input.uv, p);

    // A handful of fixed stars is enough to make the rotation legible.
    float2 starCell = floor(input.uv * uniforms.resolution / 3.0);
    float star = step(0.9987, hash21(starCell));
    float twinkle = 0.55 + 0.45 * sin(uniforms.time * 1.7 + hash21(starCell) * 30.0);
    color += float3(0.55, 0.75, 0.85) * star * twinkle * 0.22;

    // Antialiasing is derived from the distance field, so the edges stay
    // crisp at every window size and Retina scale.
    float aa = max(fwidth(distance), 0.0005);
    float fill = 1.0 - smoothstep(-aa, aa, distance);
    float halo = exp(-max(distance, 0.0) * 13.0);
    float innerRim = fill * exp(-max(-distance, 0.0) * 55.0);

    float3 bottomInk = float3(0.98, 0.55, 0.22);
    float3 topInk = float3(0.30, 0.95, 0.82);
    float3 ink = lerp(bottomInk, topInk, saturate(letterPoint.y * 0.95 + 0.55));
    ink *= 0.92 + 0.08 * sin(uniforms.time * 2.0 + letterPoint.y * 4.0);

    color += float3(0.05, 0.30, 0.30) * halo * 0.7;
    color = lerp(color, ink, fill);
    color += float3(0.75, 1.0, 0.95) * innerRim * 0.30;

    // Soft vignette keeps focus on the character.
    float vignette = smoothstep(1.75, 0.35, length(p));
    color *= lerp(0.72, 1.0, vignette);

    return float4(color, 1.0);
}

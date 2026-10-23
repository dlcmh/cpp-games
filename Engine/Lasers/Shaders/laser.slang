// Screen-space additive laser bolts. Beams are projected on the CPU, then a
// fullscreen triangle evaluates distance to each 2D segment. This keeps the
// core centered and gives a soft glow without per-beam quad interpolation.

struct ScreenBeam {
    float4 segment;  // start.xy, end.xy in aspect-corrected NDC
    float4 color;    // rgb, seed
    float4 params;   // startWidth, endWidth, style, intensity
};

struct LaserUniforms {
    float4 screen;   // x=aspect
    float4 counts;   // x=beam count
    ScreenBeam beams[24];
};

[[vk::binding(0, 0)]]
ConstantBuffer<LaserUniforms> laser;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 corner = float2((vertexID << 1) & 2, vertexID & 2);
    output.uv = corner;
    output.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

float3 beamColor(ScreenBeam beam, float distance, float t, float halfWidth)
{
    const float x = distance / max(halfWidth, 0.00001);
    const float core = exp(-x * 3.2);
    const float glow = exp(-x * 1.05);
    const float halo = exp(-x * 0.26) * 0.16;
    const float endFade = smoothstep(0.0, 0.12, t) * smoothstep(1.0, 0.82, t);
    const float pulse = 0.87 + 0.13 * sin(t * 30.0 + beam.color.a * 12.0);
    const int style = int(beam.params.z + 0.5);

    float3 color = beam.color.rgb * (glow * (0.74 + smoothstep(0.62, 1.0, t) * 0.7) + halo) * pulse +
                   lerp(beam.color.rgb, float3(1.0), 0.66) * core * 2.2;
    color *= beam.params.w * endFade;

    if (style == 1) {
        color *= float3(0.72, 1.22, 0.72);
    } else if (style == 2) {
        const float arc = pow(max(0.0, sin(t * 74.0 + beam.color.a * 30.0)), 26.0);
        color += beam.color.rgb * arc * glow * 0.9;
    } else if (style == 3) {
        const float hot = 0.55 + 0.45 * sin(t * 19.0 + beam.color.a * 8.0);
        color = lerp(color, float3(1.0, 0.52, 0.12), hot * 0.16) * 1.15;
    }
    return color;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float aspect = max(laser.screen.x, 0.001);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    float3 result = float3(0.0);
    const int count = int(laser.counts.x + 0.5);
    for (int index = 0; index < count; ++index) {
        const ScreenBeam beam = laser.beams[index];
        const float2 start = beam.segment.xy;
        const float2 end = beam.segment.zw;
        const float2 segment = end - start;
        const float lengthSquared = max(dot(segment, segment), 0.000001);
        const float t = saturate(dot(p - start, segment) / lengthSquared);
        const float2 closest = start + segment * t;
        const float distance = length(p - closest);
        const float halfWidth = lerp(beam.params.x, beam.params.y, t);
        result += beamColor(beam, distance, t, halfWidth);
    }

    return float4(result, 1.0);
}

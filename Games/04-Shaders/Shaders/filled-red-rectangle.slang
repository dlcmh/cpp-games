// Example 2: the same rectangle, now filled with a flat colour.

struct Uniforms {
    float2 resolution;
    float time;
    float padding;
};

[[vk::binding(0, 0)]]
ConstantBuffer<Uniforms> uniforms;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    const float2 corner = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);
    output.uv = corner;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float aspect = uniforms.resolution.x / max(uniforms.resolution.y, 1.0);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    const float2 halfSize = float2(0.34, 0.20);
    const float2 d = abs(p) - halfSize;
    const float inside = 1.0 - smoothstep(-0.002, 0.0, max(d.x, d.y));
    const float edge = 1.0 - smoothstep(0.0, 0.004, abs(max(d.x, d.y)));
    const float3 fill = float3(1.00, 0.00, 0.00);
    return float4(lerp(fill, float3(1.0), edge), inside);
}

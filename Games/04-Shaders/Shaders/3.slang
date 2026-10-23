// Example 3: an unfilled circle in the centre of the screen.

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

    const float radius = length(p);
    const float edge = 1.0 - smoothstep(0.0, 0.004, abs(radius - 0.28));
    return float4(0.90, 0.92, 0.96, edge);
}

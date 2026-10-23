// Barebones roller coaster: flat-shaded track strips with distance fog.

struct Camera {
    float4x4 viewProjection;
    float4 cameraPosition;
};

[[vk::binding(0, 0)]]
ConstantBuffer<Camera> camera;

struct Vertex {
    float3 position;
    float3 color;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 color : TEXCOORD1;
};

[shader("vertex")]
VSOutput vertexMain(Vertex input)
{
    VSOutput output;
    output.position = mul(camera.viewProjection, float4(input.position, 1.0));
    output.worldPosition = input.position;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float distance = length(input.worldPosition - camera.cameraPosition.xyz);
    const float fog = saturate((distance - 90.0) / 260.0);
    const float3 sky = float3(0.56, 0.68, 0.82);
    return float4(lerp(input.color, sky, fog), 1.0);
}

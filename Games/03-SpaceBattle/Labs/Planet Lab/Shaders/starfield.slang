struct StarfieldUniforms {
    float2 resolution;
    float time;
    float seed;
    float3 padding;
};

[[vk::binding(0, 0)]]
ConstantBuffer<StarfieldUniforms> stars;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

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
    for (int index = 0; index < 4; ++index) {
        value += amplitude * valueNoise(p);
        p *= 2.03;
        amplitude *= 0.5;
    }
    return value;
}

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
    const float aspect = stars.resolution.x / max(stars.resolution.y, 1.0);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    float3 color = lerp(float3(0.001, 0.002, 0.006), float3(0.006, 0.012, 0.025),
                        saturate(0.5 - p.y * 0.30));
    const float nebula = fbm(float2(p.x * 1.4 + stars.seed, p.y * 1.7));
    color += float3(0.035, 0.012, 0.055) * nebula * 0.55;
    color += float3(0.004, 0.016, 0.030) * (1.0 - nebula);

    for (int layer = 0; layer < 2; ++layer) {
        const float scale = 150.0 + float(layer) * 260.0;
        const float2 cell = floor(input.uv * scale);
        const float h = hash21(cell + float(layer) * 19.0);
        if (h > 0.885) {
            const float2 local =
                frac(input.uv * scale) - float2(hash21(cell + 1.1), hash21(cell + 4.7));
            const float star = 1.0 - smoothstep(0.01, 0.045, length(local));
            const float twinkle = 0.55 + 0.45 * sin(stars.time * (0.7 + h) + h * 50.0);
            color += float3(0.72, 0.84, 1.0) * star * twinkle * (0.50 + h * 0.75);
        }
    }

    color *= lerp(0.72, 1.0, smoothstep(1.8, 0.45, length(p)));
    return float4(color, 1.0);
}

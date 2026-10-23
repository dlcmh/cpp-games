struct RingUniforms {
    float4x4 viewProjection;
    float4x4 model;
    float4 cameraPosition;
    float4 sunDirection;
    float4 ring;                 // inner, outer, opacity, seed
    float4 planetCenterRadius;   // xyz=center, w=radius
    float4 colorA;
    float4 colorB;
};

[[vk::binding(0, 0)]]
ConstantBuffer<RingUniforms> ringData;

struct VSOutput {
    float4 position : SV_Position;
    float3 local : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
};

float hash11(float p)
{
    p = frac(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return frac(p);
}

float noise1(float p)
{
    const float i = floor(p);
    const float f = frac(p);
    const float u = f * f * (3.0 - 2.0 * f);
    return lerp(hash11(i), hash11(i + 1.0), u);
}

float fbm1(float p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int index = 0; index < 4; ++index) {
        value += amplitude * noise1(p);
        p *= 2.07;
        amplitude *= 0.5;
    }
    return value;
}

float ringBand(float t, float seed)
{
    const float bands = fbm1(t * 20.0 + seed * 31.0);
    const float fine = fbm1(t * 92.0 + seed * 7.0);
    float density = smoothstep(0.22, 0.64, bands * 0.82 + fine * 0.18);
    density = 0.55 + 0.45 * density;
    density *= 0.80 + 0.20 * sin(t * 46.0 + seed * 9.0);
    return saturate(density);
}

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    const float2 corners[4] = {
        float2(-1.0, -1.0), float2(1.0, -1.0), float2(1.0, 1.0), float2(-1.0, 1.0)
    };
    const float2 corner = corners[vertexID];
    const float outer = ringData.ring.y;
    const float3 local = float3(corner.x * outer, 0.0, corner.y * outer);
    const float4 world = mul(ringData.model, float4(local, 1.0));

    VSOutput output;
    output.position = mul(ringData.viewProjection, world);
    output.local = local;
    output.worldPosition = world.xyz;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float inner = ringData.ring.x;
    const float outer = ringData.ring.y;
    const float opacity = ringData.ring.z;
    const float seed = ringData.ring.w;
    const float radius = length(input.local.xz);
    if (radius < inner || radius > outer) {
        discard;
    }

    const float t = (radius - inner) / max(outer - inner, 0.0001);
    const float band = ringBand(t, seed);
    float alpha = band * opacity;
    alpha *= smoothstep(0.0, 0.06, t) * smoothstep(1.0, 0.88, t);
    if (alpha < 0.003) {
        discard;
    }

    const float3 normal = normalize(mul((float3x3)ringData.model, float3(0.0, 1.0, 0.0)));
    const float3 light = normalize(ringData.sunDirection.xyz);
    const float3 toPlanet = ringData.planetCenterRadius.xyz - input.worldPosition;
    const float lightDistance = dot(toPlanet, light);
    float shadow = 0.0;
    if (lightDistance > 0.0) {
        const float3 closest = input.worldPosition + light * lightDistance;
        const float distanceToPlanet = length(closest - ringData.planetCenterRadius.xyz);
        if (distanceToPlanet < ringData.planetCenterRadius.w) {
            shadow = 0.72;
        }
    }

    const float3 base = lerp(ringData.colorB.rgb, ringData.colorA.rgb,
                             saturate(band * 0.8 + 0.2 * noise1(t * 24.0 + seed)));
    const float3 color =
        base * (1.0 - shadow) * (0.42 + 0.58 * saturate(dot(normal, light))) +
        base * 0.12;
    return float4(saturate(color * 1.25), alpha);
}

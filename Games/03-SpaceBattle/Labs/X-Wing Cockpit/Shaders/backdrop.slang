// Cockpit lab backdrop: procedural stars, a planet limb, a distant target,
// and a hyperspace streak layer. This is intentionally not the SpaceBattle
// renderer; the lab isolates the shared cockpit overlay.

struct BackdropUniforms {
    float2 resolution;
    float time;
    float jump;
    float steer;
    float3 padding;
};

[[vk::binding(0, 0)]]
ConstantBuffer<BackdropUniforms> backdrop;

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

float3 stars(float2 uv, float2 p, float time)
{
    float3 color = lerp(float3(0.003, 0.006, 0.018), float3(0.012, 0.025, 0.055),
                        saturate(0.5 + p.y * 0.4));
    float nebula = fbm(float2(p.x * 1.5 + time * 0.004, p.y * 1.9));
    color += float3(0.08, 0.03, 0.13) * nebula * 0.45;
    color += float3(0.01, 0.04, 0.07) * (1.0 - nebula);

    for (int layer = 0; layer < 2; ++layer) {
        const float scale = 120.0 + float(layer) * 190.0;
        const float2 cell = floor(uv * scale);
        const float h = hash21(cell + float(layer) * 23.0);
        if (h > 0.88) {
            const float2 local = frac(uv * scale) - float2(hash21(cell + 1.1),
                                                           hash21(cell + 4.7));
            const float star = 1.0 - smoothstep(0.012, 0.05, length(local));
            const float twinkle = 0.55 + 0.45 * sin(time * (0.8 + h) + h * 50.0);
            color += float3(0.65, 0.80, 1.0) * star * twinkle * (0.35 + h * 0.65);
        }
    }
    return color;
}

float3 planet(float2 p)
{
    const float2 center = float2(-0.95 + backdrop.steer * 0.03, -0.62);
    const float radius = 0.82;
    const float2 local = p - center;
    const float distance = length(local);
    if (distance > radius) {
        return float3(0.0);
    }

    const float z = sqrt(max(0.0, 1.0 - (distance / radius) * (distance / radius)));
    const float3 normal = normalize(float3(local / radius, z));
    const float clouds = fbm(normal.xz * 5.0 + normal.y * 2.4);
    const float continents = fbm(normal.xz * 2.2 + normal.y * 1.3 + 2.0);

    const float3 ocean = float3(0.02, 0.12, 0.30);
    const float3 land = float3(0.12, 0.34, 0.16);
    float3 color = lerp(ocean, land, smoothstep(0.46, 0.60, continents));
    color = lerp(color, float3(0.72, 0.82, 0.90), smoothstep(0.54, 0.70, clouds));
    color *= 0.42 + 0.58 * saturate(dot(normal, normalize(float3(-0.5, 0.45, 0.75))));

    const float rim = pow(1.0 - z, 2.6);
    color += float3(0.20, 0.55, 0.95) * rim * 0.85;
    return color;
}

float3 target(float2 p)
{
    const float2 center = float2(0.82, 0.23);
    const float distance = length(p - center);
    if (distance > 0.135) {
        return float3(0.0);
    }

    const float z = sqrt(max(0.0, 1.0 - (distance / 0.135) * (distance / 0.135)));
    const float3 normal = normalize(float3((p - center) / 0.135, z));
    const float panels = 1.0 - smoothstep(0.0, 0.025,
                                          min(abs(frac(normal.x * 14.0) - 0.5),
                                              abs(frac(normal.y * 14.0) - 0.5)));
    float3 color = float3(0.42, 0.44, 0.48) * (1.0 - panels * 0.4);
    color *= 0.55 + 0.60 * saturate(dot(normal, normalize(float3(-0.3, 0.5, 0.8))));
    const float dish = smoothstep(0.88, 0.96, dot(normal, normalize(float3(-0.35, 0.2, 0.92))));
    color = lerp(color, float3(0.10, 0.12, 0.14), dish);
    color += float3(0.3, 1.0, 0.5) * smoothstep(0.995, 1.0,
                                                dot(normal, normalize(float3(-0.35, 0.2, 0.92)))) * 2.0;
    return color;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float aspect = backdrop.resolution.x / max(backdrop.resolution.y, 1.0);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    float3 color = stars(input.uv, p, backdrop.time);

    const float3 planetColor = planet(p);
    const float planetMask = step(0.001, dot(planetColor, float3(1.0)));
    color = lerp(color, planetColor, planetMask);

    const float3 targetColor = target(p);
    color = lerp(color, targetColor, step(0.001, dot(targetColor, float3(1.0))));

    if (backdrop.jump > 0.0) {
        const float angle = atan2(p.y, p.x);
        const float radius = length(p);
        const float starsStreak =
            pow(max(0.0, sin(angle * 58.0 + backdrop.time * 14.0)), 42.0) *
            smoothstep(0.12, 1.0, radius);
        color += float3(0.35, 0.65, 1.0) * starsStreak * backdrop.jump * 0.7;
        color += float3(0.10, 0.25, 0.65) * smoothstep(0.6, 1.7, radius) * backdrop.jump * 0.25;
    }

    color *= lerp(0.65, 1.0, smoothstep(1.8, 0.4, length(p)));
    return float4(color, 1.0);
}

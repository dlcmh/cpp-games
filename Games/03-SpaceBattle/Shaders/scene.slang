// 3D scene shaders. The CPU builds one static vertex/index buffer containing
// every model; these shaders light and texture those meshes procedurally.

struct FrameUniforms {
    float4x4 viewProjection;
    float4 cameraPosition;
    float4 lightDirection;
    float4 timing;  // x=time, y=phase, z=phaseTime, w=jump
};

struct ObjectUniforms {
    float4x4 model;
    float4 color;
    float4 params;  // x=material, y=effect phase, zw=reserved
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> frame;

[[vk::binding(1, 0)]]
ConstantBuffer<ObjectUniforms> objectData;

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
};

struct VSOutput {
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 localPosition : TEXCOORD2;
    float3 color : TEXCOORD3;
};

struct FragmentOutput {
    float4 color : SV_Target;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    const float4 world = mul(objectData.model, float4(input.position, 1.0));
    output.position = mul(frame.viewProjection, world);
    output.worldPosition = world.xyz;
    output.normal = normalize(mul(objectData.model, float4(input.normal, 0.0)).xyz);
    output.localPosition = input.position;
    output.color = input.color * objectData.color.rgb;
    return output;
}

float hash21(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float hash31(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
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

float3 starfield(float3 direction, float time)
{
    float3 color = lerp(float3(0.002, 0.004, 0.014), float3(0.010, 0.020, 0.050),
                        saturate(direction.y * 0.5 + 0.5));
    float nebula = fbm(direction.xz * 3.0 + direction.y * 2.0);
    color += float3(0.09, 0.035, 0.15) * nebula * 0.55;
    color += float3(0.01, 0.04, 0.07) * (1.0 - nebula);

    // Three depth layers of procedural stars, with a twinkle.
    for (int layer = 0; layer < 3; ++layer) {
        const float scale = 70.0 + float(layer) * 115.0;
        float3 cell = floor(direction * scale);
        float h = hash31(cell + float(layer) * 17.0);
        if (h > 0.86) {
            float3 local = frac(direction * scale) - float3(hash31(cell + 1.3),
                                                           hash31(cell + 4.7),
                                                           hash31(cell + 9.1));
            float distanceToStar = length(local);
            float size = 0.028 + 0.015 * (1.0 - h);
            float star = 1.0 - smoothstep(size, size + 0.012, distanceToStar);
            float twinkle = 0.55 + 0.45 * sin(time * (1.2 + h) + h * 70.0);
            color += float3(0.72, 0.84, 1.0) * star * twinkle * (0.55 + h * 0.75);
        }
    }

    // A warm galactic core in one quadrant.
    float band = exp(-abs(direction.y * 4.0 - sin(atan2(direction.z, direction.x) * 2.0)) * 1.3);
    color += float3(0.20, 0.10, 0.06) * band * 0.12;

    return color;
}

float3 surfacePanelColor(float3 worldPosition, float3 base)
{
    const float2 grid = abs(frac(worldPosition.xz * 0.22) - 0.5);
    const float line = 1.0 - smoothstep(0.0, 0.025, min(grid.x, grid.y));
    const float2 vertical = abs(frac(worldPosition.yy * 0.4 + worldPosition.xz * 0.12) - 0.5);
    const float detail = 1.0 - smoothstep(0.0, 0.02, min(vertical.x, vertical.y));
    const float lights = step(0.985, hash21(floor(worldPosition.xz * 0.35) + floor(worldPosition.y * 3.0)));
    return base * (1.0 - line * 0.34 - detail * 0.12) + float3(0.10, 0.30, 0.38) * lights;
}

float3 planetColor(VSOutput input, float3 normal, float3 view, float time)
{
    const float continents = fbm(normal.xz * 2.8 + normal.y * 1.7 + time * 0.003);
    const float clouds = fbm(normal.xz * 7.0 + normal.y * 3.0 + time * 0.008);
    const float3 ocean = float3(0.025, 0.16, 0.36);
    const float3 land = float3(0.16, 0.38, 0.20);
    const float3 base = lerp(ocean, land, smoothstep(0.48, 0.62, continents));
    const float3 cloud = lerp(base, float3(0.78, 0.86, 0.92), smoothstep(0.55, 0.72, clouds));

    const float rim = pow(1.0 - max(dot(normal, view), 0.0), 2.8);
    return cloud * (0.65 + 0.35 * saturate(dot(normal, float3(0.45, 0.5, 0.74)))) +
           float3(0.22, 0.55, 0.92) * rim * 0.8;
}

float3 deathStarColor(VSOutput input, float3 normal, float3 view, float time)
{
    const float latitude = asin(clamp(normal.y, -1.0, 1.0));
    const float longitude = atan2(normal.z, normal.x);
    const float latGrid = abs(frac(latitude * 18.0 / 3.14159) - 0.5);
    const float lonGrid = abs(frac(longitude * 34.0 / 3.14159) - 0.5);
    const float panelLine = 1.0 - smoothstep(0.0, 0.035, min(latGrid, lonGrid));
    float3 base = float3(0.62, 0.64, 0.68) * (1.0 - panelLine * 0.45);

    const float3 dishDirection = normalize(float3(0.10, 0.32, 0.94));
    const float dishDot = dot(normal, dishDirection);
    const float dish = smoothstep(0.955, 0.972, dishDot);
    const float dishRadius = acos(clamp(dishDot, -1.0, 1.0));
    const float dishRings = 1.0 - smoothstep(0.0, 0.03, abs(frac(dishRadius * 26.0) - 0.5));
    base = lerp(base, float3(0.20, 0.22, 0.24) * (1.0 - dishRings * 0.35), dish);

    const float trench = 1.0 - smoothstep(0.035, 0.055, abs(normal.y));
    base = lerp(base, float3(0.10, 0.12, 0.15), trench * 0.9);

    const float lights = step(0.990, hash21(float2(longitude * 30.0, latitude * 18.0)));
    const float3 lightsColor = float3(0.45, 1.0, 0.55) * lights * (1.0 - dish) * (1.0 - trench);
    const float dishCore = smoothstep(0.997, 0.999, dishDot);

    const float rim = pow(1.0 - max(dot(normal, view), 0.0), 3.0);
    return base * (0.70 + 0.55 * saturate(dot(normal, normalize(float3(-0.4, 0.55, 0.75))))) +
           lightsColor + float3(0.5, 1.0, 0.55) * dishCore * 1.5 +
           float3(0.18, 0.30, 0.42) * rim;
}

float3 shadeSurface(VSOutput input, float3 normal, float3 view, int material, float time)
{
    const float ndl = saturate(dot(normal, normalize(-frame.lightDirection.xyz)));
    const float ambient = 0.28;
    const float fillLight = saturate(dot(normal, view)) * 0.34;
    float3 base = input.color;
    float3 emission = float3(0.0);

    if (material == 2) {
        base = planetColor(input, normal, view, time);
    } else if (material == 3) {
        base = deathStarColor(input, normal, view, time);
    } else if (material == 4) {
        base = surfacePanelColor(input.worldPosition, input.color * 1.15);
        const float fog = saturate(1.0 - length(input.worldPosition - frame.cameraPosition.xyz) / 430.0);
        emission += float3(0.06, 0.22, 0.26) * (1.0 - fog) * 0.22;
    } else if (material == 5) {
        const float rock = fbm(input.localPosition.xz * 5.0 + input.localPosition.y * 3.0);
        base *= 0.55 + rock * 0.7;
    } else if (material == 6) {
        const float glow = exp(-length(input.localPosition) * 8.0);
        emission += float3(0.20, 0.75, 0.35) * glow * 0.35;
    } else if (material == 7) {
        const float windows = step(0.988, hash21(floor(input.localPosition.xz * 0.9) +
                                                 floor(input.localPosition.y * 3.0)));
        emission += float3(0.55, 0.70, 1.0) * windows * 0.25;
        base = surfacePanelColor(input.worldPosition * 0.7, input.color);
    } else if (material == 1) {
        emission += input.color;
        base = input.color;
    }

    return base * (ambient + 0.95 * ndl + fillLight) + emission;
}

[shader("fragment")]
FragmentOutput fragmentMain(VSOutput input)
{
    const int material = int(objectData.params.x + 0.5);
    const float time = frame.timing.x;
    const float3 normal = normalize(input.normal);
    const float3 view = normalize(frame.cameraPosition.xyz - input.worldPosition);

    FragmentOutput output;

    if (material == 8) {
        output.color = float4(starfield(normalize(input.localPosition), time), 1.0);
        return output;
    }

    if (material == 9) {
        const float core = exp(-abs(input.normal.z) * 2.0);
        output.color = float4(objectData.color.rgb * (2.2 + core * 2.0),
                              objectData.color.a);
        return output;
    }

    if (material == 10) {
        const float noise = fbm(input.localPosition.xz * 5.0 + input.localPosition.y * 4.5 +
                                objectData.params.y * 0.8);
        const float hot = smoothstep(0.28, 0.82, noise) *
                          (0.7 + 0.3 * sin(noise * 18.0 + objectData.params.y * 8.0));
        const float3 fire = lerp(objectData.color.rgb * 0.35,
                                 float3(1.0, 0.86, 0.48), hot);
        output.color = float4(fire * (0.55 + hot * 0.75), objectData.color.a);
        return output;
    }

    const float3 shaded = shadeSurface(input, normal, view, material, time);
    output.color = float4(shaded * 1.18, objectData.color.a);
    return output;
}

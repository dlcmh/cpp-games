struct MeshVertex {
    float4 position;
    float4 normal;
};

struct PlanetUniforms {
    float4x4 viewProjection;
    float4x4 model;
    float4 cameraPosition;     // xyz
    float4 sunDirection;       // xyz
    float4 planet;             // x=radius, y=time, z=type, w=seed
    float4 surface;            // x=noiseScale, y=warp, z=ice, w=cloudCoverage
    float4 appearance;         // x=specular, y=cityLights, z=volcanic, w=atmosphere
    float4 colorA;
    float4 colorB;
    float4 colorC;
    float4 colorD;
    float4 ring;               // x=inner, y=outer, z=opacity, w=enabled
    float4 ringPlane;          // xyz=normal, w=planetRadius
    float4 ringCenter;         // xyz=center
    float4 ringColorA;
    float4 ringColorB;
    float4 atmosphereColor;
    float4 detail;             // x=craterDensity, y=cloudSpeed
};

[[vk::binding(0, 0)]]
StructuredBuffer<MeshVertex> vertices;

[[vk::binding(1, 0)]]
ConstantBuffer<PlanetUniforms> planet;

struct VSOutput {
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 localPosition : TEXCOORD2;
};

float hash13(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float3 hash33(float3 p)
{
    p = frac(p * float3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return frac((p.xxy + p.yxx) * p.zyx);
}

float noise3(float3 p)
{
    const float3 cell = floor(p);
    const float3 local = frac(p);
    const float3 smooth = local * local * (3.0 - 2.0 * local);

    const float a = hash13(cell);
    const float b = hash13(cell + float3(1.0, 0.0, 0.0));
    const float c = hash13(cell + float3(0.0, 1.0, 0.0));
    const float d = hash13(cell + float3(1.0, 1.0, 0.0));
    const float e = hash13(cell + float3(0.0, 0.0, 1.0));
    const float f = hash13(cell + float3(1.0, 0.0, 1.0));
    const float g = hash13(cell + float3(0.0, 1.0, 1.0));
    const float h = hash13(cell + float3(1.0, 1.0, 1.0));

    return lerp(lerp(lerp(a, b, smooth.x), lerp(c, d, smooth.x), smooth.y),
                lerp(lerp(e, f, smooth.x), lerp(g, h, smooth.x), smooth.y), smooth.z);
}

float fbm3(float3 p, int octaves)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int index = 0; index < octaves; ++index) {
        value += amplitude * noise3(p);
        p = p * 2.02 + float3(1.7, 9.2, 3.8);
        amplitude *= 0.5;
    }
    return value;
}

float worley3(float3 p)
{
    const float3 cell = floor(p);
    const float3 local = frac(p);
    float closest = 10.0;
    for (int z = -1; z <= 1; ++z) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                const float3 offset = float3(float(x), float(y), float(z));
                const float3 feature = hash33(cell + offset);
                const float distance = length(offset + feature - local);
                closest = min(closest, distance);
            }
        }
    }
    return closest;
}

float2 craterSample(float3 p, float scale, float seed)
{
    const float3 q = p * scale + float3(seed * 0.37, seed * 0.73, seed * 0.19);
    const float f1 = worley3(q);
    const float bowl = 1.0 - smoothstep(0.02, 0.38, f1);
    const float rim = smoothstep(0.24, 0.34, f1) * (1.0 - smoothstep(0.34, 0.50, f1));
    return float2(bowl, rim);
}

float craterField(float3 p, float scale, float seed)
{
    const float2 crater = craterSample(p, scale, seed);
    return crater.x * 0.72 + crater.y * 0.55;
}

float ridged3(float3 p)
{
    const float n = fbm3(p, 4);
    return 1.0 - abs(2.0 * n - 1.0);
}

float reliefHeight(float3 p, float type, float seed, float craterAmount)
{
    const float3 offset = float3(seed * 0.21, seed * 0.43, seed * 0.87);
    float height = fbm3(p * 7.0 + offset, 3);
    if (type > 4.5) {
        height += craterField(p, 7.0, seed) * craterAmount * 1.4;
    } else if (type > 2.5 && type < 3.5) {
        height += ridged3(p * 6.0 + offset) * 0.55;
    }
    return height;
}

float ringBand(float t, float seed)
{
    const float bands = fbm3(float3(t * 46.0 + seed * 31.0, 0.5, 0.5), 4);
    const float fine = fbm3(float3(t * 170.0 + seed * 7.0, 1.5, 0.5), 3);
    float density = smoothstep(0.28, 0.62, bands * 0.75 + fine * 0.35);
    density *= 0.55 + 0.45 * sin(t * 160.0 + seed * 9.0);
    return saturate(density);
}

float ringShadow(float3 worldPosition, float3 light)
{
    if (planet.ring.w < 0.5) {
        return 0.0;
    }
    const float3 normal = normalize(planet.ringPlane.xyz);
    const float denominator = dot(normal, light);
    if (abs(denominator) < 0.0001) {
        return 0.0;
    }
    const float t = dot(planet.ringCenter.xyz - worldPosition, normal) / denominator;
    if (t <= 0.0) {
        return 0.0;
    }
    const float3 hit = worldPosition + light * t;
    const float radius = length(hit - planet.ringCenter.xyz);
    if (radius < planet.ring.x || radius > planet.ring.y) {
        return 0.0;
    }
    const float u = (radius - planet.ring.x) / max(planet.ring.y - planet.ring.x, 0.0001);
    return ringBand(u, planet.planet.w) * planet.ring.z * 0.70;
}

[shader("vertex")]
VSOutput planetVertex(uint vertexID : SV_VertexID)
{
    const MeshVertex vertex = vertices[vertexID];
    const float4 world = mul(planet.model, float4(vertex.position.xyz, 1.0));

    VSOutput output;
    output.position = mul(planet.viewProjection, world);
    output.worldPosition = world.xyz;
    output.normal = normalize(mul((float3x3)planet.model, vertex.normal.xyz));
    output.localPosition = vertex.position.xyz;
    return output;
}

[shader("fragment")]
float4 planetFragment(VSOutput input) : SV_Target
{
    const float type = planet.planet.z;
    const float time = planet.planet.y;
    const float seed = planet.planet.w;
    const float3 L = normalize(planet.sunDirection.xyz);
    const float3 V = normalize(planet.cameraPosition.xyz - input.worldPosition);
    float3 N = normalize(input.normal);
    const float3 p = normalize(input.localPosition);
    float ndl = dot(N, L);
    float day = smoothstep(-0.10, 0.32, ndl);

    float3 base = planet.colorA.rgb;
    float3 emissive = float3(0.0);
    float specularMask = 0.0;
    float iceMask = 0.0;
    float land = 0.0;
    float reliefAmount = 0.0;

    const float3 seedOffset = float3(seed * 0.73, seed * 1.31, seed * 0.47);

    if (type < 0.5) {
        // Terran: oceans, continents, ice caps, clouds, and night-side cities.
        const float warp = (fbm3(p * 2.7 + seedOffset, 4) - 0.5) * planet.surface.y;
        const float height = fbm3(p * planet.surface.x + warp + seedOffset, 5);
        const float detail = fbm3(p * 11.0 + seedOffset, 3);
        land = smoothstep(0.47, 0.53, height + (detail - 0.5) * 0.08);
        const float3 ocean = lerp(planet.colorA.rgb * 0.55, planet.colorA.rgb,
                                  smoothstep(0.25, 0.55, detail));
        float3 ground = lerp(planet.colorB.rgb, planet.colorC.rgb,
                             smoothstep(0.50, 0.78, height + detail * 0.18));
        ground = lerp(ground, planet.colorB.rgb * 0.62, smoothstep(0.72, 0.9, detail) * 0.35);
        base = lerp(ocean, ground, land);

        iceMask = smoothstep(0.66, 0.92, abs(p.y)) * planet.surface.z;
        base = lerp(base, planet.colorD.rgb, iceMask);

        const float clouds = fbm3(p * 3.7 + float3(time * planet.detail.y, 0.0,
                                                   time * planet.detail.y * 0.6) + seedOffset,
                                  5);
        const float cloud = smoothstep(0.48, 0.70, clouds + planet.surface.w * 0.10) *
                            planet.surface.w;
        base = lerp(base, planet.colorD.rgb * 0.98, cloud * 0.85);

        specularMask = (1.0 - land) * 0.9 + iceMask * 0.35;
        const float cityNoise = fbm3(p * 30.0 + seedOffset, 3);
        const float city = land * (1.0 - cloud) * smoothstep(0.62, 0.80, cityNoise) *
                           planet.appearance.y * (1.0 - day);
        emissive += float3(1.0, 0.52, 0.20) * city * 1.4;
        reliefAmount = land * 0.82;
    } else if (type < 1.5) {
        // Desert: dunes, dark basins, and dusty highlands.
        const float n = fbm3(p * planet.surface.x + seedOffset, 5);
        const float detail = ridged3(p * 9.0 + seedOffset);
        base = lerp(planet.colorB.rgb, planet.colorA.rgb, smoothstep(0.32, 0.72, n));
        base = lerp(base, planet.colorC.rgb, smoothstep(0.58, 0.86, detail) * 0.35);
        base *= 0.78 + 0.22 * detail;
        iceMask = smoothstep(0.86, 0.98, abs(p.y)) * planet.surface.z;
        base = lerp(base, planet.colorD.rgb, iceMask);
        specularMask = iceMask * 0.2;
        const float crater = craterField(p, 5.2, seed) * (0.25 + 0.55 * planet.detail.x);
        base = lerp(base, planet.colorB.rgb * 0.55, saturate(crater));
        reliefAmount = 0.55;
    } else if (type < 2.5) {
        // Arctic: layered ice, crevasses, and polar light.
        const float n = fbm3(p * planet.surface.x + seedOffset, 5);
        const float cracks = pow(ridged3(p * 6.0 + seedOffset), 6.0);
        base = lerp(planet.colorB.rgb, planet.colorA.rgb, smoothstep(0.30, 0.72, n));
        base = lerp(base, planet.colorC.rgb, cracks * 0.45);
        iceMask = smoothstep(0.20, 0.75, n + planet.surface.z * 0.35);
        base = lerp(base, planet.colorA.rgb * 1.06, iceMask * 0.75);
        specularMask = 0.45 + iceMask * 0.45;
        reliefAmount = 0.08;
    } else if (type < 3.5) {
        // Volcanic: basalt, glowing fissures, and ash.
        const float n = fbm3(p * planet.surface.x + seedOffset, 5);
        const float detail = ridged3(p * 4.2 + seedOffset);
        base = lerp(planet.colorA.rgb, planet.colorC.rgb, smoothstep(0.30, 0.72, n));
        base *= 0.62 + 0.38 * detail;
        const float lava = smoothstep(0.92, 1.0, detail) * planet.appearance.z;
        const float fineLava = smoothstep(0.955, 1.0, ridged3(p * 15.0 + seedOffset)) *
                               planet.appearance.z;
        base = lerp(base, planet.colorB.rgb, saturate(lava * 0.75 + fineLava * 0.55));
        emissive += planet.colorB.rgb * (lava * 0.30 + fineLava * 0.55);
        const float crater = craterField(p, 4.0, seed) * planet.detail.x;
        base = lerp(base, planet.colorA.rgb * 0.40, crater * 0.30);
        specularMask = 0.02;
        reliefAmount = 0.95;
    } else if (type < 4.5) {
        // Gas giant: turbulent latitude bands and storm ovals.
        const float warp = fbm3(p * 2.5 + seedOffset, 4);
        const float bands = p.y * 16.0 + warp * 2.4 + 0.07 * sin(p.y * 42.0);
        const float band = 0.5 + 0.5 * sin(bands);
        base = lerp(planet.colorA.rgb, planet.colorB.rgb, band);
        base = lerp(base, planet.colorC.rgb, smoothstep(0.62, 0.88,
                                                       fbm3(p * 5.0 + seedOffset, 4)) *
                                                   0.42);
        const float storm = smoothstep(0.74, 0.93, fbm3(p * 3.4 + seedOffset, 4));
        base = lerp(base, planet.colorD.rgb, storm * 0.55);
        specularMask = 0.02;
        reliefAmount = 0.14;
    } else if (type < 5.5) {
        // Pockmarked moon: overlapping craters, rays, and dark maria.
        const float n = fbm3(p * planet.surface.x + seedOffset, 5);
        base = lerp(planet.colorA.rgb, planet.colorB.rgb, smoothstep(0.30, 0.70, n));
        const float2 crater = craterSample(p, 3.6, seed);
        const float2 fineCrater = craterSample(p, 9.0, seed + 7.0);
        const float maria = smoothstep(0.42, 0.62, fbm3(p * 1.5 + seedOffset, 3));
        base = lerp(base, planet.colorB.rgb * 0.30, maria * 0.45);
        base = lerp(base, planet.colorB.rgb * 0.32,
                    saturate(crater.x * planet.detail.x) * 0.62);
        base = lerp(base, planet.colorC.rgb,
                    saturate(crater.y * planet.detail.x) * 0.035);
        base = lerp(base, planet.colorC.rgb,
                    saturate(fineCrater.y * planet.detail.x) * 0.02);
        base *= 0.78 + 0.22 * fbm3(p * 22.0 + seedOffset, 3);
        base *= 1.12;
        specularMask = 0.02;
        reliefAmount = 0.58;
    } else {
        // Toxic: dense clouds over a chemically stained surface.
        const float n = fbm3(p * planet.surface.x + seedOffset, 5);
        const float clouds = fbm3(p * 4.2 + seedOffset + float3(time * 0.02, 0.0, 0.0), 5);
        base = lerp(planet.colorA.rgb, planet.colorB.rgb, smoothstep(0.30, 0.72, n));
        const float cloud = smoothstep(0.42, 0.66, clouds + planet.surface.w * 0.12) *
                            planet.surface.w;
        base = lerp(base, planet.colorC.rgb, cloud * 0.25);
        base = lerp(base, planet.colorB.rgb * 1.05, cloud * 0.45);
        specularMask = 0.0;
        reliefAmount = 0.22;
    }

    if (reliefAmount > 0.001) {
        const float3 reference =
            abs(N.y) < 0.9 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
        const float3 tangent = normalize(cross(N, reference));
        const float3 bitangent = cross(N, tangent);
        const float epsilon = 0.008;
        const float h0 = reliefHeight(p, type, seed, planet.detail.x);
        const float hx = reliefHeight(normalize(p + tangent * epsilon), type, seed,
                                      planet.detail.x);
        const float hy = reliefHeight(normalize(p + bitangent * epsilon), type, seed,
                                      planet.detail.x);
        const float3 reliefNormal = normalize(N - tangent * (hx - h0) * 34.0 * reliefAmount -
                                              bitangent * (hy - h0) * 34.0 * reliefAmount);
        N = reliefNormal;
        ndl = dot(N, L);
        day = smoothstep(-0.10, 0.32, ndl);
    }

    const float ringShadowAmount = ringShadow(input.worldPosition, L);
    const float direct = saturate(ndl);
    const float light = (0.025 + day * (0.08 + 0.92 * direct)) * (1.0 - ringShadowAmount);

    float3 color = base * light;
    const float cameraFill = saturate(dot(N, V));
    color += base * (float3(0.016, 0.028, 0.052) * (1.0 - day) +
                     float3(0.015, 0.024, 0.042) * cameraFill * (1.0 - day));

    if (specularMask > 0.001) {
        const float3 H = normalize(L + V);
        const float spec = pow(saturate(dot(N, H)), 42.0);
        color += float3(1.0, 0.96, 0.88) * spec * specularMask * planet.appearance.x *
                 saturate(ndl);
    }

    color += emissive;
    const float fresnel = pow(1.0 - saturate(dot(N, V)), 3.2);
    color += planet.atmosphereColor.rgb * fresnel * 0.12 * day;
    color *= 1.30;
    color = color / (color + float3(0.90));
    color = pow(saturate(color), float3(0.92));
    return float4(saturate(color), 1.0);
}

[shader("fragment")]
float4 atmosphereFragment(VSOutput input) : SV_Target
{
    const float3 N = normalize(input.normal);
    const float3 V = normalize(planet.cameraPosition.xyz - input.worldPosition);
    const float3 L = normalize(planet.sunDirection.xyz);
    const float rim = pow(1.0 - abs(dot(N, V)), 3.0);
    const float sun = saturate(dot(N, L) * 0.55 + 0.45);
    const float3 color = planet.atmosphereColor.rgb * rim * sun * planet.appearance.w * 1.15;
    return float4(color, 1.0);
}

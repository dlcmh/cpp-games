// Five procedural skins. Each skin supplies one fill treatment and one edge
// treatment; the scene shader composes them with the glyph distance field.

float3 skinFill(int skin, float2 p, float time)
{
    if (skin == 0) {
        // Aurora: slow ribbons flowing through the surface.
        float ribbon = 0.5 + 0.5 * sin(p.y * 8.0 + time * 1.4 + sin(p.x * 4.0 + time * 0.7) * 1.8);
        float wave = 0.5 + 0.5 * sin(p.x * 6.0 - time * 1.1 + p.y * 3.0);
        float3 color = lerp(float3(0.03, 0.85, 0.62), float3(0.55, 0.35, 1.0), ribbon * 0.8);
        color = lerp(color, float3(0.20, 0.95, 0.90), wave * 0.35);
        color += 0.06 * sin((p.x + p.y) * 22.0 + time * 2.0);
        return color;
    }

    if (skin == 1) {
        // Ember: molten metal with cracks that open and heal.
        float slow = fbm(p * 3.0 + float2(time * 0.10, -time * 0.06));
        float veins = abs(fbm(p * 5.0 + slow * 1.7) - 0.5);
        float crack = 1.0 - smoothstep(0.0, 0.085, veins);
        crack *= 0.55 + 0.45 * sin(time * 0.9 + slow * 7.0);

        float3 base = lerp(float3(0.38, 0.025, 0.01), float3(1.0, 0.46, 0.06),
                           saturate(p.y * 0.8 + 0.5));
        base += float3(1.5, 0.55, 0.05) * crack * crack;
        return base;
    }

    if (skin == 2) {
        // Arc: two spotlights sweeping across an electric blue conductor.
        float beam = exp(-pow((p.x - sin(time * 1.1) * 0.60) / 0.16, 2.0));
        float softBeam = exp(-pow((p.x - sin(time * 0.7 + 2.2) * 0.62) / 0.32, 2.0));
        float scan = 0.5 + 0.5 * sin(p.y * 20.0 - time * 6.0);

        float3 base = lerp(float3(0.02, 0.07, 0.30), float3(0.02, 0.55, 0.95),
                           saturate(p.y * 0.7 + 0.5));
        base += float3(0.35, 0.90, 1.0) * beam * 0.9;
        base += float3(0.10, 0.35, 0.65) * softBeam * 0.35;
        base += 0.05 * scan;
        return base;
    }

    if (skin == 3) {
        // Frost: crystalline facets under a pale blue surface.
        float facet = abs(sin(p.x * 16.0 + sin(p.y * 9.0) * 1.8) +
                          sin(p.y * 13.0 - p.x * 4.0));
        float glint = 1.0 - smoothstep(0.02, 0.28, facet);

        float3 base = lerp(float3(0.02, 0.18, 0.34), float3(0.62, 0.94, 1.0),
                           saturate(p.y * 0.8 + 0.5));
        base += float3(0.60, 0.95, 1.0) * glint * 0.22;
        return base;
    }

    // Prism: iridescent interference bands.
    float hue = 0.5 + 0.5 * sin(p.x * 2.5 + p.y * 3.0 + time * 1.4);
    float3 color = lerp(float3(0.95, 0.20, 0.65), float3(0.10, 0.85, 0.95), hue);
    color = lerp(color, float3(1.0, 0.80, 0.25), 0.5 + 0.5 * sin(p.y * 7.0 - time * 2.4));
    color += 0.10 * sin(p.x * 28.0 + time * 4.0);
    return color;
}

float3 skinEdge(int skin, float2 p, float distance, float time)
{
    float halo = exp(-max(distance, 0.0) * 12.0);
    float angle = atan2(p.y, p.x);

    if (skin == 0) {
        float mist = fbm(p * 3.0 + float2(time * 0.12, -time * 0.18));
        return halo * lerp(float3(0.10, 0.90, 0.65), float3(0.50, 0.40, 1.0), mist) * 0.7;
    }

    if (skin == 1) {
        float sparks = pow(max(0.0, sin(angle * 24.0 + time * 5.0 +
                                       valueNoise(p * 8.0) * 6.0)), 30.0);
        float tightHalo = exp(-max(distance, 0.0) * 20.0);
        return halo * float3(1.0, 0.35, 0.05) * 0.45 +
               tightHalo * float3(1.0, 0.75, 0.20) * sparks * 0.9;
    }

    if (skin == 2) {
        float bolts = pow(max(0.0, sin(angle * 18.0 + time * 9.0 +
                                       sin(angle * 7.0 - time) * 3.0)), 28.0);
        float sparks = pow(max(0.0, sin(angle * 43.0 - time * 6.0)), 60.0);
        float nearEdge = 1.0 - smoothstep(0.0, 0.11, max(distance, 0.0));
        return halo * float3(0.30, 0.85, 1.0) * 0.65 +
               nearEdge * (bolts * 0.75 + sparks * 0.45) * float3(0.70, 1.0, 1.0);
    }

    if (skin == 3) {
        float vapor = fbm(p * 4.0 + float2(time * 0.15, -time * 0.25));
        float wisps = smoothstep(0.42, 0.82, vapor) * exp(-max(distance, 0.0) * 4.0);
        return halo * float3(0.55, 0.90, 1.0) * 0.60 +
               wisps * float3(0.70, 0.95, 1.0) * 0.50;
    }

    float shimmer = 0.6 + 0.4 * sin(time * 4.0 + p.x * 11.0 + p.y * 13.0);
    float sparkle = pow(max(0.0, sin(p.x * 47.0 + p.y * 43.0 + time * 7.0)), 30.0);
    float3 iridescent = lerp(float3(1.0, 0.40, 0.80), float3(0.30, 0.90, 1.0),
                             0.5 + 0.5 * sin(time * 3.0 + p.y * 9.0));
    float nearEdge = 1.0 - smoothstep(0.0, 0.08, max(distance, 0.0));
    return halo * iridescent * shimmer * 0.70 +
           nearEdge * sparkle * float3(1.0, 1.0, 0.9) * 0.8;
}

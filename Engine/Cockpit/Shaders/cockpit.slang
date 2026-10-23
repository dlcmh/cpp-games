// Screen-space X-Wing cockpit, HUD, hyperspace streaks, and explosion flash.
// Drawn after the 3D pass with alpha blending.

struct CockpitUniforms {
    float2 resolution;
    float time;
    float phase;
    float phaseTime;
    float flash;
    float jump;
    float steer;
    float firing;
    float showCanopy;
    float showDashboard;
    float showHud;
    float glassTint;
    float hudScale;
    float2 padding;
};

[[vk::binding(0, 0)]]
ConstantBuffer<CockpitUniforms> cockpit;

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

float sdSegment(float2 p, float2 a, float2 b)
{
    float2 pa = p - a;
    float2 ba = b - a;
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    return length(pa - ba * h);
}

float sdRoundBox(float2 p, float2 halfSize, float radius)
{
    float2 q = abs(p) - halfSize + radius;
    return length(max(q, float2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

float lineMask(float distance, float width)
{
    float aa = max(fwidth(distance), 0.0008);
    return 1.0 - smoothstep(width, width + aa * 1.5, abs(distance));
}

float fillMask(float distance)
{
    float aa = max(fwidth(distance), 0.0008);
    return 1.0 - smoothstep(-aa, aa, distance);
}

void layer(inout float3 rgb, inout float alpha, float3 color, float mask)
{
    const float a = saturate(mask);
    rgb = lerp(rgb, color, a);
    alpha = saturate(alpha + a * (1.0 - alpha));
}

float3 hudAccent()
{
    int phase = int(cockpit.phase + 0.5);
    if (phase == 1) {
        return float3(0.25, 1.0, 0.45);
    }
    if (phase == 4) {
        return float3(1.0, 0.62, 0.18);
    }
    if (phase == 3) {
        return float3(0.50, 0.82, 1.0);
    }
    return float3(0.35, 0.92, 1.0);
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float aspect = cockpit.resolution.x / max(cockpit.resolution.y, 1.0);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    float3 rgb = float3(0.0);
    float alpha = 0.0;
    const float3 frameColor = float3(0.012, 0.016, 0.022);
    const float3 frameEdge = float3(0.10, 0.13, 0.16);
    const float3 accent = hudAccent();

    if (cockpit.showCanopy > 0.5) {
    // Canopy glass tint and subtle dust/scratches.
    const float canopy = smoothstep(0.55, 1.15, abs(p.x) * 0.6 + abs(p.y) * 0.9);
    layer(rgb, alpha, float3(0.012, 0.028, 0.042), canopy * cockpit.glassTint);

    // Upper diagonal canopy struts.
    layer(rgb, alpha, frameColor,
          lineMask(sdSegment(p, float2(-aspect, 1.02), float2(aspect, 1.02)), 0.018));
    const float leftStrut = sdSegment(p, float2(-aspect * 1.04, 1.16), float2(-0.54, 0.90));
    const float rightStrut = sdSegment(p, float2(aspect * 1.04, 1.16), float2(0.54, 0.90));
    layer(rgb, alpha, frameColor, lineMask(leftStrut, 0.038));
    layer(rgb, alpha, frameColor, lineMask(rightStrut, 0.038));
    layer(rgb, alpha, frameEdge, lineMask(leftStrut - 0.035, 0.008));
    layer(rgb, alpha, frameEdge, lineMask(rightStrut - 0.035, 0.008));
    layer(rgb, alpha, frameColor,
          lineMask(sdSegment(p, float2(-0.54, 0.90), float2(0.54, 0.90)), 0.018));
    layer(rgb, alpha, frameEdge,
          lineMask(sdSegment(p, float2(-0.54, 0.90), float2(0.54, 0.90)) - 0.012, 0.005));

    // Centre canopy spine and windshield side frames.
    layer(rgb, alpha, frameColor,
          lineMask(sdSegment(p, float2(0.0, 1.20), float2(0.0, 0.90)), 0.014));
    const float leftPillar = sdSegment(p, float2(-aspect * 0.88, 0.84), float2(-aspect * 0.95, 0.18));
    const float rightPillar = sdSegment(p, float2(aspect * 0.88, 0.84), float2(aspect * 0.95, 0.18));
    layer(rgb, alpha, frameColor, lineMask(leftPillar, 0.022));
    layer(rgb, alpha, frameColor, lineMask(rightPillar, 0.022));
    }

    if (cockpit.showDashboard > 0.5) {
    // Dashboard silhouette: a low, angular instrument binnacle.
    const float dashEdge = -0.64 + 0.10 * abs(p.x) + 0.018 * sin(p.x * 5.0);
    const float dashboard = 1.0 - smoothstep(dashEdge - 0.006, dashEdge + 0.006, p.y);
    layer(rgb, alpha, frameColor, dashboard);
    layer(rgb, alpha, frameEdge, (1.0 - smoothstep(0.0, 0.0045, abs(p.y - dashEdge))) * 0.30);
    layer(rgb, alpha, accent * 0.22,
          (1.0 - smoothstep(0.0, 0.020, abs(p.y - dashEdge + 0.018))) * 0.22);

    // Raised centre console with a targeting display.
    const float centreConsole = sdRoundBox(p - float2(0.0, -0.80), float2(0.30, 0.115), 0.045);
    layer(rgb, alpha, float3(0.018, 0.028, 0.032), fillMask(centreConsole));
    layer(rgb, alpha, frameEdge, lineMask(centreConsole, 0.010));

    const float display = sdRoundBox(p - float2(0.0, -0.80), float2(0.23, 0.078), 0.030);
    layer(rgb, alpha, float3(0.010, 0.075, 0.058), fillMask(display));
    const float scan = 0.55 + 0.45 * sin(p.y * 110.0 + cockpit.time * 5.0);
    layer(rgb, alpha, accent * 0.24, fillMask(display) * scan);
    layer(rgb, alpha, accent * 0.45,
          lineMask(sdRoundBox(p - float2(0.0, -0.80), float2(0.23, 0.078), 0.030), 0.002));

    // Side instrument panels with segmented bars.
    for (int side = -1; side <= 1; side += 2) {
        const float2 centre = float2(float(side) * 0.72, -0.79);
        const float panel = sdRoundBox(p - centre, float2(0.20, 0.078), 0.028);
        layer(rgb, alpha, float3(0.014, 0.025, 0.030), fillMask(panel));
        layer(rgb, alpha, frameEdge * 0.9, lineMask(panel, 0.010));
        for (int bar = 0; bar < 3; ++bar) {
            const float2 barPoint = p - centre - float2(-0.11 + float(bar) * 0.10, 0.0);
            const float barMask = sdRoundBox(barPoint, float2(0.021, 0.034), 0.008);
            const float lit = 0.45 + 0.55 * sin(cockpit.time * 4.0 + float(bar) +
                                                float(side) * 1.3);
            layer(rgb, alpha, accent * lit, fillMask(barMask));
        }
    }

    // Small status dots along the dash.
    for (int dot = 0; dot < 7; ++dot) {
        const float2 dotPoint = p - float2(-0.36 + float(dot) * 0.12, -0.93);
        const float dotMask = 1.0 - smoothstep(0.007, 0.012, length(dotPoint));
        const float pulse = 0.45 + 0.55 * sin(cockpit.time * 2.0 + float(dot) * 1.7);
        layer(rgb, alpha, accent * pulse, dotMask * 0.75);
    }
    }

    if (cockpit.showHud > 0.5) {
    const float2 hudP = p / max(cockpit.hudScale, 0.2);
    // HUD reticle.
    // Keep in sync with kReticleCenterY / kReticleSteerShift in CockpitState.hpp.
    const float2 reticleCentre = float2(-cockpit.steer * 0.035, 0.07);
    const float reticleRadius = length(hudP - reticleCentre);
    const float ring = 1.0 - smoothstep(0.0, 0.004, abs(reticleRadius - 0.078));
    const float innerRing = 1.0 - smoothstep(0.0, 0.003, abs(reticleRadius - 0.031));
    const float crossX = lineMask(hudP.x - reticleCentre.x, 0.0015) *
                         step(0.055, abs(hudP.y - reticleCentre.y)) * step(abs(hudP.y - reticleCentre.y), 0.12);
    const float crossY = lineMask(hudP.y - reticleCentre.y, 0.0015) *
                         step(0.055, abs(hudP.x - reticleCentre.x)) * step(abs(hudP.x - reticleCentre.x), 0.12);
    const float fireGlow = 0.6 + cockpit.firing * 0.8;
    const float hud = ring + innerRing * 0.7 + crossX + crossY;
    layer(rgb, alpha, accent * fireGlow, hud * 0.72);
    layer(rgb, alpha, float3(1.0), smoothstep(0.008, 0.0, reticleRadius) * 0.9);

    // Out-of-phase target brackets drift with steering.
    const float bracket = sdRoundBox(hudP - float2(-0.34 - cockpit.steer * 0.06, 0.32),
                                     float2(0.075, 0.050), 0.012);
    layer(rgb, alpha, accent * 0.55, lineMask(bracket, 0.004));
    }

    // Hyperspace: radial star streaks and a blue tunnel glow.
    if (cockpit.jump > 0.0) {
        const float angle = atan2(p.y, p.x);
        const float radius = length(p);
        const float streaks = pow(max(0.0, sin(angle * 62.0 + cockpit.time * 16.0)), 36.0) *
                              smoothstep(0.18, 1.05, radius);
        const float tunnel = smoothstep(0.55, 1.6, radius) * 0.16;
        layer(rgb, alpha, float3(0.35, 0.70, 1.0), streaks * cockpit.jump * 0.60);
        layer(rgb, alpha, float3(0.20, 0.45, 1.0), tunnel * cockpit.jump);
    }

    // Damage/explosion flash and final vignette.
    layer(rgb, alpha, float3(1.0, 0.64, 0.30), saturate(cockpit.flash) * 0.85);
    const float vignette = smoothstep(1.7, 0.4, length(p)) * 0.20;
    layer(rgb, alpha, float3(0.0), vignette);

    return float4(rgb, saturate(alpha));
}

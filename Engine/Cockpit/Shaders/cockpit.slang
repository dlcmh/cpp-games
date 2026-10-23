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


float sdCircle(float2 p, float radius)
{
    return length(p) - radius;
}

float sdRing(float2 p, float radius, float thickness)
{
    return abs(length(p) - radius) - thickness;
}

float dashboardTop(float x)
{
    return -0.33 + 0.115 * smoothstep(0.42, 1.32, abs(x));
}

float3 metalColor(float2 p)
{
    const float grain = hash21(floor(p * 220.0)) * 0.10;
    const float3 dark = float3(0.045, 0.050, 0.052);
    const float3 light = float3(0.27, 0.255, 0.225);
    return lerp(dark, light, saturate(p.y * 0.55 + 0.62)) * (0.88 + grain);
}

void drawStrut(inout float3 rgb, inout float alpha, float2 p, float2 a, float2 b,
               float width, float3 color)
{
    const float d = sdSegment(p, a, b);
    layer(rgb, alpha, color, fillMask(d - width));
    layer(rgb, alpha, min(color * 1.9, float3(1.0)), lineMask(d - width + 0.006, 0.003));
    layer(rgb, alpha, float3(0.0), lineMask(d - width - 0.004, 0.003) * 0.55);
}

void drawPanel(inout float3 rgb, inout float alpha, float d, float3 color)
{
    layer(rgb, alpha, color, fillMask(d));
    layer(rgb, alpha, float3(0.48, 0.45, 0.39), lineMask(d, 0.0035));
    layer(rgb, alpha, float3(0.0), lineMask(d + 0.010, 0.004) * 0.55);
}

void drawBolt(inout float3 rgb, inout float alpha, float2 p, float radius)
{
    const float body = sdCircle(p, radius);
    layer(rgb, alpha, float3(0.055, 0.055, 0.052), fillMask(body));
    layer(rgb, alpha, float3(0.56, 0.52, 0.45), lineMask(body, 0.0025));
    layer(rgb, alpha, float3(0.22, 0.20, 0.18), fillMask(sdCircle(p + float2(0.004, 0.004), radius * 0.35)));
}

void drawButton(inout float3 rgb, inout float alpha, float2 p, float2 centre, float radius,
                float3 color)
{
    const float body = sdCircle(p - centre, radius);
    layer(rgb, alpha, float3(0.035, 0.035, 0.032), fillMask(body));
    layer(rgb, alpha, float3(0.34, 0.31, 0.26), lineMask(body, 0.003));
    layer(rgb, alpha, color, fillMask(sdCircle(p - centre + float2(0.0, 0.003), radius * 0.58)));
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float aspect = cockpit.resolution.x / max(cockpit.resolution.y, 1.0);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    float3 rgb = float3(0.0);
    float alpha = 0.0;
    const float3 accent = hudAccent();
    const float3 frame = float3(0.16, 0.15, 0.135);
    const float3 trim = float3(0.40, 0.37, 0.31);
    const float3 shadow = float3(0.0);

    if (cockpit.showCanopy > 0.5) {
        // Tinted glass, then a faint reflected streak across the upper canopy.
        const float glass = smoothstep(0.58, 1.18, abs(p.x) * 0.55 + abs(p.y) * 0.85);
        layer(rgb, alpha, float3(0.012, 0.028, 0.040), glass * cockpit.glassTint);
        const float sheen = smoothstep(0.006, 0.0, abs(p.y - 0.82 + p.x * 0.16));
        layer(rgb, alpha, float3(0.10, 0.18, 0.22), sheen * 0.06);
        const float edgeSheen = smoothstep(0.003, 0.0, abs(p.x * 0.20 + p.y - 0.30));
        layer(rgb, alpha, float3(0.08, 0.14, 0.18), edgeSheen * 0.035);

        // Overhead canopy rail and the heavy A-pillars.
        drawStrut(rgb, alpha, p, float2(-aspect * 1.05, 0.955), float2(aspect * 1.05, 0.955),
                  0.050, frame);
        drawStrut(rgb, alpha, p, float2(-aspect * 1.05, 0.905), float2(aspect * 1.05, 0.905),
                  0.010, trim);
        drawStrut(rgb, alpha, p, float2(-aspect * 1.04, 1.14), float2(-0.55, -0.44),
                  0.078, float3(0.035, 0.034, 0.032));
        drawStrut(rgb, alpha, p, float2(aspect * 1.04, 1.14), float2(0.55, -0.44),
                  0.078, float3(0.035, 0.034, 0.032));
        drawStrut(rgb, alpha, p, float2(-aspect * 1.04, 1.14), float2(-0.55, -0.44),
                  0.060, frame);
        drawStrut(rgb, alpha, p, float2(aspect * 1.04, 1.14), float2(0.55, -0.44),
                  0.060, frame);
        drawStrut(rgb, alpha, p, float2(-aspect * 1.04, 1.12), float2(-0.58, -0.44),
                  0.007, trim);
        drawStrut(rgb, alpha, p, float2(aspect * 1.04, 1.12), float2(0.58, -0.44),
                  0.007, trim);

        // Side window rails.
        drawStrut(rgb, alpha, p, float2(-aspect * 0.93, 0.96), float2(-aspect * 0.99, -0.42),
                  0.028, frame);
        drawStrut(rgb, alpha, p, float2(aspect * 0.93, 0.96), float2(aspect * 0.99, -0.42),
                  0.028, frame);
        drawStrut(rgb, alpha, p, float2(-aspect * 0.94, 0.96), float2(-aspect * 1.00, -0.42),
                  0.004, trim);
        drawStrut(rgb, alpha, p, float2(aspect * 0.94, 0.96), float2(aspect * 1.00, -0.42),
                  0.004, trim);

        // Bolts down each A-pillar.
        for (int i = 0; i < 7; ++i) {
            const float u = 0.10 + float(i) * 0.125;
            const float2 left = lerp(float2(-aspect * 1.04, 1.14), float2(-0.55, -0.44), u);
            const float2 right = lerp(float2(aspect * 1.04, 1.14), float2(0.55, -0.44), u);
            drawBolt(rgb, alpha, p - left, 0.006);
            drawBolt(rgb, alpha, p - right, 0.006);
        }
    }

    if (cockpit.showDashboard > 0.5) {
        // The armoured dashboard slopes down toward the side consoles.
        const float dashTop = dashboardTop(p.x) + 0.008 * sin(p.x * 3.4);
        const float dash = 1.0 - smoothstep(dashTop - 0.004, dashTop + 0.004, p.y);
        const float depth = saturate((dashTop - p.y) / 0.62);
        const float3 panelBase = lerp(float3(0.20, 0.19, 0.17), float3(0.045, 0.048, 0.050),
                                      depth) * (0.92 + 0.10 * hash21(floor(p * 160.0)));
        layer(rgb, alpha, panelBase, dash);
        layer(rgb, alpha, trim, (1.0 - smoothstep(0.0, 0.004, abs(p.y - dashTop))) * 0.55);
        layer(rgb, alpha, shadow, (1.0 - smoothstep(0.0, 0.006, abs(p.y - dashTop + 0.018))) * 0.55);

        // Rivets along the upper dash lip.
        for (int i = 0; i < 15; ++i) {
            const float x = -0.84 + float(i) * 0.12;
            drawBolt(rgb, alpha, p - float2(x, dashboardTop(x) - 0.030), 0.005);
        }

        // Panel seams and side instrument bays.
        for (int side = -1; side <= 1; side += 2) {
            const float2 bay = float2(float(side) * 0.70, -0.58);
            drawPanel(rgb, alpha, sdRoundBox(p - bay, float2(0.245, 0.185), 0.035),
                      float3(0.13, 0.125, 0.112) * (0.8 + 0.2 * hash21(floor(p * 90.0))));
            layer(rgb, alpha, shadow, lineMask(sdRoundBox(p - bay, float2(0.245, 0.185), 0.035) - 0.012,
                                                0.003) * 0.35);

            // Small status screen, then two rows of guarded switches.
            const float2 bayScreen = bay + float2(0.0, 0.095);
            const float screenBox = sdRoundBox(p - bayScreen, float2(0.085, 0.032), 0.010);
            layer(rgb, alpha, float3(0.008, 0.05, 0.042), fillMask(screenBox));
            layer(rgb, alpha, accent * 0.35,
                  fillMask(screenBox) * (0.22 + 0.18 * sin(p.x * 70.0 + cockpit.time * 2.0)));
            layer(rgb, alpha, accent * 0.55, lineMask(screenBox, 0.002));
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 3; ++col) {
                    const float2 button = bay + float2(-0.14 + float(col) * 0.14,
                                                       -0.045 - float(row) * 0.095);
                    const float3 lamp = (row == 0 && col == 1) ? float3(1.0, 0.20, 0.10)
                                                              : accent * 0.75;
                    drawButton(rgb, alpha, p, button, 0.017, lamp);
                }
            }
            layer(rgb, alpha, float3(0.38, 0.34, 0.28),
                  fillMask(sdRoundBox(p - bay - float2(0.0, -0.135), float2(0.19, 0.006), 0.003)));
        }

        // Wraparound side consoles. They sit outboard of the A-pillars so the
        // canopy stays in front, and rise with the dash edge.
        for (int side = -1; side <= 1; side += 2) {
            const float sideX = abs(p.x);
            const float2 consoleCentre = float2(float(side) * 1.43, -0.55);
            const float wrapPanel = sdRoundBox(p - consoleCentre, float2(0.19, 0.36), 0.035);
            drawPanel(rgb, alpha, wrapPanel,
                      float3(0.105, 0.100, 0.090) * (0.86 + 0.14 * hash21(floor(p * 70.0))));
            layer(rgb, alpha, trim, lineMask(wrapPanel, 0.003));
            layer(rgb, alpha, shadow,
                  lineMask(wrapPanel - 0.012, 0.003) * 0.35);

            // Recessed side status screen.
            const float2 sideScreen = consoleCentre + float2(0.0, 0.19);
            const float sideScreenD = sdRoundBox(p - sideScreen, float2(0.105, 0.042), 0.010);
            layer(rgb, alpha, float3(0.006, 0.045, 0.040), fillMask(sideScreenD));
            layer(rgb, alpha, accent * 0.35,
                  fillMask(sideScreenD) * (0.20 + 0.16 * sin(p.y * 80.0 + cockpit.time * 3.0)));
            layer(rgb, alpha, accent * 0.55, lineMask(sideScreenD, 0.002));

            // Guarded switches and a row of lamps.
            drawButton(rgb, alpha, p, consoleCentre + float2(0.0, 0.08), 0.022,
                       float3(0.95, 0.16, 0.10));
            drawButton(rgb, alpha, p, consoleCentre + float2(0.0, 0.00), 0.016,
                       float3(1.0, 0.60, 0.16));
            for (int lamp = 0; lamp < 3; ++lamp) {
                drawButton(rgb, alpha, p,
                           consoleCentre + float2(-0.10 + float(lamp) * 0.10, -0.13), 0.011,
                           accent * (0.55 + 0.25 * sin(cockpit.time * 2.0 + float(lamp))));
            }
            layer(rgb, alpha, float3(0.38, 0.34, 0.28),
                  fillMask(sdRoundBox(p - consoleCentre - float2(0.0, -0.245),
                                      float2(0.14, 0.006), 0.003)));

        }

        // Raised centre binnacle.
        const float2 binnacle = float2(0.0, -0.53);
        drawPanel(rgb, alpha, sdRoundBox(p - binnacle, float2(0.345, 0.145), 0.035),
                  float3(0.155, 0.145, 0.125));
        const float binnacleLip = sdRoundBox(p - binnacle, float2(0.30, 0.115), 0.025);
        layer(rgb, alpha, float3(0.04, 0.045, 0.045), fillMask(binnacleLip));
        layer(rgb, alpha, trim, lineMask(binnacleLip, 0.003));

        // Main targeting display.
        const float2 displayCentre = float2(0.0, -0.545);
        const float display = sdRoundBox(p - displayCentre, float2(0.25, 0.085), 0.022);
        layer(rgb, alpha, float3(0.006, 0.055, 0.040), fillMask(display));
        layer(rgb, alpha, float3(0.0, 0.10, 0.065),
              fillMask(display) * (0.30 + 0.18 * sin(p.y * 130.0 + cockpit.time * 3.0)));
        layer(rgb, alpha, accent * 0.55, lineMask(display, 0.0022));

        // Radar sweep and target wireframe inside the main display.
        const float2 target = p - displayCentre;
        const float scope = sdCircle(target, 0.056);
        layer(rgb, alpha, accent * 0.30, lineMask(scope, 0.0022));
        layer(rgb, alpha, accent * 0.22, lineMask(sdCircle(target, 0.030), 0.0016));
        const float sweepAngle = cockpit.time * 1.8;
        const float sweep = lineMask(sdSegment(target, float2(0.0), float2(cos(sweepAngle), sin(sweepAngle)) * 0.056), 0.0018);
        layer(rgb, alpha, accent * 0.65, sweep);
        const float targetBlip = sdCircle(target - float2(0.022, 0.010), 0.006);
        layer(rgb, alpha, float3(1.0, 0.30, 0.14), fillMask(targetBlip));
        for (int i = 0; i < 4; ++i) {
            const float2 a = float2(-0.11 + float(i) * 0.073, -0.065);
            layer(rgb, alpha, accent * 0.45, fillMask(sdRoundBox(p - displayCentre - a,
                                                                float2(0.018, 0.004), 0.002)));
        }
        // A tiny wireframe capital ship and its target brackets.
        const float2 ship = target - float2(0.0, 0.002);
        const float hull = min(sdSegment(ship, float2(-0.040, 0.018), float2(0.040, 0.018)),
                               min(sdSegment(ship, float2(-0.040, 0.018), float2(0.0, -0.020)),
                                   sdSegment(ship, float2(0.040, 0.018), float2(0.0, -0.020))));
        layer(rgb, alpha, accent * 0.75, lineMask(hull, 0.0018));
        layer(rgb, alpha, accent * 0.55,
              lineMask(sdSegment(ship, float2(-0.050, 0.030), float2(-0.050, -0.018)), 0.0013));
        layer(rgb, alpha, accent * 0.55,
              lineMask(sdSegment(ship, float2(0.050, 0.030), float2(0.050, -0.018)), 0.0013));

        // Circular radar on the left and the large speed gauge on the right.
        const float2 radarCentre = float2(-0.70, -0.60);
        drawPanel(rgb, alpha, sdCircle(radarCentre, 0.125), float3(0.05, 0.05, 0.048));
        layer(rgb, alpha, trim, lineMask(sdCircle(radarCentre, 0.125), 0.004));
        layer(rgb, alpha, float3(0.01, 0.07, 0.055), fillMask(sdCircle(radarCentre, 0.108)));
        layer(rgb, alpha, accent * 0.35, lineMask(sdCircle(radarCentre, 0.108), 0.002));
        const float2 radarP = p - radarCentre;
        layer(rgb, alpha, accent * 0.45,
              lineMask(sdSegment(radarP, float2(0.0), float2(cos(cockpit.time * 2.2), sin(cockpit.time * 2.2)) * 0.10), 0.002));
        layer(rgb, alpha, accent * 0.28, lineMask(sdCircle(radarP, 0.052), 0.0015));
        for (int i = 0; i < 4; ++i) {
            const float angle = float(i) * 1.5708 + 0.785;
            layer(rgb, alpha, accent * 0.55,
                  fillMask(sdCircle(radarP - float2(cos(angle), sin(angle)) * 0.072, 0.004)));
        }

        const float2 gaugeCentre = float2(0.70, -0.60);
        drawPanel(rgb, alpha, sdCircle(gaugeCentre, 0.125), float3(0.06, 0.055, 0.050));
        layer(rgb, alpha, trim, lineMask(sdCircle(gaugeCentre, 0.125), 0.004));
        layer(rgb, alpha, float3(0.015, 0.018, 0.018), fillMask(sdCircle(gaugeCentre, 0.108)));
        layer(rgb, alpha, accent * 0.55, lineMask(sdCircle(gaugeCentre, 0.096), 0.002));
        for (int i = 0; i < 24; ++i) {
            const float angle = float(i) / 24.0 * 6.2831853;
            const float2 tickA = gaugeCentre + float2(cos(angle), sin(angle)) * 0.082;
            const float2 tickB = gaugeCentre + float2(cos(angle), sin(angle)) * 0.094;
            const float lit = i < 17 ? 1.0 : 0.35;
            layer(rgb, alpha, accent * lit * 0.75, lineMask(sdSegment(p, tickA, tickB), 0.0018));
        }
        const float needleAngle = -2.4 + 2.4 * saturate(sin(cockpit.time * 0.7) * 0.5 + 0.5);
        layer(rgb, alpha, float3(1.0, 0.64, 0.18),
              lineMask(sdSegment(p - gaugeCentre, float2(0.0), float2(cos(needleAngle), sin(needleAngle)) * 0.074), 0.0025));
        layer(rgb, alpha, float3(0.035, 0.030, 0.025), fillMask(sdCircle(p - gaugeCentre, 0.014)));
        layer(rgb, alpha, float3(0.80, 0.73, 0.58), lineMask(sdCircle(p - gaugeCentre, 0.014), 0.002));

        // Rows of switches and indicator lamps along the lower dash.
        for (int i = 0; i < 11; ++i) {
            const float x = -0.57 + float(i) * 0.115;
            const float3 lamp = (i % 4 == 0) ? float3(1.0, 0.16, 0.10)
                                             : (i % 3 == 0 ? float3(1.0, 0.63, 0.16)
                                                           : float3(0.25, 0.95, 0.55));
            drawButton(rgb, alpha, p, float2(x, -0.86), 0.013, lamp);
            layer(rgb, alpha, float3(0.42, 0.38, 0.31),
                  fillMask(sdRoundBox(p - float2(x, -0.90), float2(0.019, 0.009), 0.003)));
        }

        for (int i = 0; i < 6; ++i) {
            const float x = -0.26 + float(i) * 0.104;
            layer(rgb, alpha, accent * 0.55,
                  fillMask(sdRoundBox(p - float2(x, -0.715), float2(0.026, 0.004), 0.002)));
        }

    }

    if (cockpit.showHud > 0.5) {
        const float2 hudP = p / max(cockpit.hudScale, 0.2);

        // Top status strip: shield ring, white hull bar, red damage segment.
        const float2 healthCentre = float2(0.0, 0.80);
        const float health = sdRoundBox(hudP - healthCentre, float2(0.31, 0.026), 0.012);
        layer(rgb, alpha, float3(0.0, 0.0, 0.0), fillMask(health) * 0.55);
        layer(rgb, alpha, float3(0.72, 0.70, 0.63),
              fillMask(sdRoundBox(hudP - healthCentre, float2(0.286, 0.014), 0.007)) * 0.90);
        layer(rgb, alpha, float3(0.95, 0.12, 0.08),
              fillMask(sdRoundBox(hudP - healthCentre - float2(0.20, 0.0), float2(0.076, 0.014), 0.007)) * 0.92);
        layer(rgb, alpha, float3(0.95, 0.75, 0.30), lineMask(health, 0.0022));
        layer(rgb, alpha, float3(0.80, 0.75, 0.62),
              lineMask(sdCircle(hudP - float2(-0.355, 0.80), 0.022), 0.003));
        layer(rgb, alpha, float3(0.92, 0.88, 0.72),
              fillMask(sdCircle(hudP - float2(-0.355, 0.80), 0.005)));

        for (int i = 0; i < 5; ++i) {
            const float x = -0.12 + float(i) * 0.060;
            layer(rgb, alpha, float3(0.95, 0.48, 0.16),
                  fillMask(sdRoundBox(hudP - float2(x, 0.725), float2(0.016, 0.004), 0.002)) * 0.75);
        }

        // Reticle.
        const float2 reticleCentre = float2(-cockpit.steer * 0.035, 0.07);
        const float reticleRadius = length(hudP - reticleCentre);
        const float ring = 1.0 - smoothstep(0.0, 0.004, abs(reticleRadius - 0.078));
        const float innerRing = 1.0 - smoothstep(0.0, 0.003, abs(reticleRadius - 0.031));
        const float crossX = lineMask(hudP.x - reticleCentre.x, 0.0015) *
                             step(0.055, abs(hudP.y - reticleCentre.y)) *
                             step(abs(hudP.y - reticleCentre.y), 0.12);
        const float crossY = lineMask(hudP.y - reticleCentre.y, 0.0015) *
                             step(0.055, abs(hudP.x - reticleCentre.x)) *
                             step(abs(hudP.x - reticleCentre.x), 0.12);
        for (int i = 0; i < 12; ++i) {
            const float angle = float(i) / 12.0 * 6.2831853;
            const float2 tickA = reticleCentre + float2(cos(angle), sin(angle)) * 0.086;
            const float2 tickB = reticleCentre + float2(cos(angle), sin(angle)) * 0.096;
            layer(rgb, alpha, accent * 0.55, lineMask(sdSegment(hudP, tickA, tickB), 0.0014));
        }
        const float fireGlow = 0.6 + cockpit.firing * 0.8;
        const float hud = ring + innerRing * 0.7 + crossX + crossY;
        layer(rgb, alpha, accent * fireGlow, hud * 0.78);
        layer(rgb, alpha, float3(1.0), smoothstep(0.008, 0.0, reticleRadius) * 0.9);
    }

    if (cockpit.jump > 0.0) {
        const float angle = atan2(p.y, p.x);
        const float radius = length(p);
        const float streaks = pow(max(0.0, sin(angle * 62.0 + cockpit.time * 16.0)), 36.0) *
                              smoothstep(0.18, 1.05, radius);
        const float tunnel = smoothstep(0.55, 1.6, radius) * 0.16;
        layer(rgb, alpha, float3(0.35, 0.70, 1.0), streaks * cockpit.jump * 0.60);
        layer(rgb, alpha, float3(0.20, 0.45, 1.0), tunnel * cockpit.jump);
    }

    layer(rgb, alpha, float3(1.0, 0.64, 0.30), saturate(cockpit.flash) * 0.85);
    const float vignette = smoothstep(1.7, 0.4, length(p)) * 0.20;
    layer(rgb, alpha, float3(0.0), vignette);

    return float4(rgb, saturate(alpha));
}

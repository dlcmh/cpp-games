#include "common.slang"
#include "skins.slang"

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

float3 background(float2 uv, float2 p, float aspect)
{
    float3 color = lerp(float3(0.008, 0.012, 0.030), float3(0.025, 0.045, 0.090),
                        saturate(1.0 - length(p) * 0.35));

    float3 accentA = accentColor(int(uniforms.skinA + 0.5));
    float3 accentB = accentColor(int(uniforms.skinB + 0.5));
    float2 centerA = float2(-aspect * 0.5, 0.0);
    float2 centerB = float2(aspect * 0.5, 0.0);
    color += accentA * exp(-length(p - centerA) * 2.3) * 0.055;
    color += accentB * exp(-length(p - centerB) * 2.3) * 0.055;

    float2 starCell = floor(uv * uniforms.resolution / 3.0);
    float star = step(0.9987, hash21(starCell));
    float twinkle = 0.55 + 0.45 * sin(uniforms.time * 1.7 + hash21(starCell) * 30.0);
    color += float3(0.55, 0.75, 0.90) * star * twinkle * 0.20;

    float divider = 1.0 - smoothstep(0.0, 0.0025, abs(uv.x - 0.5));
    color += float3(0.30, 0.60, 0.80) * divider * 0.06;
    color *= lerp(0.70, 1.0, smoothstep(1.85, 0.35, length(p)));
    return color;
}

float3 shadeCharacter(float2 uv, float aspect, float3 color)
{
    int selected = int(uniforms.selectedIndex + 0.5);
    int side = uv.x < 0.5 ? 0 : 1;
    bool isSelected = side == selected;

    // Each half gets its own square-ish coordinate system. Using half the
    // aspect ratio keeps the glyph geometry circular on wide displays.
    float2 cell = float2(side == 0 ? uv.x * 2.0 : (uv.x - 0.5) * 2.0, uv.y);
    float2 cellCentered = cell * 2.0 - 1.0;
    cellCentered.x *= aspect * 0.5;

    float angle = side == 0 ? uniforms.angleA : uniforms.angleB;
    int skin = int(side == 0 ? uniforms.skinA : uniforms.skinB);

    float2 glyphPoint = rotate2(cellCentered, angle);
    glyphPoint *= (isSelected ? 1.16 : 1.04) * 1.12;

    float distance = side == 0 ? sdLetterA(glyphPoint) : sdLetterB(glyphPoint);
    float aa = max(fwidth(distance), 0.0008);
    float fill = 1.0 - smoothstep(-aa, aa, distance);
    float rim = fill * exp(-max(-distance, 0.0) * 58.0);

    float3 ink = skinFill(skin, glyphPoint, uniforms.time);
    float3 edge = skinEdge(skin, glyphPoint, distance, uniforms.time);
    float3 accent = accentColor(skin);

    color += edge;
    color = lerp(color, ink, fill);
    color += rim * float3(0.85, 1.0, 1.0) * 0.26;

    // The selector only encloses the row of skin pips. It stays in unrotated
    // cell space so the UI is stable while the glyph spins.
    float2 selectorCenter = cellCentered - float2(0.0, -0.84);
    float selectorDistance = sdRoundBox(selectorCenter, float2(0.295, 0.052), 0.048);
    float selectorLine = 1.0 - smoothstep(0.0015, 0.0045, abs(selectorDistance));
    color += selectorLine * accent * (isSelected ? 0.50 : 0.09);

    float selectorGlow =
        1.0 - smoothstep(0.0, 0.08, max(selectorDistance, 0.0));
    color += accent * selectorGlow * (isSelected ? 0.08 : 0.015);

    for (int pip = 0; pip < 5; ++pip) {
        float2 pipPoint = cellCentered - float2(float(pip - 2) * 0.105, -0.84);
        float distanceToPip = length(pipPoint);
        float radius = (pip == skin) ? 0.022 : 0.012;
        float pipMask = 1.0 - smoothstep(radius, radius + 0.006, distanceToPip);
        color = lerp(color, accentColor(pip) * (pip == skin ? 1.0 : 0.55),
                     pipMask * (pip == skin ? 0.90 : 0.45));
        if (pip == skin) {
            color += accentColor(pip) * exp(-max(distanceToPip - radius, 0.0) * 60.0) * 0.22;
        }
    }

    return color;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    float aspect = uniforms.resolution.x / max(uniforms.resolution.y, 1.0);
    float2 p = input.uv * 2.0 - 1.0;
    p.x *= aspect;

    float3 color = background(input.uv, p, aspect);
    color = shadeCharacter(input.uv, aspect, color);
    return float4(color, 1.0);
}

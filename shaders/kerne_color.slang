// KerneTile: Color Space & Porter-Duff Compositing Library
// IEC 61966-2-1 sRGB Transfer Function & Premultiplied Alpha Over Operator

#ifndef KERNETILE_COLOR_SLANG
#define KERNETILE_COLOR_SLANG

// IEC 61966-2-1 传递函数常量
static const float SRGB_THRESHOLD_LIN = 0.0031308;
static const float SRGB_THRESHOLD_ENC = 0.04045;

// sRGB -> 线性光空间 (单通道)
float srgbToLinear(float c)
{
    c = saturate(c);
    if (c <= SRGB_THRESHOLD_ENC) {
        return c / 12.92;
    }
    return pow((c + 0.055) / 1.055, 2.4);
}

float3 srgbToLinear(float3 c)
{
    return float3(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b));
}

// 线性光空间 -> sRGB (单通道)
float linearToSrgb(float c)
{
    c = saturate(c);
    if (c <= SRGB_THRESHOLD_LIN) {
        return c * 12.92;
    }
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

float3 linearToSrgb(float3 c)
{
    return float3(linearToSrgb(c.r), linearToSrgb(c.g), linearToSrgb(c.b));
}

// 预乘色域合法性钳制 (防止非法 RGB > A)
float4 clampPremul(float4 color)
{
    float a = saturate(color.a);
    float r = clamp(color.r, 0.0, a);
    float g = clamp(color.g, 0.0, a);
    float b = clamp(color.b, 0.0, a);
    return float4(r, g, b, a);
}

// Porter-Duff Source-Over 预乘合成:
// Co = Ca + Cb * (1 - Aa)
float4 blendOver(float4 src, float4 dst)
{
    float one_minus_src_a = 1.0 - src.a;
    return float4(
        src.rgb + dst.rgb * one_minus_src_a,
        src.a + dst.a * one_minus_src_a
    );
}

#endif // KERNETILE_COLOR_SLANG

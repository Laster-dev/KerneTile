// KerneTile: Constant-Time Analytical Gaussian Shadow Library
// Implements Abramowitz & Stegun 4th-order rational erf approximation & Squircle correction

#ifndef KERNETILE_SHADOW_SLANG
#define KERNETILE_SHADOW_SLANG

// Abramowitz & Stegun 4阶误差函数有理逼近
// 最大绝对误差: 8.198e-4 (约 0.21 LSB @ 8-bit)
float erfApprox(float x)
{
    float ax = abs(x);
    float poly = 1.0 + 0.278393 * ax + 0.230389 * ax * ax + 0.078108 * (ax * ax * ax * ax);
    float poly4 = poly * poly * poly * poly;
    float result = 1.0 - 1.0 / poly4;
    return (x >= 0.0) ? result : -result;
}

// 一维高斯累积分布函数 (CDF)
// F(x) = 0.5 * (1 + erf(x / (sigma * sqrt(2))))
float gaussianCDF(float x, float sigma)
{
    if (sigma <= 1e-4) {
        return (x >= 0.0) ? 1.0 : 0.0;
    }
    // 1.0 / sqrt(2) ≈ 0.7071067811865475
    return 0.5 * (1.0 + erfApprox(x * (0.70710678f / sigma)));
}

// Evan Wallace 常数时间解析高斯圆角阴影求值
// p: 采样点物理像素坐标
// box_min, box_max: 投影源矩形几何包围盒
// radius: 圆角半径
// sigma: 高斯模糊标准差 (shadow_blur)
// spread: 阴影外扩距离
float evaluateAnalyticShadow(
    float2 p, 
    float2 box_min, 
    float2 box_max, 
    float radius, 
    float sigma,
    float spread)
{
    if (sigma <= 0.0 && spread <= 0.0) {
        return 0.0;
    }

    // 1. 外扩 (Spread) 影响几何尺寸与有效圆角
    box_min -= float2(spread, spread);
    box_max += float2(spread, spread);
    radius = max(0.0, radius + spread);

    float2 half_size = (box_max - box_min) * 0.5;
    radius = min(radius, min(half_size.x, half_size.y));

    // 2. 直角矩形解析高斯外积积分
    float fx = gaussianCDF(box_max.x - p.x, sigma) - gaussianCDF(box_min.x - p.x, sigma);
    float fy = gaussianCDF(box_max.y - p.y, sigma) - gaussianCDF(box_min.y - p.y, sigma);
    float shadow = fx * fy;

    // 3. 圆角积分补偿与大半径 Squircle (超椭圆) 修正
    if (radius > 0.0) {
        float2 center = (box_min + box_max) * 0.5;
        float2 q = abs(p - center) - (half_size - float2(radius, radius));

        // 仅在角部象限应用修正
        if (q.x > 0.0 && q.y > 0.0) {
            float corner_d = length(q) - radius;
            
            // Raph Levien Squircle 修正: 随模糊半径动态调整指数与长轴峰值衰减
            float p_exp = 2.0 + 2.0 * saturate(sigma / max(1.0, radius));
            float squircle_d = pow(pow(max(0.0, q.x / radius), p_exp) + pow(max(0.0, q.y / radius), p_exp), 1.0 / p_exp) * radius - radius;
            float effective_d = lerp(corner_d, squircle_d, saturate(sigma / (2.0 * radius)));

            float corner_factor = gaussianCDF(-effective_d, sigma);
            shadow = min(shadow, corner_factor);
        }
    }

    return saturate(shadow);
}

#endif // KERNETILE_SHADOW_SLANG

// KerneTile: 2D Signed Distance Field (SDF) Library
// Implements Inigo Quilez exact branchless rounded box and stroke operators

#ifndef KERNETILE_SDF_SLANG
#define KERNETILE_SDF_SLANG

// Inigo Quilez 2D 精确圆角矩形 SDF
// p: 当前像素物理坐标
// center: 矩形中心点 (min + max) * 0.5
// half_size: 矩形半宽高 (max - min) * 0.5
// radii: float4(左上, 右上, 右下, 左下) 独立四圆角
float sdRoundedBox(float2 p, float2 center, float2 half_size, float4 radii)
{
    float2 s = p - center;

    // 根据象限对称选取对应的独立圆角半径
    // 顺时针: x>=0, y>=0 (右下: z); x>=0, y<0 (右上: y); x<0, y>=0 (左下: w); x<0, y<0 (左上: x)
    float r = (s.x >= 0.0) ? 
              ((s.y >= 0.0) ? radii.z : radii.y) : 
              ((s.y >= 0.0) ? radii.w : radii.x);

    // 确保圆角半径不超过矩形尺寸半长
    r = min(r, min(half_size.x, half_size.y));

    // 局部象限折叠并平移至内缩角点
    float2 q = abs(s) - (half_size - float2(r, r));

    // 无分支欧氏有向距离
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

// 环形折叠描边算子 (等宽边框)
// d: 基础几何体 SDF 距离
// stroke_width: 描边宽度 (半径 t = stroke_width * 0.5)
float opRing(float d, float stroke_width)
{
    return abs(d) - stroke_width * 0.5;
}

// 多项式平滑最小值 (平滑融合算子)
float smoothMin(float a, float b, float k)
{
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k * 0.25;
}

// 亚像素平滑抗锯齿覆盖率计算 (1 像素过渡带)
// d < -0.5: 完全在内部 (coverage = 1.0)
// d > +0.5: 完全在外部 (coverage = 0.0)
float evaluateCoverage(float d)
{
    return saturate(0.5 - d);
}

#endif // KERNETILE_SDF_SLANG

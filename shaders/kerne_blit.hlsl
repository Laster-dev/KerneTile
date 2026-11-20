// KerneTile: Full-Screen Quad Blit Shader
// Transfers offscreen UNORM storage texture to sRGB Swapchain backbuffer

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

// 全屏大三角形无缓冲顶点生成
VSOutput vs_blit(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    // vertex_id: 0 -> (0, 0), 1 -> (2, 0), 2 -> (0, 2)
    output.uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.position = float4(output.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return output;
}

Texture2D<float4> g_OffscreenTexture : register(t0);
SamplerState      g_PointSampler     : register(s0);

float4 ps_blit(VSOutput input) : SV_Target
{
    // 计算着色器内部已完成 IEC 61966-2-1 传递函数编码，此处直接无损采样搬运
    return g_OffscreenTexture.Sample(g_PointSampler, input.uv);
}

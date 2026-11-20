// KerneTile: Dual Kawase Downsample Compute Shader
// Implements Marius Bjorge (SIGGRAPH 2015) Dual Kawase downsampling filter.
// Utilizes 4 bilinear sample taps with half-diagonal offsets + center sample.
// Center weight = 4.0, 4 corners weight = 1.0, total = 8.0, normalized with * 0.125.

cbuffer BlurUniforms : register(b0)
{
    float2 g_TexelSize; // 1.0 / input_width, 1.0 / input_height
    float  g_Offset;    // Blur step offset scale (typically 1.0)
    float  g_Padding;   // 16-byte alignment
};

Texture2D<float4>   g_Input         : register(t0);
SamplerState        g_LinearSampler : register(s0);
RWTexture2D<float4> g_Output        : register(u0);

[numthreads(16, 16, 1)]
void cs_kawase_down(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint2 out_pixel = dispatch_thread_id.xy;
    uint out_width, out_height;
    g_Output.GetDimensions(out_width, out_height);

    if (out_pixel.x >= out_width || out_pixel.y >= out_height)
    {
        return;
    }

    // Normalized UV corresponding to the center of the destination pixel
    // Output resolution is half of input, so dst UV maps with factor of 2.0 * g_TexelSize
    float2 uv = (float2(out_pixel) + 0.5) * (g_TexelSize * 2.0);
    float2 half_pixel = g_TexelSize * g_Offset;

    // Dual Kawase downsample sampling pattern:
    // Center point weighted 4.0
    float4 sum = g_Input.SampleLevel(g_LinearSampler, uv, 0.0) * 4.0;

    // 4 diagonal corners weighted 1.0 each
    sum += g_Input.SampleLevel(g_LinearSampler, uv - half_pixel, 0.0);
    sum += g_Input.SampleLevel(g_LinearSampler, uv + half_pixel, 0.0);
    sum += g_Input.SampleLevel(g_LinearSampler, uv + float2(half_pixel.x, -half_pixel.y), 0.0);
    sum += g_Input.SampleLevel(g_LinearSampler, uv + float2(-half_pixel.x, half_pixel.y), 0.0);

    // Total weight sum = 4 + 1 + 1 + 1 + 1 = 8.0
    g_Output[out_pixel] = sum * 0.125;
}

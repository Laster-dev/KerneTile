#include "kernetile/fx/dual_kawase_blur.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace kernetile::fx {

namespace {

inline uint32_t get_quality_levels(const BlurParams& params) {
    if (params.custom_levels > 0) {
        return std::clamp(params.custom_levels, 1u, 6u);
    }
    switch (params.quality) {
        case BlurQuality::Low:    return 1u;
        case BlurQuality::Medium: return 2u;
        case BlurQuality::High:   return 3u;
        default:                  return 2u;
    }
}

} // namespace

std::vector<BlurPassDesc> DualKawaseBlur::build_pass_plan(
    uint32_t width,
    uint32_t height,
    const BlurParams& params)
{
    std::vector<BlurPassDesc> passes;
    if (width == 0 || height == 0) {
        return passes;
    }

    uint32_t levels = get_quality_levels(params);

    // 金字塔各级分辨率：mip_dims[0] 为输入原图，mip_dims[k] 为第 k 级降采样纹理
    struct MipDim {
        uint32_t w{0};
        uint32_t h{0};
    };
    std::vector<MipDim> mip_dims(levels + 1);
    mip_dims[0] = {width, height};

    for (uint32_t i = 1; i <= levels; ++i) {
        mip_dims[i].w = std::max(1u, mip_dims[i - 1].w / 2);
        mip_dims[i].h = std::max(1u, mip_dims[i - 1].h / 2);
    }

    passes.reserve(levels * 2);
    uint32_t pass_index = 0;

    // 1. 降采样通道 (从原图逐步降采样至金字塔深层)
    for (uint32_t i = 0; i < levels; ++i) {
        BlurPassDesc desc{};
        desc.pass_index = pass_index++;
        desc.is_downsample = true;
        desc.input_width = mip_dims[i].w;
        desc.input_height = mip_dims[i].h;
        desc.output_width = mip_dims[i + 1].w;
        desc.output_height = mip_dims[i + 1].h;
        desc.pyramid_level = i;

        float offset = (1.0f + 0.5f * static_cast<float>(i)) * params.offset_scale;
        desc.uniforms.texel_size_x = 1.0f / static_cast<float>(desc.input_width);
        desc.uniforms.texel_size_y = 1.0f / static_cast<float>(desc.input_height);
        desc.uniforms.offset = offset;
        desc.uniforms.padding = 0.0f;

        passes.push_back(desc);
    }

    // 2. 升采样通道 (从金字塔深层逐步升采样恢复至全分辨率)
    for (int32_t i = static_cast<int32_t>(levels) - 1; i >= 0; --i) {
        uint32_t in_lvl = static_cast<uint32_t>(i + 1);
        uint32_t out_lvl = static_cast<uint32_t>(i);

        BlurPassDesc desc{};
        desc.pass_index = pass_index++;
        desc.is_downsample = false;
        desc.input_width = mip_dims[in_lvl].w;
        desc.input_height = mip_dims[in_lvl].h;
        desc.output_width = mip_dims[out_lvl].w;
        desc.output_height = mip_dims[out_lvl].h;
        desc.pyramid_level = out_lvl;

        float offset = (1.0f + 0.5f * static_cast<float>(i)) * params.offset_scale;
        desc.uniforms.texel_size_x = 1.0f / static_cast<float>(desc.input_width);
        desc.uniforms.texel_size_y = 1.0f / static_cast<float>(desc.input_height);
        desc.uniforms.offset = offset;
        desc.uniforms.padding = 0.0f;

        passes.push_back(desc);
    }

    return passes;
}

size_t DualKawaseBlur::calculate_pyramid_vram_bytes(
    uint32_t width,
    uint32_t height,
    const BlurParams& params)
{
    if (width == 0 || height == 0) return 0;

    uint32_t levels = get_quality_levels(params);
    size_t total_bytes = 0;

    uint32_t cur_w = width;
    uint32_t cur_h = height;

    // 统计各降采样中间层尺寸 (每个像素 4 字节 RGBA8)
    for (uint32_t i = 1; i <= levels; ++i) {
        cur_w = std::max(1u, cur_w / 2);
        cur_h = std::max(1u, cur_h / 2);
        total_bytes += static_cast<size_t>(cur_w) * cur_h * 4;
    }

    return total_bytes;
}

void DualKawaseBlur::sample_bilinear_rgba(
    const uint8_t* src,
    uint32_t w,
    uint32_t h,
    float u,
    float v,
    float out_color[4])
{
    // ClampToEdge 寻址模式保护
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float px = u * static_cast<float>(w) - 0.5f;
    float py = v * static_cast<float>(h) - 0.5f;

    int32_t x0 = static_cast<int32_t>(std::floor(px));
    int32_t y0 = static_cast<int32_t>(std::floor(py));
    int32_t x1 = x0 + 1;
    int32_t y1 = y0 + 1;

    float fx = px - static_cast<float>(x0);
    float fy = py - static_cast<float>(y0);

    // 边界安全 clamp
    int32_t cx0 = std::clamp(x0, 0, static_cast<int32_t>(w) - 1);
    int32_t cx1 = std::clamp(x1, 0, static_cast<int32_t>(w) - 1);
    int32_t cy0 = std::clamp(y0, 0, static_cast<int32_t>(h) - 1);
    int32_t cy1 = std::clamp(y1, 0, static_cast<int32_t>(h) - 1);

    auto get_pixel = [&](int32_t x, int32_t y, float rgba[4]) {
        size_t idx = (static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 4;
        rgba[0] = static_cast<float>(src[idx + 0]);
        rgba[1] = static_cast<float>(src[idx + 1]);
        rgba[2] = static_cast<float>(src[idx + 2]);
        rgba[3] = static_cast<float>(src[idx + 3]);
    };

    float p00[4], p10[4], p01[4], p11[4];
    get_pixel(cx0, cy0, p00);
    get_pixel(cx1, cy0, p10);
    get_pixel(cx0, cy1, p01);
    get_pixel(cx1, cy1, p11);

    float w00 = (1.0f - fx) * (1.0f - fy);
    float w10 = fx * (1.0f - fy);
    float w01 = (1.0f - fx) * fy;
    float w11 = fx * fy;

    for (int c = 0; c < 4; ++c) {
        out_color[c] = p00[c] * w00 + p10[c] * w10 + p01[c] * w01 + p11[c] * w11;
    }
}

void DualKawaseBlur::downsample_cpu(
    const uint8_t* src_rgba,
    uint32_t in_w,
    uint32_t in_h,
    uint8_t* dst_rgba,
    uint32_t out_w,
    uint32_t out_h,
    float offset)
{
    float texel_x = 1.0f / static_cast<float>(in_w);
    float texel_y = 1.0f / static_cast<float>(in_h);
    float half_x = texel_x * offset;
    float half_y = texel_y * offset;

    for (uint32_t oy = 0; oy < out_h; ++oy) {
        for (uint32_t ox = 0; ox < out_w; ++ox) {
            float uv_x = (static_cast<float>(ox) + 0.5f) * (texel_x * 2.0f);
            float uv_y = (static_cast<float>(oy) + 0.5f) * (texel_y * 2.0f);

            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float sample[4];

            // 1. 中心采样点 (权重 4.0)
            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x, uv_y, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c] * 4.0f;

            // 2. 四个对角点 (各权重 1.0)
            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x - half_x, uv_y - half_y, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x + half_x, uv_y + half_y, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x + half_x, uv_y - half_y, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x - half_x, uv_y + half_y, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            // 归一化 (权重总和 8.0, 乘以 0.125)
            size_t dst_idx = (static_cast<size_t>(oy) * out_w + static_cast<size_t>(ox)) * 4;
            for (int c = 0; c < 4; ++c) {
                float val = std::round(sum[c] * 0.125f);
                dst_rgba[dst_idx + c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
            }
        }
    }
}

void DualKawaseBlur::upsample_cpu(
    const uint8_t* src_rgba,
    uint32_t in_w,
    uint32_t in_h,
    uint8_t* dst_rgba,
    uint32_t out_w,
    uint32_t out_h,
    float offset)
{
    float texel_x = 1.0f / static_cast<float>(in_w);
    float texel_y = 1.0f / static_cast<float>(in_h);
    float dx = texel_x * offset;
    float dy = texel_y * offset;

    for (uint32_t oy = 0; oy < out_h; ++oy) {
        for (uint32_t ox = 0; ox < out_w; ++ox) {
            float uv_x = (static_cast<float>(ox) + 0.5f) * (texel_x * 0.5f);
            float uv_y = (static_cast<float>(oy) + 0.5f) * (texel_y * 0.5f);

            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            float sample[4];

            // 8 采样点 Tent 滤波
            // 4 个正交轴采样点 (各权重 1.0)
            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x - dx * 2.0f, uv_y, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x, uv_y + dy * 2.0f, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x + dx * 2.0f, uv_y, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x, uv_y - dy * 2.0f, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c];

            // 4 个对角采样点 (各权重 2.0)
            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x - dx, uv_y + dy, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c] * 2.0f;

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x + dx, uv_y + dy, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c] * 2.0f;

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x + dx, uv_y - dy, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c] * 2.0f;

            sample_bilinear_rgba(src_rgba, in_w, in_h, uv_x - dx, uv_y - dy, sample);
            for (int c = 0; c < 4; ++c) sum[c] += sample[c] * 2.0f;

            // 归一化 (权重总和 4*1 + 4*2 = 12.0)
            size_t dst_idx = (static_cast<size_t>(oy) * out_w + static_cast<size_t>(ox)) * 4;
            for (int c = 0; c < 4; ++c) {
                float val = std::round(sum[c] / 12.0f);
                dst_rgba[dst_idx + c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
            }
        }
    }
}

void DualKawaseBlur::blur_cpu(
    const uint8_t* src_rgba,
    uint32_t width,
    uint32_t height,
    uint8_t* dst_rgba,
    const BlurParams& params)
{
    if (width == 0 || height == 0 || !src_rgba || !dst_rgba) return;

    auto passes = build_pass_plan(width, height, params);
    if (passes.empty()) {
        std::memcpy(dst_rgba, src_rgba, static_cast<size_t>(width) * height * 4);
        return;
    }

    uint32_t levels = get_quality_levels(params);

    // 金字塔纹理缓冲管理:
    // pyramid_buffers[0]: 降至 1/2 尺寸
    // pyramid_buffers[1]: 降至 1/4 尺寸
    // ...
    std::vector<std::vector<uint8_t>> pyramid_buffers(levels);
    uint32_t cur_w = width;
    uint32_t cur_h = height;
    for (uint32_t i = 0; i < levels; ++i) {
        cur_w = std::max(1u, cur_w / 2);
        cur_h = std::max(1u, cur_h / 2);
        pyramid_buffers[i].resize(static_cast<size_t>(cur_w) * cur_h * 4);
    }

    // 1. 执行 Downsample 序列
    for (uint32_t i = 0; i < levels; ++i) {
        const auto& pass = passes[i];
        const uint8_t* in_ptr = (i == 0) ? src_rgba : pyramid_buffers[i - 1].data();
        uint8_t* out_ptr = pyramid_buffers[i].data();

        downsample_cpu(
            in_ptr,
            pass.input_width,
            pass.input_height,
            out_ptr,
            pass.output_width,
            pass.output_height,
            pass.uniforms.offset);
    }

    // 2. 执行 Upsample 序列
    for (uint32_t i = levels; i < passes.size(); ++i) {
        const auto& pass = passes[i];
        // 输入是当前层
        uint32_t in_lvl = pass.pyramid_level + 1; // 对应来源层级
        const uint8_t* in_ptr = pyramid_buffers[in_lvl - 1].data();

        // 输出如果已到达最顶层 (原图尺寸)，直接写入 dst_rgba；否则写入 pyramid_buffers[pass.pyramid_level - 1]
        uint8_t* out_ptr = (pass.pyramid_level == 0) ? dst_rgba : pyramid_buffers[pass.pyramid_level - 1].data();

        upsample_cpu(
            in_ptr,
            pass.input_width,
            pass.input_height,
            out_ptr,
            pass.output_width,
            pass.output_height,
            pass.uniforms.offset);
    }
}

} // namespace kernetile::fx

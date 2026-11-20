#include "kernetile/cpu/cpu_rasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace kernetile::cpu {

uint32_t CpuTileRasterizer::calculate_worker_threads(uint32_t dirty_tile_count, uint32_t cpu_cores) noexcept {
    if (cpu_cores == 0) cpu_cores = 1;
    if (dirty_tile_count <= 8) {
        return 1; // 极小局部更新 (如光标闪烁): 纯单线程执行，零上下文切换
    } else if (dirty_tile_count <= 64) {
        return std::clamp(cpu_cores / 4, 2u, 4u); // 轻微交互: 2-4 线程
    } else {
        return std::clamp(cpu_cores, 4u, 8u);     // 大面积/全屏重绘: 启动最大 8 线程
    }
}

float CpuTileRasterizer::erf_approx(float x) noexcept {
    float x_abs = std::abs(x);
    float t = 1.0f / (1.0f + 0.278393f * x_abs + 0.230389f * x_abs * x_abs + 
                      0.000972f * x_abs * x_abs * x_abs + 0.078108f * x_abs * x_abs * x_abs * x_abs);
    float res = 1.0f - t * t * t * t;
    return (x >= 0.0f) ? res : -res;
}

float CpuTileRasterizer::evaluate_coverage(float d) noexcept {
    return std::clamp(0.5f - d, 0.0f, 1.0f);
}

void CpuTileRasterizer::render_tile(
    uint32_t tile_x,
    uint32_t tile_y,
    uint32_t tile_id,
    uint32_t width,
    uint32_t height,
    std::span<const core::UiPrimitive> primitives,
    std::span<const uint32_t>          tile_indices,
    std::span<const core::TileHeader>  tile_headers,
    std::span<const core::ClipRect>    clip_rects,
    uint8_t* dst_framebuffer,
    const uint8_t* atlas_pixels,
    uint32_t atlas_width,
    uint32_t atlas_height)
{
    if (tile_id >= tile_headers.size()) return;

    core::TileHeader header = tile_headers[tile_id];
    uint32_t prim_count = header.primitive_count;
    uint32_t prim_offset = header.primitive_offset;

    uint32_t start_x = tile_x * 16;
    uint32_t start_y = tile_y * 16;
    uint32_t end_x = std::min(start_x + 16, width);
    uint32_t end_y = std::min(start_y + 16, height);

    for (uint32_t py = start_y; py < end_y; ++py) {
        for (uint32_t px = start_x; px < end_x; ++px) {
            float p_x = static_cast<float>(px) + 0.5f;
            float p_y = static_cast<float>(py) + 0.5f;

            float accum_r = 0.0f;
            float accum_g = 0.0f;
            float accum_b = 0.0f;
            float accum_a = 0.0f;

            // Back-to-front 画家算法合成瓦片内图元
            for (uint32_t i = 0; i < prim_count; ++i) {
                uint32_t prim_idx = tile_indices[prim_offset + i];
                const auto& prim = primitives[prim_idx];

                // 1. 裁剪判断 (如果关联了 ClipRect)
                if (prim.clip_index != core::INVALID_INDEX_U16 && prim.clip_index < clip_rects.size()) {
                    uint32_t curr_clip = prim.clip_index;
                    bool culled = false;
                    uint32_t safety = 0;
                    while (curr_clip != core::INVALID_INDEX_U32 && curr_clip < clip_rects.size() && safety < 64) {
                        safety++;
                        const auto& clip = clip_rects[curr_clip];
                        if (p_x < clip.min_x || p_x > clip.max_x || p_y < clip.min_y || p_y > clip.max_y) {
                            culled = true;
                            break;
                        }
                        if (clip.corner_radius > 0.0f) {
                            float ccx = (clip.min_x + clip.max_x) * 0.5f;
                            float ccy = (clip.min_y + clip.max_y) * 0.5f;
                            float chx = (clip.max_x - clip.min_x) * 0.5f;
                            float chy = (clip.max_y - clip.min_y) * 0.5f;
                            float cr = clip.corner_radius;
                            float cqx = std::abs(p_x - ccx) - chx + cr;
                            float cqy = std::abs(p_y - ccy) - chy + cr;
                            float out_len = std::sqrt(std::max(0.0f, cqx) * std::max(0.0f, cqx) +
                                                      std::max(0.0f, cqy) * std::max(0.0f, cqy));
                            float in_d = std::min(std::max(cqx, cqy), 0.0f);
                            if (in_d + out_len - cr > 0.0f) {
                                culled = true;
                                break;
                            }
                        }
                        curr_clip = clip.parent_index;
                    }
                    if (culled) continue;
                }

                // 2. 解析软阴影
                if (prim.shadow_blur > 0.0f) {
                    float s_min_x = prim.min_x - prim.shadow_spread + prim.shadow_offset_x;
                    float s_min_y = prim.min_y - prim.shadow_spread + prim.shadow_offset_y;
                    float s_max_x = prim.max_x + prim.shadow_spread + prim.shadow_offset_x;
                    float s_max_y = prim.max_y + prim.shadow_spread + prim.shadow_offset_y;

                    float inv_sigma = 1.0f / (prim.shadow_blur * 1.41421356f);
                    float fx = 0.5f * (erf_approx((p_x - s_min_x) * inv_sigma) - erf_approx((p_x - s_max_x) * inv_sigma));
                    float fy = 0.5f * (erf_approx((p_y - s_min_y) * inv_sigma) - erf_approx((p_y - s_max_y) * inv_sigma));
                    float shadow_intensity = fx * fy;

                    if (shadow_intensity > 0.0005f) {
                        float src_a = prim.color_a * shadow_intensity;
                        float one_minus_sa = 1.0f - src_a;
                        accum_r = accum_r * one_minus_sa;
                        accum_g = accum_g * one_minus_sa;
                        accum_b = accum_b * one_minus_sa;
                        accum_a = src_a + accum_a * one_minus_sa;
                    }
                }

                // 3. 几何本体 SDF
                float center_x = (prim.min_x + prim.max_x) * 0.5f;
                float center_y = (prim.min_y + prim.max_y) * 0.5f;
                float half_w = (prim.max_x - prim.min_x) * 0.5f;
                float half_h = (prim.max_y - prim.min_y) * 0.5f;

                float r = (p_x > center_x) 
                    ? ((p_y > center_y) ? prim.radius_bottom_right : prim.radius_top_right)
                    : ((p_y > center_y) ? prim.radius_bottom_left  : prim.radius_top_left);
                r = std::clamp(r, 0.0f, std::min(half_w, half_h));

                float qx = std::abs(p_x - center_x) - half_w + r;
                float qy = std::abs(p_y - center_y) - half_h + r;

                float out_len = std::sqrt(std::max(0.0f, qx) * std::max(0.0f, qx) +
                                          std::max(0.0f, qy) * std::max(0.0f, qy));
                float in_d = std::min(std::max(qx, qy), 0.0f);
                float dist = in_d + out_len - r;

                if (prim.border_width > 0.0f) {
                    dist = std::abs(dist + prim.border_width * 0.5f) - prim.border_width * 0.5f;
                }

                // 判断是否为字形图元 (IsGlyph)
                bool is_glyph = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph)) != 0;
                float geom_cov = 0.0f;

                if (is_glyph) {
                    if (p_x >= prim.min_x && p_x <= prim.max_x &&
                        p_y >= prim.min_y && p_y <= prim.max_y &&
                        atlas_pixels && atlas_width > 0 && atlas_height > 0)
                    {
                        float pw = prim.max_x - prim.min_x;
                        float ph = prim.max_y - prim.min_y;
                        if (pw > 0.001f && ph > 0.001f) {
                            float norm_x = (p_x - prim.min_x) / pw;
                            float norm_y = (p_y - prim.min_y) / ph;
                            float u0 = static_cast<float>(prim.atlas_u0) / 65535.0f;
                            float v0 = static_cast<float>(prim.atlas_v0) / 65535.0f;
                            float u1 = static_cast<float>(prim.atlas_u1) / 65535.0f;
                            float v1 = static_cast<float>(prim.atlas_v1) / 65535.0f;
                            float u = u0 + norm_x * (u1 - u0);
                            float v = v0 + norm_y * (v1 - v0);
                            uint32_t ax = std::clamp(static_cast<uint32_t>(u * (atlas_width - 1) + 0.5f), 0u, atlas_width - 1);
                            uint32_t ay = std::clamp(static_cast<uint32_t>(v * (atlas_height - 1) + 0.5f), 0u, atlas_height - 1);
                            geom_cov = static_cast<float>(atlas_pixels[ay * atlas_width + ax]) / 255.0f;
                        }
                    }
                } else {
                    geom_cov = evaluate_coverage(dist);
                }

                if (geom_cov > 0.0001f) {
                    // 线性光空间预乘混合
                    float src_r = prim.color_r * geom_cov;
                    float src_g = prim.color_g * geom_cov;
                    float src_b = prim.color_b * geom_cov;
                    float src_a = prim.color_a * geom_cov;

                    float one_minus_sa = 1.0f - src_a;
                    accum_r = src_r + accum_r * one_minus_sa;
                    accum_g = src_g + accum_g * one_minus_sa;
                    accum_b = src_b + accum_b * one_minus_sa;
                    accum_a = src_a + accum_a * one_minus_sa;
                }
            }

            // 写回目标像素
            size_t p_idx = (static_cast<size_t>(py) * width + px) * 4;
            dst_framebuffer[p_idx + 0] = static_cast<uint8_t>(std::clamp(std::round(accum_r * 255.0f), 0.0f, 255.0f));
            dst_framebuffer[p_idx + 1] = static_cast<uint8_t>(std::clamp(std::round(accum_g * 255.0f), 0.0f, 255.0f));
            dst_framebuffer[p_idx + 2] = static_cast<uint8_t>(std::clamp(std::round(accum_b * 255.0f), 0.0f, 255.0f));
            dst_framebuffer[p_idx + 3] = static_cast<uint8_t>(std::clamp(std::round(accum_a * 255.0f), 0.0f, 255.0f));
        }
    }
}

void CpuTileRasterizer::render(
    const RenderParams& params,
    uint8_t* dst_framebuffer)
{
    if (params.width == 0 || params.height == 0 || !dst_framebuffer) return;

    uint32_t dirty_count = static_cast<uint32_t>(params.dirty_tiles.size());
    if (dirty_count == 0) return;

    uint32_t cpu_cores = std::thread::hardware_concurrency();
    uint32_t threads = (params.thread_count > 0) 
        ? params.thread_count 
        : calculate_worker_threads(dirty_count, cpu_cores);

    uint32_t tiles_x = (params.width + 15) / 16;

    // 单线程直接执行
    if (threads <= 1 || dirty_count <= 1) {
        for (uint32_t tile_id : params.dirty_tiles) {
            uint32_t tx = tile_id % tiles_x;
            uint32_t ty = tile_id / tiles_x;
            render_tile(tx, ty, tile_id, params.width, params.height,
                        params.primitives, params.tile_indices,
                        params.tile_headers, params.clip_rects,
                        dst_framebuffer,
                        params.atlas_pixels, params.atlas_width, params.atlas_height);
        }
        return;
    }

    // 多线程并发分块调度
    std::vector<std::thread> workers;
    workers.reserve(threads);

    uint32_t chunk_size = (dirty_count + threads - 1) / threads;

    for (uint32_t t = 0; t < threads; ++t) {
        uint32_t start_idx = t * chunk_size;
        uint32_t end_idx = std::min(start_idx + chunk_size, dirty_count);

        if (start_idx >= end_idx) continue;

        workers.emplace_back([&params, start_idx, end_idx, tiles_x, dst_framebuffer]() {
            for (uint32_t i = start_idx; i < end_idx; ++i) {
                uint32_t tile_id = params.dirty_tiles[i];
                uint32_t tx = tile_id % tiles_x;
                uint32_t ty = tile_id / tiles_x;
                render_tile(tx, ty, tile_id, params.width, params.height,
                            params.primitives, params.tile_indices,
                            params.tile_headers, params.clip_rects,
                            dst_framebuffer,
                            params.atlas_pixels, params.atlas_width, params.atlas_height);
            }
        });
    }

    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

} // namespace kernetile::cpu

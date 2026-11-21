#include "kernetile/cpu/cpu_rasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace kernetile::cpu {

uint32_t CpuTileRasterizer::calculate_worker_threads(uint32_t dirty_tile_count, uint32_t cpu_cores) noexcept {
    if (cpu_cores == 0) cpu_cores = 1;
    if (dirty_tile_count <= 600) {
        return 1; // 常规桌面 UI (<= 480x320 瓦片量级): 纯单线程执行，零线程启停开销与零系统颠簸
    } else if (dirty_tile_count <= 2400) {
        return std::clamp(cpu_cores / 4, 2u, 4u); // 中等面积交互 (1080p): 2-4 线程
    } else {
        return std::clamp(cpu_cores, 4u, 8u);     // 2K/4K 大面积重绘: 启动最大 8 线程
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
    uint32_t tile_w = end_x - start_x;

    // 0. 空瓦片极速早出 (直接以 0 填充整块，耗时趋近 0ns)
    if (prim_count == 0) {
        for (uint32_t py = start_y; py < end_y; ++py) {
            uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(py) * width + start_x) * 4);
            std::fill_n(row_dst, tile_w, 0u);
        }
        return;
    }

    // 0.1 纯实心不透明矩形瓦片极速早出 (完全覆盖该瓦片的背景或卡片色块，直接 SIMD memset 填充，免除全部 256 次逐像素浮点循环)
    if (prim_count == 1) {
        uint32_t prim_idx = tile_indices[prim_offset];
        const auto& prim = primitives[prim_idx];
        if (prim.clip_index == core::INVALID_INDEX_U16 &&
            prim.shadow_blur <= 0.0f &&
            !(prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph)) &&
            prim.color_a >= 0.999f &&
            prim.border_width <= 0.0f &&
            prim.radius_top_left <= 0.0f && prim.radius_top_right <= 0.0f &&
            prim.radius_bottom_right <= 0.0f && prim.radius_bottom_left <= 0.0f &&
            prim.min_x <= static_cast<float>(start_x) && prim.max_x >= static_cast<float>(end_x) &&
            prim.min_y <= static_cast<float>(start_y) && prim.max_y >= static_cast<float>(end_y))
        {
            uint32_t r = static_cast<uint32_t>(std::clamp(prim.color_r * 255.0f + 0.5f, 0.0f, 255.0f));
            uint32_t g = static_cast<uint32_t>(std::clamp(prim.color_g * 255.0f + 0.5f, 0.0f, 255.0f));
            uint32_t b = static_cast<uint32_t>(std::clamp(prim.color_b * 255.0f + 0.5f, 0.0f, 255.0f));
            uint32_t a = 255u;
            uint32_t pixel_val = r | (g << 8) | (b << 16) | (a << 24);

            for (uint32_t py = start_y; py < end_y; ++py) {
                uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(py) * width + start_x) * 4);
                std::fill_n(row_dst, tile_w, pixel_val);
            }
            return;
        }
    }

    // 0.2 瓦片级图元粗判与几何不变式预提炼 (提炼后在 256 次逐像素循环中直接复用，完全消除重复除法与裁剪栈开销)
    struct PreparedPrim {
        const core::UiPrimitive* prim{nullptr};
        float center_x{0.0f}, center_y{0.0f};
        float half_w{0.0f}, half_h{0.0f};
        float r_tl{0.0f}, r_tr{0.0f}, r_br{0.0f}, r_bl{0.0f};
        // shadow
        bool has_shadow{false};
        float s_min_x{0.0f}, s_min_y{0.0f}, s_max_x{0.0f}, s_max_y{0.0f};
        float inv_sigma{0.0f};
        // glyph
        bool is_glyph{false};
        float inv_pw{0.0f}, inv_ph{0.0f};
        float ax0{0.0f}, ay0{0.0f}, dax{0.0f}, day{0.0f};
        // clip mode: 0 = 无需裁剪测试, 1 = 需逐像素裁剪测试
        uint8_t clip_mode{0};
    };

    PreparedPrim prepared_prims[64];
    uint32_t active_prim_count = 0;
    float tile_f_start_x = static_cast<float>(start_x);
    float tile_f_start_y = static_cast<float>(start_y);
    float tile_f_end_x   = static_cast<float>(end_x);
    float tile_f_end_y   = static_cast<float>(end_y);

    for (uint32_t i = 0; i < prim_count && active_prim_count < 64; ++i) {
        uint32_t prim_idx = tile_indices[prim_offset + i];
        const auto& prim = primitives[prim_idx];

        // 裁剪级粗判：若整个瓦片完全落在某一级 ClipRect 外，直接跳过该图元
        uint8_t clip_mode = 0;
        if (prim.clip_index != core::INVALID_INDEX_U16 && prim.clip_index < clip_rects.size()) {
            uint32_t curr_clip = prim.clip_index;
            bool culled_away = false;
            uint32_t safety = 0;
            while (curr_clip != core::INVALID_INDEX_U32 && curr_clip < clip_rects.size() && safety < 64) {
                safety++;
                const auto& clip = clip_rects[curr_clip];
                if (tile_f_end_x <= clip.min_x || tile_f_start_x >= clip.max_x ||
                    tile_f_end_y <= clip.min_y || tile_f_start_y >= clip.max_y)
                {
                    culled_away = true;
                    break;
                }
                bool tile_fully_inside = (tile_f_start_x >= clip.min_x &&
                                          tile_f_end_x <= clip.max_x &&
                                          tile_f_start_y >= clip.min_y &&
                                          tile_f_end_y <= clip.max_y &&
                                          clip.corner_radius <= 0.0f);
                if (!tile_fully_inside) {
                    clip_mode = 1;
                }
                curr_clip = clip.parent_index;
            }
            if (culled_away) {
                continue;
            }
        }

        auto& p = prepared_prims[active_prim_count++];
        p.prim = &prim;
        p.clip_mode = clip_mode;

        p.center_x = (prim.min_x + prim.max_x) * 0.5f;
        p.center_y = (prim.min_y + prim.max_y) * 0.5f;
        p.half_w = (prim.max_x - prim.min_x) * 0.5f;
        p.half_h = (prim.max_y - prim.min_y) * 0.5f;

        float max_r = std::min(p.half_w, p.half_h);
        p.r_tl = std::clamp(prim.radius_top_left, 0.0f, max_r);
        p.r_tr = std::clamp(prim.radius_top_right, 0.0f, max_r);
        p.r_br = std::clamp(prim.radius_bottom_right, 0.0f, max_r);
        p.r_bl = std::clamp(prim.radius_bottom_left, 0.0f, max_r);

        if (prim.shadow_blur > 0.0f) {
            p.has_shadow = true;
            p.s_min_x = prim.min_x - prim.shadow_spread + prim.shadow_offset_x;
            p.s_min_y = prim.min_y - prim.shadow_spread + prim.shadow_offset_y;
            p.s_max_x = prim.max_x + prim.shadow_spread + prim.shadow_offset_x;
            p.s_max_y = prim.max_y + prim.shadow_spread + prim.shadow_offset_y;
            p.inv_sigma = 1.0f / (prim.shadow_blur * 1.41421356f);
        } else {
            p.has_shadow = false;
        }

        bool is_glyph = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph)) != 0;
        p.is_glyph = is_glyph;
        if (is_glyph) {
            float pw = prim.max_x - prim.min_x;
            float ph = prim.max_y - prim.min_y;
            if (pw > 0.001f && ph > 0.001f && atlas_pixels && atlas_width > 0 && atlas_height > 0) {
                p.inv_pw = 1.0f / pw;
                p.inv_ph = 1.0f / ph;
                p.ax0 = (static_cast<float>(prim.atlas_u0) * (1.0f / 65535.0f)) * static_cast<float>(atlas_width);
                p.ay0 = (static_cast<float>(prim.atlas_v0) * (1.0f / 65535.0f)) * static_cast<float>(atlas_height);
                float ax1 = (static_cast<float>(prim.atlas_u1) * (1.0f / 65535.0f)) * static_cast<float>(atlas_width);
                float ay1 = (static_cast<float>(prim.atlas_v1) * (1.0f / 65535.0f)) * static_cast<float>(atlas_height);
                p.dax = ax1 - p.ax0;
                p.day = ay1 - p.ay0;
            } else {
                p.inv_pw = 0.0f;
                p.inv_ph = 0.0f;
            }
        }
    }

    if (active_prim_count == 0) {
        for (uint32_t py = start_y; py < end_y; ++py) {
            uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(py) * width + start_x) * 4);
            std::fill_n(row_dst, tile_w, 0u);
        }
        return;
    }

    for (uint32_t py = start_y; py < end_y; ++py) {
        float p_y = static_cast<float>(py) + 0.5f;

        // 行级阴影 Y 因子预计算 (每行计算 1 次，免除列循环中 16 次重复计算 erf)
        float row_shadow_fy[64];
        for (uint32_t i = 0; i < active_prim_count; ++i) {
            if (prepared_prims[i].has_shadow) {
                row_shadow_fy[i] = 0.5f * (erf_approx((p_y - prepared_prims[i].s_min_y) * prepared_prims[i].inv_sigma) -
                                           erf_approx((p_y - prepared_prims[i].s_max_y) * prepared_prims[i].inv_sigma));
            }
        }

        for (uint32_t px = start_x; px < end_x; ++px) {
            float p_x = static_cast<float>(px) + 0.5f;

            float accum_r = 0.0f;
            float accum_g = 0.0f;
            float accum_b = 0.0f;
            float accum_a = 0.0f;

            // Back-to-front 画家算法合成瓦片内图元
            for (uint32_t i = 0; i < active_prim_count; ++i) {
                const auto& prep = prepared_prims[i];
                const auto& prim = *prep.prim;

                // 1. 裁剪判断 (仅在需要逐像素精细裁剪时才执行)
                if (prep.clip_mode == 1) {
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

                // 2. 解析软阴影 (利用预提炼参数与行级 fy)
                if (prep.has_shadow) {
                    float fx = 0.5f * (erf_approx((p_x - prep.s_min_x) * prep.inv_sigma) - erf_approx((p_x - prep.s_max_x) * prep.inv_sigma));
                    float shadow_intensity = fx * row_shadow_fy[i];

                    if (shadow_intensity > 0.0005f) {
                        float src_a = prim.color_a * shadow_intensity;
                        float one_minus_sa = 1.0f - src_a;
                        accum_r = accum_r * one_minus_sa;
                        accum_g = accum_g * one_minus_sa;
                        accum_b = accum_b * one_minus_sa;
                        accum_a = src_a + accum_a * one_minus_sa;
                    }
                }

                // 3. 几何本体 SDF (使用预提炼中心与半径常量)
                float r = (p_x > prep.center_x) 
                    ? ((p_y > prep.center_y) ? prep.r_br : prep.r_tr)
                    : ((p_y > prep.center_y) ? prep.r_bl : prep.r_tl);

                float qx = std::abs(p_x - prep.center_x) - prep.half_w + r;
                float qy = std::abs(p_y - prep.center_y) - prep.half_h + r;

                // 仅在圆角外侧象限 (qx > 0 且 qy > 0) 时计算平方根，其余绝大多数点直接求 max(qx, qy)
                float out_len = (qx > 0.0f && qy > 0.0f)
                    ? std::sqrt(qx * qx + qy * qy)
                    : std::max(0.0f, std::max(qx, qy));
                float in_d = std::min(std::max(qx, qy), 0.0f);
                float dist = in_d + out_len - r;

                if (prim.border_width > 0.0f) {
                    dist = std::abs(dist + prim.border_width * 0.5f) - prim.border_width * 0.5f;
                }

                float geom_cov = 0.0f;

                if (prep.is_glyph) {
                    if (p_x >= prim.min_x && p_x <= prim.max_x &&
                        p_y >= prim.min_y && p_y <= prim.max_y &&
                        prep.inv_pw > 0.0f && atlas_pixels)
                    {
                        float norm_x = (p_x - prim.min_x) * prep.inv_pw;
                        float norm_y = (p_y - prim.min_y) * prep.inv_ph;

                        float fx = prep.ax0 + norm_x * prep.dax - 0.5f;
                        float fy = prep.ay0 + norm_y * prep.day - 0.5f;

                        int32_t ix0 = static_cast<int32_t>(std::floor(fx));
                        int32_t iy0 = static_cast<int32_t>(std::floor(fy));
                        float tx = fx - static_cast<float>(ix0);
                        float ty = fy - static_cast<float>(iy0);

                        const size_t row0 = static_cast<size_t>(iy0) * atlas_width;
                        const size_t row1 = row0 + atlas_width;
                        if (ix0 >= 0 && ix0 + 1 < static_cast<int32_t>(atlas_width) &&
                            iy0 >= 0 && iy0 + 1 < static_cast<int32_t>(atlas_height))
                        {
                            float c00 = static_cast<float>(atlas_pixels[row0 + ix0]);
                            float c10 = static_cast<float>(atlas_pixels[row0 + ix0 + 1]);
                            float c01 = static_cast<float>(atlas_pixels[row1 + ix0]);
                            float c11 = static_cast<float>(atlas_pixels[row1 + ix0 + 1]);
                            float c0 = c00 + tx * (c10 - c00);
                            float c1 = c01 + tx * (c11 - c01);
                            geom_cov = (c0 + ty * (c1 - c0)) * (1.0f / 255.0f);
                        } else {
                            auto sample_atlas = [&](int32_t sx, int32_t sy) noexcept -> float {
                                sx = std::clamp(sx, 0, static_cast<int32_t>(atlas_width) - 1);
                                sy = std::clamp(sy, 0, static_cast<int32_t>(atlas_height) - 1);
                                return static_cast<float>(atlas_pixels[static_cast<size_t>(sy) * atlas_width + sx]);
                            };
                            float c00 = sample_atlas(ix0, iy0);
                            float c10 = sample_atlas(ix0 + 1, iy0);
                            float c01 = sample_atlas(ix0, iy0 + 1);
                            float c11 = sample_atlas(ix0 + 1, iy0 + 1);
                            float c0 = c00 + tx * (c10 - c00);
                            float c1 = c01 + tx * (c11 - c01);
                            geom_cov = (c0 + ty * (c1 - c0)) * (1.0f / 255.0f);
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

            // 写回目标像素 (免除昂贵 std::round 函数跳转开销)
            size_t p_idx = (static_cast<size_t>(py) * width + px) * 4;
            dst_framebuffer[p_idx + 0] = static_cast<uint8_t>(std::clamp(accum_r * 255.0f + 0.5f, 0.0f, 255.0f));
            dst_framebuffer[p_idx + 1] = static_cast<uint8_t>(std::clamp(accum_g * 255.0f + 0.5f, 0.0f, 255.0f));
            dst_framebuffer[p_idx + 2] = static_cast<uint8_t>(std::clamp(accum_b * 255.0f + 0.5f, 0.0f, 255.0f));
            dst_framebuffer[p_idx + 3] = static_cast<uint8_t>(std::clamp(accum_a * 255.0f + 0.5f, 0.0f, 255.0f));
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

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    uint32_t threads = 1;
#else
    uint32_t cpu_cores = std::thread::hardware_concurrency();
    uint32_t threads = (params.thread_count > 0) 
        ? params.thread_count 
        : calculate_worker_threads(dirty_count, cpu_cores);
#endif

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

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
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
#endif
}

} // namespace kernetile::cpu

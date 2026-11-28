#include "kernetile/cpu/cpu_rasterizer.hpp"
#include "kernetile/fx/dual_kawase_blur.hpp"
#include "kernetile/math/color_space.hpp"
#include "kernetile/text/stem_darkening.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace kernetile::cpu {

namespace {

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
// 持久化轻量化瓦片渲染线程池：
// 1. 零每帧 std::thread 创建/销毁内核开销
// 2. 调用线程自身作为 Worker 0 协同参与计算，零主线程空转等待
// 3. 动态 16-tile 块抢占调度 (Work Stealing)，避免阴影/文本密集瓦片拖慢整体帧时间
class CpuTileThreadPool {
public:
    static CpuTileThreadPool& instance() {
        static CpuTileThreadPool s_pool;
        return s_pool;
    }

    template <typename Fn>
    void parallel_for_tiles(uint32_t tile_count, uint32_t requested_threads, Fn&& fn) {
        if (tile_count == 0) return;
        const uint32_t max_workers = static_cast<uint32_t>(m_workers.size()) + 1u;
        const uint32_t active_threads = std::min(std::max(1u, requested_threads), max_workers);
        if (active_threads <= 1u || tile_count <= 16u) {
            for (uint32_t i = 0; i < tile_count; ++i) {
                fn(i);
            }
            return;
        }

        using RawFn = std::remove_reference_t<Fn>;
        auto thunk = [](void* ctx, uint32_t idx) {
            (*static_cast<RawFn*>(ctx))(idx);
        };

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_task_ctx = &fn;
            m_task_fn = thunk;
            m_total_tiles = tile_count;
            m_next_tile.store(0u, std::memory_order_relaxed);
            m_active_workers = active_threads - 1u;
            m_remaining_workers.store(active_threads - 1u, std::memory_order_relaxed);
            ++m_generation;
        }
        m_cv.notify_all();

        // 主线程作为 Worker 0 立即参与动态抢占
        run_work_loop(tile_count, thunk, &fn);

        // 等待后台工作线程完成当前帧
        if (m_remaining_workers.load(std::memory_order_acquire) > 0u) {
            std::unique_lock<std::mutex> lock(m_done_mutex);
            m_done_cv.wait(lock, [this]() {
                return m_remaining_workers.load(std::memory_order_acquire) == 0u;
            });
        }
    }

private:
    CpuTileThreadPool() {
        uint32_t hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4;
        uint32_t bg_count = std::clamp(hw, 2u, 8u) - 1u;
        m_workers.reserve(bg_count);
        for (uint32_t id = 0; id < bg_count; ++id) {
            m_workers.emplace_back([this, id]() { worker_main(id); });
        }
    }

    ~CpuTileThreadPool() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
            ++m_generation;
        }
        m_cv.notify_all();
        for (auto& t : m_workers) {
            if (t.joinable()) t.join();
        }
    }

    static constexpr uint32_t kChunkSize = 16u;

    void run_work_loop(uint32_t total, void (*fn)(void*, uint32_t), void* ctx) {
        while (true) {
            uint32_t start = m_next_tile.fetch_add(kChunkSize, std::memory_order_relaxed);
            if (start >= total) break;
            uint32_t end = std::min(start + kChunkSize, total);
            for (uint32_t i = start; i < end; ++i) {
                fn(ctx, i);
            }
        }
    }

    void worker_main(uint32_t worker_id) {
        uint64_t seen_gen = 0;
        while (true) {
            void* ctx = nullptr;
            void (*fn)(void*, uint32_t) = nullptr;
            uint32_t total = 0;
            bool participate = false;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this, seen_gen]() {
                    return m_stop || m_generation != seen_gen;
                });
                if (m_stop) return;
                seen_gen = m_generation;
                if (worker_id < m_active_workers) {
                    participate = true;
                    ctx = m_task_ctx;
                    fn = m_task_fn;
                    total = m_total_tiles;
                }
            }
            if (participate) {
                run_work_loop(total, fn, ctx);
                if (m_remaining_workers.fetch_sub(1u, std::memory_order_acq_rel) == 1u) {
                    std::lock_guard<std::mutex> lock(m_done_mutex);
                    m_done_cv.notify_one();
                }
            }
        }
    }

    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::mutex m_done_mutex;
    std::condition_variable m_done_cv;
    bool m_stop{false};
    uint64_t m_generation{0};
    void* m_task_ctx{nullptr};
    void (*m_task_fn)(void*, uint32_t){nullptr};
    uint32_t m_total_tiles{0};
    uint32_t m_active_workers{0};
    std::atomic<uint32_t> m_next_tile{0};
    std::atomic<uint32_t> m_remaining_workers{0};
};
#endif

// 极速无 libm 函数调用的整数向下取整 (比 MSVC std::floor 快 ~5x)
inline int32_t fast_floor_i32(float x) noexcept {
    int32_t i = static_cast<int32_t>(x);
    return i - (x < static_cast<float>(i) ? 1 : 0);
}

// Apple G2 连续曲率超椭圆圆角 SDF (与 HLSL/WGSL box_distance 严格逐像素一致)
inline float eval_squircle_box_distance(float abs_dx, float abs_dy,
                                        float half_w, float half_h,
                                        float raw_r) noexcept {
    const float min_half = std::min(half_w, half_h);
    const float r = std::clamp(raw_r, 0.0f, min_half);
    if (r <= 0.001f) {
        const float qx = abs_dx - half_w;
        const float qy = abs_dy - half_h;
        const float out_len = (qx > 0.0f && qy > 0.0f)
            ? std::sqrt(qx * qx + qy * qy)
            : std::max(0.0f, std::max(qx, qy));
        return std::min(std::max(qx, qy), 0.0f) + out_len;
    }

    const float ratio = r / std::max(min_half, 1e-4f);
    const float squircle_t = std::clamp((0.90f - ratio) * 4.0f, 0.0f, 1.0f);

    const float qx2 = abs_dx - half_w + r;
    const float qy2 = abs_dy - half_h + r;
    const float out2 = (qx2 > 0.0f && qy2 > 0.0f)
        ? std::sqrt(qx2 * qx2 + qy2 * qy2)
        : std::max(0.0f, std::max(qx2, qy2));
    const float d_circle = std::min(std::max(qx2, qy2), 0.0f) + out2 - r;

    if (squircle_t <= 0.001f) {
        return d_circle;
    }

    const float r_eff = std::min(min_half, r * 1.35f);
    const float qx4 = abs_dx - half_w + r_eff;
    const float qy4 = abs_dy - half_h + r_eff;
    float d_squircle;
    if (qx4 > 0.0f && qy4 > 0.0f) {
        const float inv_r = 1.0f / r_eff;
        const float u = qx4 * inv_r;
        const float v = qy4 * inv_r;
        const float u2 = u * u, v2 = v * v;
        const float u4 = u2 * u2, v4 = v2 * v2;
        const float s4 = u4 + v4;
        const float s2 = std::sqrt(s4);
        const float d4 = std::sqrt(s2);
        const float denom = s2 * d4;
        const float grad = (denom > 1e-6f) ? (std::sqrt(u4 * u2 + v4 * v2) / denom) : 1.0f;
        d_squircle = (d4 - 1.0f) * r_eff / std::max(grad, 0.5f);
    } else {
        d_squircle = std::max(qx4, qy4) - r_eff;
    }

    return d_circle + (d_squircle - d_circle) * squircle_t;
}

// 单点级联裁剪与渐隐羽化系数计算 (0.0 = 剔除, 0..1 = 列表边缘平滑衰减系数)
inline float eval_clip_alpha(float p_x, float p_y, uint16_t first_clip,
                             std::span<const core::ClipRect> clip_rects) noexcept {
    uint32_t curr_clip = first_clip;
    uint32_t safety = 0;
    float alpha = 1.0f;
    while (curr_clip != core::INVALID_INDEX_U32 && curr_clip < clip_rects.size() && safety < 64) {
        ++safety;
        const auto& clip = clip_rects[curr_clip];
        if (p_x < clip.min_x || p_x > clip.max_x || p_y < clip.min_y || p_y > clip.max_y) {
            return 0.0f;
        }
        if (clip.corner_radius > 0.0f) {
            float ccx = (clip.min_x + clip.max_x) * 0.5f;
            float ccy = (clip.min_y + clip.max_y) * 0.5f;
            float chx = (clip.max_x - clip.min_x) * 0.5f;
            float chy = (clip.max_y - clip.min_y) * 0.5f;
            if (eval_squircle_box_distance(std::abs(p_x - ccx), std::abs(p_y - ccy), chx, chy, clip.corner_radius) > 0.0f) {
                return 0.0f;
            }
        }
        if ((clip.flags & 6u) != 0u) {
            if ((clip.flags & core::kClipFlagFeatherTop) != 0u) {
                const float top_f = static_cast<float>(clip.reserved & 0xFFFFu) * (1.0f / 16.0f);
                if (top_f > 0.05f) {
                    const float u = std::clamp((p_y - clip.min_y) / top_f, 0.0f, 1.0f);
                    alpha *= u * u * (3.0f - 2.0f * u);
                }
            }
            if ((clip.flags & core::kClipFlagFeatherBottom) != 0u) {
                const float bot_f = static_cast<float>((clip.reserved >> 16u) & 0xFFFFu) * (1.0f / 16.0f);
                if (bot_f > 0.05f) {
                    const float u = std::clamp((clip.max_y - p_y) / bot_f, 0.0f, 1.0f);
                    alpha *= u * u * (3.0f - 2.0f * u);
                }
            }
        }
        curr_clip = clip.parent_index;
    }
    return alpha;
}

inline void sample_mica_corner(float p_x, float p_y, float inv_w, float inv_h,
                               const std::array<uint32_t, 16>& mica_grid,
                               float& r, float& g, float& b, float& a) noexcept {
    float u = std::clamp(p_x * inv_w, 0.0f, 1.0f) * 3.0f;
    float v = std::clamp(p_y * inv_h, 0.0f, 1.0f) * 3.0f;
    uint32_t ix = std::min<uint32_t>(static_cast<uint32_t>(u), 2u);
    uint32_t iy = std::min<uint32_t>(static_cast<uint32_t>(v), 2u);
    float fx = u - static_cast<float>(ix);
    float fy = v - static_cast<float>(iy);
    auto unpack = [&](uint32_t x, uint32_t y, float& cr, float& cg, float& cb, float& ca) noexcept {
        uint32_t pk = mica_grid[y * 4u + x];
        cr = static_cast<float>(pk & 255u) * (1.0f / 255.0f);
        cg = static_cast<float>((pk >> 8u) & 255u) * (1.0f / 255.0f);
        cb = static_cast<float>((pk >> 16u) & 255u) * (1.0f / 255.0f);
        ca = static_cast<float>((pk >> 24u) & 255u) * (1.0f / 255.0f);
    };
    float r00, g00, b00, a00, r10, g10, b10, a10, r01, g01, b01, a01, r11, g11, b11, a11;
    unpack(ix, iy, r00, g00, b00, a00);
    unpack(ix + 1u, iy, r10, g10, b10, a10);
    unpack(ix, iy + 1u, r01, g01, b01, a01);
    unpack(ix + 1u, iy + 1u, r11, g11, b11, a11);
    float r0 = r00 + (r10 - r00) * fx, r1 = r01 + (r11 - r01) * fx;
    float g0 = g00 + (g10 - g00) * fx, g1 = g01 + (g11 - g01) * fx;
    float b0 = b00 + (b10 - b00) * fx, b1 = b01 + (b11 - b01) * fx;
    float a0 = a00 + (a10 - a00) * fx, a1 = a01 + (a11 - a01) * fx;
    r = r0 + (r1 - r0) * fy;
    g = g0 + (g1 - g0) * fy;
    b = b0 + (b1 - b0) * fy;
    a = a0 + (a1 - a0) * fy;
}

inline float eval_ign(uint32_t px, uint32_t py) noexcept {
    const float d = static_cast<float>(px) * 0.06711056f + static_cast<float>(py) * 0.00583715f;
    const float f1 = d - std::floor(d);
    const float t = 52.9829189f * f1;
    return t - std::floor(t);
}

inline float eval_acrylic_grain(float p_x, float p_y, float scale) noexcept {
    const float sc = std::max(scale, 1.0f);
    const uint32_t gx = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(std::floor(p_x / sc))));
    const uint32_t gy = static_cast<uint32_t>(std::max(0, static_cast<int32_t>(std::floor(p_y / sc))));
    uint32_t h = gx * 747796405u + gy * 2891336453u + 277803737u;
    h = ((h >> ((h >> 28u) + 4u)) ^ h) * 277803737u;
    h = (h >> 22u) ^ h;
    const float u0 = static_cast<float>(h & 65535u) * (1.0f / 65535.0f);
    const float u1 = static_cast<float>((h >> 16u) & 65535u) * (1.0f / 65535.0f);
    return (u0 + u1) - 1.0f;
}

} // namespace

uint32_t CpuTileRasterizer::calculate_worker_threads(uint32_t dirty_tile_count, uint32_t cpu_cores) noexcept {
    if (cpu_cores == 0) cpu_cores = 1;
    if (dirty_tile_count <= 8) {
        return 1; // 极小局部重绘 (<= 8 瓦片，如光标闪烁): 纯单线程执行，零同步开销
    } else if (dirty_tile_count <= 64) {
        return std::clamp(cpu_cores / 4, 2u, 4u); // 中小交互 (按钮悬停/下拉菜单): 2-4 线程
    } else {
        return std::clamp(cpu_cores, 4u, 8u);     // 滚动/动画/全屏重绘: 持久化线程池 4-8 线程并发
    }
}

float CpuTileRasterizer::erf_approx(float x) noexcept {
    float x_abs = std::abs(x);
    float x2 = x_abs * x_abs;
    float t = 1.0f / (1.0f + 0.278393f * x_abs + 0.230389f * x2 +
                      0.000972f * x2 * x_abs + 0.078108f * x2 * x2);
    float t2 = t * t;
    float res = 1.0f - t2 * t2;
    return (x >= 0.0f) ? res : -res;
}

float CpuTileRasterizer::evaluate_coverage(float d) noexcept {
    return std::clamp(0.5f - d, 0.0f, 1.0f);
}

namespace {
inline float evaluate_coverage(float d) noexcept { return CpuTileRasterizer::evaluate_coverage(d); }
inline float erf_approx(float x) noexcept { return CpuTileRasterizer::erf_approx(x); }

void render_tile_range(
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
    uint32_t atlas_height,
    bool mica_enabled,
    const std::array<uint32_t, 16>* mica_grid,
    const uint8_t* color_atlas_pixels,
    uint32_t color_atlas_width,
    uint32_t color_atlas_height,
    bool enable_dithering,
    bool enable_font_gamma,
    bool enable_acrylic_grain,
    uint32_t min_prim_id,
    uint32_t max_prim_id,
    bool load_from_dst)
{
    if (tile_id >= tile_headers.size()) return;

    const bool use_mica = mica_enabled && (mica_grid != nullptr);
    const core::TileHeader header = tile_headers[tile_id];
    const uint32_t prim_count = header.primitive_count;
    const uint32_t prim_offset = header.primitive_offset;

    const uint32_t start_x = tile_x * 16;
    const uint32_t start_y = tile_y * 16;
    const uint32_t end_x = std::min(start_x + 16, width);
    const uint32_t end_y = std::min(start_y + 16, height);
    const uint32_t tile_w = end_x - start_x;
    const uint32_t tile_h = end_y - start_y;
    if (tile_w == 0 || tile_h == 0) return;

    const float tile_f_start_x = static_cast<float>(start_x);
    const float tile_f_start_y = static_cast<float>(start_y);
    const float tile_f_end_x   = static_cast<float>(end_x);
    const float tile_f_end_y   = static_cast<float>(end_y);

    // 辅助：直接用 4 角双线性步进写满整个瓦片的云母背景到 framebuffer (比逐像素解包快 ~10x)
    auto fill_mica_to_fb = [&]() {
        const float inv_w = 1.0f / std::max(1.0f, static_cast<float>(width));
        const float inv_h = 1.0f / std::max(1.0f, static_cast<float>(height));
        float r00, g00, b00, a00, r10, g10, b10, a10, r01, g01, b01, a01, r11, g11, b11, a11;
        sample_mica_corner(tile_f_start_x + 0.5f, tile_f_start_y + 0.5f, inv_w, inv_h, *mica_grid, r00, g00, b00, a00);
        sample_mica_corner(tile_f_end_x - 0.5f,   tile_f_start_y + 0.5f, inv_w, inv_h, *mica_grid, r10, g10, b10, a10);
        sample_mica_corner(tile_f_start_x + 0.5f, tile_f_end_y - 0.5f,   inv_w, inv_h, *mica_grid, r01, g01, b01, a01);
        sample_mica_corner(tile_f_end_x - 0.5f,   tile_f_end_y - 0.5f,   inv_w, inv_h, *mica_grid, r11, g11, b11, a11);
        const float inv_tw = (tile_w > 1) ? (1.0f / static_cast<float>(tile_w - 1)) : 0.0f;
        const float inv_th = (tile_h > 1) ? (1.0f / static_cast<float>(tile_h - 1)) : 0.0f;
        for (uint32_t ly = 0; ly < tile_h; ++ly) {
            float ty = static_cast<float>(ly) * inv_th;
            float rl = r00 + (r01 - r00) * ty, rr = r10 + (r11 - r10) * ty;
            float gl = g00 + (g01 - g00) * ty, gr = g10 + (g11 - g10) * ty;
            float bl = b00 + (b01 - b00) * ty, br = b10 + (b11 - b10) * ty;
            float al = a00 + (a01 - a00) * ty, ar = a10 + (a11 - a10) * ty;
            float dr = (rr - rl) * inv_tw, dg = (gr - gl) * inv_tw, db = (br - bl) * inv_tw, da = (ar - al) * inv_tw;
            uint8_t* row_dst = dst_framebuffer + (static_cast<size_t>(start_y + ly) * width + start_x) * 4;
            if (enable_dithering || enable_acrylic_grain) {
                const uint32_t py = start_y + ly;
                constexpr float kInv255 = 1.0f / 255.0f;
                for (uint32_t lx = 0; lx < tile_w; ++lx) {
                    const float g_off = enable_acrylic_grain
                        ? (eval_acrylic_grain(static_cast<float>(start_x + lx) + 0.5f, static_cast<float>(py) + 0.5f, 1.0f) * 0.018f * al)
                        : 0.0f;
                    const float dither = (enable_dithering && al > 0.0f) ? ((eval_ign(start_x + lx, py) - 0.5f) * kInv255) : 0.0f;
                    const float base_r = std::clamp(rl + g_off, 0.0f, al);
                    const float base_g = std::clamp(gl + g_off, 0.0f, al);
                    const float base_b = std::clamp(bl + g_off, 0.0f, al);
                    row_dst[0] = static_cast<uint8_t>(std::clamp(base_r + dither, 0.0f, 1.0f) * 255.0f + 0.5f);
                    row_dst[1] = static_cast<uint8_t>(std::clamp(base_g + dither, 0.0f, 1.0f) * 255.0f + 0.5f);
                    row_dst[2] = static_cast<uint8_t>(std::clamp(base_b + dither, 0.0f, 1.0f) * 255.0f + 0.5f);
                    row_dst[3] = static_cast<uint8_t>(std::clamp(al, 0.0f, 1.0f) * 255.0f + 0.5f);
                    rl += dr; gl += dg; bl += db; al += da;
                    row_dst += 4;
                }
            } else {
                for (uint32_t lx = 0; lx < tile_w; ++lx) {
                    row_dst[0] = static_cast<uint8_t>(std::clamp(rl * 255.0f + 0.5f, 0.0f, 255.0f));
                    row_dst[1] = static_cast<uint8_t>(std::clamp(gl * 255.0f + 0.5f, 0.0f, 255.0f));
                    row_dst[2] = static_cast<uint8_t>(std::clamp(bl * 255.0f + 0.5f, 0.0f, 255.0f));
                    row_dst[3] = static_cast<uint8_t>(std::clamp(al * 255.0f + 0.5f, 0.0f, 255.0f));
                    rl += dr; gl += dg; bl += db; al += da;
                    row_dst += 4;
                }
            }
        }
    };

    // 0. 空瓦片极速早出
    if (prim_count == 0) {
        if (load_from_dst) return;
        if (!use_mica) {
            for (uint32_t py = start_y; py < end_y; ++py) {
                uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(py) * width + start_x) * 4);
                std::fill_n(row_dst, tile_w, 0u);
            }
            return;
        }
        fill_mica_to_fb();
        return;
    }

    // 1. 瓦片级图元预提炼与裁剪粗判 + 瓦片级不透明遮挡早出点探测
    struct PreparedPrim {
        const core::UiPrimitive* prim{nullptr};
        float center_x{0.0f}, center_y{0.0f};
        float half_w{0.0f}, half_h{0.0f};
        float r_tl{0.0f}, r_tr{0.0f}, r_br{0.0f}, r_bl{0.0f};
        float max_r{0.0f};
        bool has_shadow{false};
        float s_min_x{0.0f}, s_min_y{0.0f}, s_max_x{0.0f}, s_max_y{0.0f};
        float inv_sigma{0.0f};
        bool is_glyph{false};
        bool is_icon{false};
        bool is_gradient{false};
        bool is_oklab_gradient{false};
        bool is_reveal{false};
        bool is_rim{false};
        bool is_acrylic{false};
        bool is_inset{false};
        bool is_metaball{false};
        bool apply_font_gamma{false};
        bool full_tile_cover{false}; // 内接无圆角矩形完全覆盖本 16x16 瓦片 (cov == 1.0)
        float inv_pw{0.0f}, inv_ph{0.0f};
        float ax0{0.0f}, ay0{0.0f}, dax{0.0f}, day{0.0f};
        float font_px{13.0f};
        float fg_luma{0.0f};
        math::OkLab lab0{};
        math::OkLab lab1{};
        uint8_t clip_mode{0};
    };

    PreparedPrim prepared_prims[64];
    uint32_t active_prim_count = 0;
    int32_t last_opaque_cover = -1;

    for (uint32_t i = 0; i < prim_count && active_prim_count < 64; ++i) {
        uint32_t prim_idx = tile_indices[prim_offset + i];
        if (prim_idx < min_prim_id || prim_idx >= max_prim_id) continue;
        const auto& prim = primitives[prim_idx];
        if (load_from_dst && (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0) continue;

        uint8_t clip_mode = 0;
        if (prim.clip_index != core::INVALID_INDEX_U16 && prim.clip_index < clip_rects.size()) {
            uint32_t curr_clip = prim.clip_index;
            bool culled_away = false;
            uint32_t safety = 0;
            while (curr_clip != core::INVALID_INDEX_U32 && curr_clip < clip_rects.size() && safety < 64) {
                ++safety;
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
                                          clip.corner_radius <= 0.0f &&
                                          (clip.flags & 6u) == 0u);
                if (!tile_fully_inside) {
                    clip_mode = 1;
                }
                curr_clip = clip.parent_index;
            }
            if (culled_away) continue;
        }

        const uint32_t slot_idx = active_prim_count++;
        auto& p = prepared_prims[slot_idx];
        p.prim = &prim;
        p.clip_mode = clip_mode;

        p.center_x = (prim.min_x + prim.max_x) * 0.5f;
        p.center_y = (prim.min_y + prim.max_y) * 0.5f;
        p.half_w = (prim.max_x - prim.min_x) * 0.5f;
        p.half_h = (prim.max_y - prim.min_y) * 0.5f;

        float cap_r = std::min(p.half_w, p.half_h);
        p.r_tl = std::clamp(prim.radius_top_left, 0.0f, cap_r);
        p.r_tr = std::clamp(prim.radius_top_right, 0.0f, cap_r);
        p.r_br = std::clamp(prim.radius_bottom_right, 0.0f, cap_r);
        p.r_bl = std::clamp(prim.radius_bottom_left, 0.0f, cap_r);
        p.max_r = std::max({p.r_tl, p.r_tr, p.r_br, p.r_bl});

        const bool is_glyph = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsGlyph)) != 0;
        const bool is_icon = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsIcon)) != 0;
        const bool is_reveal = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsPointerReveal)) != 0;
        const bool is_rim = (prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasDirectionalRim)) != 0;
        const bool is_acrylic = ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasAcrylicGrain)) != 0) &&
                                !is_glyph && !is_icon;
        const bool is_backdrop = ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0) &&
                                 !is_glyph && !is_icon;
        const bool is_inset = ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasInsetShadow)) != 0) &&
                              !is_glyph && !is_icon;
        const bool is_metaball = ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::IsMetaball)) != 0) &&
                                 !is_glyph && !is_icon;
        const bool is_gradient = ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasLinearGradient)) != 0) &&
                                 !is_glyph && !is_icon && !is_reveal && !is_rim &&
                                 !is_acrylic && !is_backdrop && !is_inset && !is_metaball;
        p.is_glyph = is_glyph;
        p.is_icon = is_icon;
        p.is_reveal = is_reveal;
        p.is_rim = is_rim;
        p.is_acrylic = is_acrylic;
        p.is_inset = is_inset;
        p.is_metaball = is_metaball;
        p.is_gradient = is_gradient;
        p.is_oklab_gradient = is_gradient &&
                              ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::OkLabGradient)) != 0);

        if (p.is_oklab_gradient) {
            const math::Color s0 = (prim.color_a > 1e-4f)
                ? math::Color{std::clamp(prim.color_r / prim.color_a, 0.0f, 1.0f),
                              std::clamp(prim.color_g / prim.color_a, 0.0f, 1.0f),
                              std::clamp(prim.color_b / prim.color_a, 0.0f, 1.0f), prim.color_a}
                : math::Color{0.0f, 0.0f, 0.0f, 0.0f};
            const float c1r_p = static_cast<float>(prim.atlas_u0) * (1.0f / 65535.0f);
            const float c1g_p = static_cast<float>(prim.atlas_v0) * (1.0f / 65535.0f);
            const float c1b_p = static_cast<float>(prim.atlas_u1) * (1.0f / 65535.0f);
            const float c1a_p = static_cast<float>(prim.atlas_v1) * (1.0f / 65535.0f);
            const math::Color s1 = (c1a_p > 1e-4f)
                ? math::Color{std::clamp(c1r_p / c1a_p, 0.0f, 1.0f),
                              std::clamp(c1g_p / c1a_p, 0.0f, 1.0f),
                              std::clamp(c1b_p / c1a_p, 0.0f, 1.0f), c1a_p}
                : math::Color{0.0f, 0.0f, 0.0f, 0.0f};
            const math::Color eff0 = (s0.a <= 1e-4f && s1.a > 1e-4f) ? s1 : s0;
            const math::Color eff1 = (s1.a <= 1e-4f && s0.a > 1e-4f) ? s0 : s1;
            p.lab0 = math::srgb_to_oklab(eff0);
            p.lab1 = math::srgb_to_oklab(eff1);
        }

        if (!is_gradient && !is_reveal && !is_glyph && !is_icon && !is_rim &&
            !is_acrylic && !is_backdrop && !is_inset && !is_metaball && prim.shadow_blur > 0.0f) {
            p.has_shadow = true;
            p.s_min_x = prim.min_x - prim.shadow_spread + prim.shadow_offset_x;
            p.s_min_y = prim.min_y - prim.shadow_spread + prim.shadow_offset_y;
            p.s_max_x = prim.max_x + prim.shadow_spread + prim.shadow_offset_x;
            p.s_max_y = prim.max_y + prim.shadow_spread + prim.shadow_offset_y;
            p.inv_sigma = 1.0f / (prim.shadow_blur * 1.41421356f);
        }

        if (is_glyph || is_icon) {
            if (is_glyph) {
                p.apply_font_gamma = enable_font_gamma &&
                    ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::StemDarkening)) != 0);
                p.font_px = (prim.border_width > 0.5f) ? prim.border_width : 13.0f;
                const float inv_a = (prim.color_a > 1e-4f) ? (1.0f / prim.color_a) : 0.0f;
                p.fg_luma = std::clamp(
                    (0.2126f * prim.color_r + 0.7152f * prim.color_g + 0.0722f * prim.color_b) * inv_a,
                    0.0f, 1.0f);
            }
            float pw = prim.max_x - prim.min_x;
            float ph = prim.max_y - prim.min_y;
            const uint8_t* src_atlas = is_icon ? color_atlas_pixels : atlas_pixels;
            uint32_t src_w = is_icon ? color_atlas_width : atlas_width;
            uint32_t src_h = is_icon ? color_atlas_height : atlas_height;
            if (pw > 0.001f && ph > 0.001f && src_atlas && src_w > 0 && src_h > 0) {
                p.inv_pw = 1.0f / pw;
                p.inv_ph = 1.0f / ph;
                p.ax0 = (static_cast<float>(prim.atlas_u0) * (1.0f / 65535.0f)) * static_cast<float>(src_w);
                p.ay0 = (static_cast<float>(prim.atlas_v0) * (1.0f / 65535.0f)) * static_cast<float>(src_h);
                float ax1 = (static_cast<float>(prim.atlas_u1) * (1.0f / 65535.0f)) * static_cast<float>(src_w);
                float ay1 = (static_cast<float>(prim.atlas_v1) * (1.0f / 65535.0f)) * static_cast<float>(src_h);
                p.dax = ax1 - p.ax0;
                p.day = ay1 - p.ay0;
            }
        } else if (!is_gradient && !is_reveal && !is_rim && !is_acrylic && !is_inset && !is_metaball &&
                   clip_mode == 0 && !p.has_shadow && prim.border_width <= 0.0f) {
            // 判定是否完全覆盖本 16x16 瓦片 (内缩 max_r + 0.5px 保证所有像素 SDF <= -0.5 -> cov == 1.0)
            const float margin = p.max_r + 0.5f;
            if (prim.min_x + margin <= tile_f_start_x + 0.5f &&
                prim.max_x - margin >= tile_f_end_x - 0.5f &&
                prim.min_y + margin <= tile_f_start_y + 0.5f &&
                prim.max_y - margin >= tile_f_end_y - 0.5f)
            {
                p.full_tile_cover = true;
                if (prim.color_a >= 0.999f) {
                    last_opaque_cover = static_cast<int32_t>(slot_idx);
                }
            }
        }
    }

    if (active_prim_count == 0) {
        if (!use_mica) {
            for (uint32_t py = start_y; py < end_y; ++py) {
                uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(py) * width + start_x) * 4);
                std::fill_n(row_dst, tile_w, 0u);
            }
            return;
        }
        fill_mica_to_fb();
        return;
    }

    // 1.5 若最后一个有效图元就是完全覆盖本瓦片的实心不透明矩形，直接 memset 级填充早出
    // (开启抖动时，仅当三通道均接近精确 8-bit 整数值时才走快路径，保证亚量化半色阶仍受 IGN 抖动保护)
    if (last_opaque_cover == static_cast<int32_t>(active_prim_count - 1)) {
        const auto& prim = *prepared_prims[last_opaque_cover].prim;
        const float rf = prim.color_r * 255.0f;
        const float gf = prim.color_g * 255.0f;
        const float bf = prim.color_b * 255.0f;
        const bool exact_u8 = !enable_dithering ||
            (std::abs(rf - std::round(rf)) < 0.01f &&
             std::abs(gf - std::round(gf)) < 0.01f &&
             std::abs(bf - std::round(bf)) < 0.01f);
        if (exact_u8) {
            uint32_t r = static_cast<uint32_t>(std::clamp(rf + 0.5f, 0.0f, 255.0f));
            uint32_t g = static_cast<uint32_t>(std::clamp(gf + 0.5f, 0.0f, 255.0f));
            uint32_t b = static_cast<uint32_t>(std::clamp(bf + 0.5f, 0.0f, 255.0f));
            uint32_t pixel_val = r | (g << 8) | (b << 16) | (255u << 24);
            for (uint32_t py = start_y; py < end_y; ++py) {
                uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(py) * width + start_x) * 4);
                std::fill_n(row_dst, tile_w, pixel_val);
            }
            return;
        }
    }

    // 2. L1 缓存驻留的 16x16 瓦片 SoA 累加器 (4 KB，完美适配 AVX2/SSE2/NEON 自动向量化)
    if (load_from_dst && active_prim_count == 0) {
        return;
    }

    alignas(32) float tile_r[256];
    alignas(32) float tile_g[256];
    alignas(32) float tile_b[256];
    alignas(32) float tile_a[256];

    uint32_t first_prim_idx = 0;
    if (load_from_dst) {
        constexpr float kInv255 = 1.0f / 255.0f;
        for (uint32_t ly = 0; ly < tile_h; ++ly) {
            const uint32_t row_off = ly * 16u;
            const uint8_t* row_src = dst_framebuffer + (static_cast<size_t>(start_y + ly) * width + start_x) * 4u;
            for (uint32_t lx = 0; lx < tile_w; ++lx) {
                tile_r[row_off + lx] = static_cast<float>(row_src[lx * 4u + 0u]) * kInv255;
                tile_g[row_off + lx] = static_cast<float>(row_src[lx * 4u + 1u]) * kInv255;
                tile_b[row_off + lx] = static_cast<float>(row_src[lx * 4u + 2u]) * kInv255;
                tile_a[row_off + lx] = static_cast<float>(row_src[lx * 4u + 3u]) * kInv255;
            }
        }
    } else if (last_opaque_cover >= 0) {
        const auto& base = *prepared_prims[last_opaque_cover].prim;
        const float br = base.color_r, bg = base.color_g, bb = base.color_b, ba = base.color_a;
        const uint32_t total_px = tile_h * 16u;
        for (uint32_t k = 0; k < total_px; ++k) {
            tile_r[k] = br;
            tile_g[k] = bg;
            tile_b[k] = bb;
            tile_a[k] = ba;
        }
        first_prim_idx = static_cast<uint32_t>(last_opaque_cover + 1);
    } else if (use_mica) {
        const float inv_w = 1.0f / std::max(1.0f, static_cast<float>(width));
        const float inv_h = 1.0f / std::max(1.0f, static_cast<float>(height));
        float r00, g00, b00, a00, r10, g10, b10, a10, r01, g01, b01, a01, r11, g11, b11, a11;
        sample_mica_corner(tile_f_start_x + 0.5f, tile_f_start_y + 0.5f, inv_w, inv_h, *mica_grid, r00, g00, b00, a00);
        sample_mica_corner(tile_f_end_x - 0.5f,   tile_f_start_y + 0.5f, inv_w, inv_h, *mica_grid, r10, g10, b10, a10);
        sample_mica_corner(tile_f_start_x + 0.5f, tile_f_end_y - 0.5f,   inv_w, inv_h, *mica_grid, r01, g01, b01, a01);
        sample_mica_corner(tile_f_end_x - 0.5f,   tile_f_end_y - 0.5f,   inv_w, inv_h, *mica_grid, r11, g11, b11, a11);
        const float inv_tw = (tile_w > 1) ? (1.0f / static_cast<float>(tile_w - 1)) : 0.0f;
        const float inv_th = (tile_h > 1) ? (1.0f / static_cast<float>(tile_h - 1)) : 0.0f;
        for (uint32_t ly = 0; ly < tile_h; ++ly) {
            float ty = static_cast<float>(ly) * inv_th;
            float rl = r00 + (r01 - r00) * ty, rr = r10 + (r11 - r10) * ty;
            float gl = g00 + (g01 - g00) * ty, gr = g10 + (g11 - g10) * ty;
            float bl = b00 + (b01 - b00) * ty, br = b10 + (b11 - b10) * ty;
            float al = a00 + (a01 - a00) * ty, ar = a10 + (a11 - a10) * ty;
            float dr = (rr - rl) * inv_tw, dg = (gr - gl) * inv_tw, db = (br - bl) * inv_tw, da = (ar - al) * inv_tw;
            const uint32_t row_off = ly * 16u;
            const float py_c = tile_f_start_y + static_cast<float>(ly) + 0.5f;
            for (uint32_t lx = 0; lx < tile_w; ++lx) {
                float mr = rl, mg = gl, mb = bl;
                if (enable_acrylic_grain) {
                    const float g_off = eval_acrylic_grain(tile_f_start_x + static_cast<float>(lx) + 0.5f, py_c, 1.0f) * 0.018f * al;
                    mr = std::clamp(mr + g_off, 0.0f, al);
                    mg = std::clamp(mg + g_off, 0.0f, al);
                    mb = std::clamp(mb + g_off, 0.0f, al);
                }
                tile_r[row_off + lx] = mr;
                tile_g[row_off + lx] = mg;
                tile_b[row_off + lx] = mb;
                tile_a[row_off + lx] = al;
                rl += dr; gl += dg; bl += db; al += da;
            }
        }
    } else {
        const uint32_t total_px = tile_h * 16u;
        for (uint32_t k = 0; k < total_px; ++k) {
            tile_r[k] = 0.0f;
            tile_g[k] = 0.0f;
            tile_b[k] = 0.0f;
            tile_a[k] = 0.0f;
        }
    }

    // 3. 图元主序 (Primitive-Major) 分特化紧致循环合成
    // 彻底消除内层像素循环中的分支预测失败、重复裁剪/元数据读取与无效包围盒外像素计算
    for (uint32_t i = first_prim_idx; i < active_prim_count; ++i) {
        const auto& prep = prepared_prims[i];
        const auto& prim = *prep.prim;

        // 通道 A: 全瓦片无圆角覆盖极速 SIMD 混合 (零 SDF 计算)
        if (prep.full_tile_cover) {
            const float sr = prim.color_r, sg = prim.color_g, sb = prim.color_b, sa = prim.color_a;
            const float one_minus_sa = 1.0f - sa;
            for (uint32_t ly = 0; ly < tile_h; ++ly) {
                const uint32_t row_off = ly * 16u;
                for (uint32_t lx = 0; lx < tile_w; ++lx) {
                    const uint32_t idx = row_off + lx;
                    tile_r[idx] = sr + tile_r[idx] * one_minus_sa;
                    tile_g[idx] = sg + tile_g[idx] * one_minus_sa;
                    tile_b[idx] = sb + tile_b[idx] * one_minus_sa;
                    tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                }
            }
            continue;
        }

        // 计算图元在本 16x16 瓦片内的精确局部包围盒 [lx0, lx1) x [ly0, ly1)
        float bmin_x = prim.min_x - 1.0f;
        float bmin_y = prim.min_y - 1.0f;
        float bmax_x = prim.max_x + 1.0f;
        float bmax_y = prim.max_y + 1.0f;
        if (prep.has_shadow) {
            float pad = prim.shadow_blur * 3.0f + 1.0f;
            bmin_x = std::min(bmin_x, prep.s_min_x - pad);
            bmin_y = std::min(bmin_y, prep.s_min_y - pad);
            bmax_x = std::max(bmax_x, prep.s_max_x + pad);
            bmax_y = std::max(bmax_y, prep.s_max_y + pad);
        }
        int32_t ix0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(bmin_x - tile_f_start_x)));
        int32_t iy0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(bmin_y - tile_f_start_y)));
        int32_t ix1 = std::min<int32_t>(static_cast<int32_t>(tile_w), static_cast<int32_t>(std::ceil(bmax_x - tile_f_start_x)));
        int32_t iy1 = std::min<int32_t>(static_cast<int32_t>(tile_h), static_cast<int32_t>(std::ceil(bmax_y - tile_f_start_y)));
        if (ix0 >= ix1 || iy0 >= iy1) continue;

        const uint32_t lx0 = static_cast<uint32_t>(ix0);
        const uint32_t ly0 = static_cast<uint32_t>(iy0);
        const uint32_t lx1 = static_cast<uint32_t>(ix1);
        const uint32_t ly1 = static_cast<uint32_t>(iy1);
        const bool need_clip = (prep.clip_mode == 1);

        // 通道 B: 字形 R8 蒙版极速光栅化 (完全跳过 SDF 计算，外提行级 fy 与 1:1 极速采样路径)
        if (prep.is_glyph) {
            if (prep.inv_pw <= 0.0f || !atlas_pixels || atlas_width == 0 || atlas_height == 0) continue;
            const float step_x = prep.inv_pw * prep.dax;
            const float step_y = prep.inv_ph * prep.day;
            const float cr = prim.color_r, cg = prim.color_g, cb = prim.color_b, ca = prim.color_a;
            const int32_t aw_m1 = static_cast<int32_t>(atlas_width) - 1;
            const int32_t ah_m1 = static_cast<int32_t>(atlas_height) - 1;

            // 检测 1:1 整数对齐字形 (桌面清晰文字常见情形，单字节直读免 4 点双线性插值)
            const float first_px = tile_f_start_x + static_cast<float>(lx0) + 0.5f;
            const float first_py = tile_f_start_y + static_cast<float>(ly0) + 0.5f;
            const float fx0_test = prep.ax0 + (first_px - prim.min_x) * step_x - 0.5f;
            const float fy0_test = prep.ay0 + (first_py - prim.min_y) * step_y - 0.5f;
            const bool is_1to1 = (std::abs(step_x - 1.0f) < 0.002f && std::abs(step_y - 1.0f) < 0.002f &&
                                  std::abs(fx0_test - std::round(fx0_test)) < 0.03f &&
                                  std::abs(fy0_test - std::round(fy0_test)) < 0.03f);

            for (uint32_t ly = ly0; ly < ly1; ++ly) {
                const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
                if (p_y < prim.min_y || p_y > prim.max_y) continue;
                const uint32_t row_off = ly * 16u;

                if (is_1to1) {
                    int32_t sy = std::clamp(static_cast<int32_t>(std::lround(prep.ay0 + (p_y - prim.min_y) * step_y - 0.5f)), 0, ah_m1);
                    const uint8_t* arow = atlas_pixels + static_cast<size_t>(sy) * atlas_width;
                    int32_t sx_base = static_cast<int32_t>(std::lround(prep.ax0 + (tile_f_start_x + 0.5f - prim.min_x) * step_x - 0.5f));
                    for (uint32_t lx = lx0; lx < lx1; ++lx) {
                        const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                        if (p_x < prim.min_x || p_x > prim.max_x) continue;
                        const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                        if (clip_a <= 0.0001f) continue;
                        int32_t sx = std::clamp(sx_base + static_cast<int32_t>(lx), 0, aw_m1);
                        uint8_t cov_u8 = arow[sx];
                        if (cov_u8 == 0) continue;
                        float cov = static_cast<float>(cov_u8) * (1.0f / 255.0f);
                        const uint32_t idx = row_off + lx;
                        if (prep.apply_font_gamma && cov_u8 < 255) {
                            const float dst_a = tile_a[idx];
                            const float bg_luma = (dst_a > 1e-4f)
                                ? std::clamp((0.2126f * tile_r[idx] + 0.7152f * tile_g[idx] + 0.0722f * tile_b[idx]) / dst_a, 0.0f, 1.0f)
                                : 0.5f;
                            cov = text::StemDarkener::evaluate_irradiation_coverage(cov, prep.font_px, prep.fg_luma, bg_luma, dst_a);
                        }
                        cov *= clip_a;
                        const float sa = ca * cov;
                        const float one_minus_sa = 1.0f - sa;
                        tile_r[idx] = cr * cov + tile_r[idx] * one_minus_sa;
                        tile_g[idx] = cg * cov + tile_g[idx] * one_minus_sa;
                        tile_b[idx] = cb * cov + tile_b[idx] * one_minus_sa;
                        tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                    }
                } else {
                    const float fy = prep.ay0 + (p_y - prim.min_y) * step_y - 0.5f;
                    const int32_t ay0_i = fast_floor_i32(fy);
                    const float ty = fy - static_cast<float>(ay0_i);
                    const int32_t sy0 = std::clamp(ay0_i, 0, ah_m1);
                    const int32_t sy1 = std::clamp(ay0_i + 1, 0, ah_m1);
                    const uint8_t* arow0 = atlas_pixels + static_cast<size_t>(sy0) * atlas_width;
                    const uint8_t* arow1 = atlas_pixels + static_cast<size_t>(sy1) * atlas_width;
                    float fx = prep.ax0 + (tile_f_start_x + static_cast<float>(lx0) + 0.5f - prim.min_x) * step_x - 0.5f;
                    for (uint32_t lx = lx0; lx < lx1; ++lx, fx += step_x) {
                        const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                        if (p_x < prim.min_x || p_x > prim.max_x) continue;
                        const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                        if (clip_a <= 0.0001f) continue;
                        const int32_t ax0_i = fast_floor_i32(fx);
                        const float tx = fx - static_cast<float>(ax0_i);
                        const int32_t sx0 = std::clamp(ax0_i, 0, aw_m1);
                        const int32_t sx1 = std::clamp(ax0_i + 1, 0, aw_m1);
                        float c0 = static_cast<float>(arow0[sx0]) + tx * static_cast<float>(static_cast<int32_t>(arow0[sx1]) - static_cast<int32_t>(arow0[sx0]));
                        float c1 = static_cast<float>(arow1[sx0]) + tx * static_cast<float>(static_cast<int32_t>(arow1[sx1]) - static_cast<int32_t>(arow1[sx0]));
                        float cov = (c0 + ty * (c1 - c0)) * (1.0f / 255.0f);
                        if (cov <= 0.0001f) continue;
                        const uint32_t idx = row_off + lx;
                        if (prep.apply_font_gamma && cov < 0.9999f) {
                            const float dst_a = tile_a[idx];
                            const float bg_luma = (dst_a > 1e-4f)
                                ? std::clamp((0.2126f * tile_r[idx] + 0.7152f * tile_g[idx] + 0.0722f * tile_b[idx]) / dst_a, 0.0f, 1.0f)
                                : 0.5f;
                            cov = text::StemDarkener::evaluate_irradiation_coverage(cov, prep.font_px, prep.fg_luma, bg_luma, dst_a);
                        }
                        cov *= clip_a;
                        const float sa = ca * cov;
                        const float one_minus_sa = 1.0f - sa;
                        tile_r[idx] = cr * cov + tile_r[idx] * one_minus_sa;
                        tile_g[idx] = cg * cov + tile_g[idx] * one_minus_sa;
                        tile_b[idx] = cb * cov + tile_b[idx] * one_minus_sa;
                        tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                    }
                }
            }
            continue;
        }

        // 通道 C: 彩色 RGBA8 位图采样 (外提行级 fy，仅当存在圆角时才计算 SDF)
        if (prep.is_icon) {
            if (prep.inv_pw <= 0.0f || !color_atlas_pixels || color_atlas_width == 0 || color_atlas_height == 0) continue;
            const float step_x = prep.inv_pw * prep.dax;
            const float step_y = prep.inv_ph * prep.day;
            const int32_t aw_m1 = static_cast<int32_t>(color_atlas_width) - 1;
            const int32_t ah_m1 = static_cast<int32_t>(color_atlas_height) - 1;
            const bool has_radius = (prep.max_r > 0.0f);

            for (uint32_t ly = ly0; ly < ly1; ++ly) {
                const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
                if (p_y < prim.min_y || p_y > prim.max_y) continue;
                const uint32_t row_off = ly * 16u;
                const float fy = prep.ay0 + (p_y - prim.min_y) * step_y - 0.5f;
                const int32_t ay0_i = fast_floor_i32(fy);
                const float ty = fy - static_cast<float>(ay0_i);
                const float omt = 1.0f - ty;
                const int32_t sy0 = std::clamp(ay0_i, 0, ah_m1);
                const int32_t sy1 = std::clamp(ay0_i + 1, 0, ah_m1);
                const uint8_t* crow0 = color_atlas_pixels + static_cast<size_t>(sy0) * color_atlas_width * 4u;
                const uint8_t* crow1 = color_atlas_pixels + static_cast<size_t>(sy1) * color_atlas_width * 4u;
                float fx = prep.ax0 + (tile_f_start_x + static_cast<float>(lx0) + 0.5f - prim.min_x) * step_x - 0.5f;

                for (uint32_t lx = lx0; lx < lx1; ++lx, fx += step_x) {
                    const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                    if (p_x < prim.min_x || p_x > prim.max_x) continue;
                    const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                    if (clip_a <= 0.0001f) continue;

                    float geom_cov = 1.0f;
                    if (has_radius) {
                        float r = (p_x > prep.center_x)
                            ? ((p_y > prep.center_y) ? prep.r_br : prep.r_tr)
                            : ((p_y > prep.center_y) ? prep.r_bl : prep.r_tl);
                        float dist = eval_squircle_box_distance(
                            std::abs(p_x - prep.center_x), std::abs(p_y - prep.center_y),
                            prep.half_w, prep.half_h, r);
                        geom_cov = evaluate_coverage(dist);
                        if (geom_cov <= 0.0001f) continue;
                    }

                    const int32_t ax0_i = fast_floor_i32(fx);
                    const float tx = fx - static_cast<float>(ax0_i);
                    const int32_t sx0 = std::clamp(ax0_i, 0, aw_m1);
                    const int32_t sx1 = std::clamp(ax0_i + 1, 0, aw_m1);
                    const uint8_t* p00 = crow0 + static_cast<size_t>(sx0) * 4u;
                    const uint8_t* p10 = crow0 + static_cast<size_t>(sx1) * 4u;
                    const uint8_t* p01 = crow1 + static_cast<size_t>(sx0) * 4u;
                    const uint8_t* p11 = crow1 + static_cast<size_t>(sx1) * 4u;

                    constexpr float kInv255 = 1.0f / 255.0f;
                    float tr = ((p00[0] + tx * (p10[0] - p00[0])) * omt + (p01[0] + tx * (p11[0] - p01[0])) * ty) * kInv255;
                    float tg = ((p00[1] + tx * (p10[1] - p00[1])) * omt + (p01[1] + tx * (p11[1] - p01[1])) * ty) * kInv255;
                    float tb = ((p00[2] + tx * (p10[2] - p00[2])) * omt + (p01[2] + tx * (p11[2] - p01[2])) * ty) * kInv255;
                    float ta = ((p00[3] + tx * (p10[3] - p00[3])) * omt + (p01[3] + tx * (p11[3] - p01[3])) * ty) * kInv255;

                    const float factor = ta * geom_cov * clip_a;
                    const float src_r = tr * factor * prim.color_r;
                    const float src_g = tg * factor * prim.color_g;
                    const float src_b = tb * factor * prim.color_b;
                    const float src_a = factor * prim.color_a;
                    const float one_minus_sa = 1.0f - src_a;
                    const uint32_t idx = row_off + lx;
                    tile_r[idx] = src_r + tile_r[idx] * one_minus_sa;
                    tile_g[idx] = src_g + tile_g[idx] * one_minus_sa;
                    tile_b[idx] = src_b + tile_b[idx] * one_minus_sa;
                    tile_a[idx] = src_a + tile_a[idx] * one_minus_sa;
                }
            }
            continue;
        }

        // 通道 D: 无圆角无描边无阴影轴对齐矩形极速通道 (零 sqrt、零四象限分支)
        if (!prep.is_gradient && !prep.is_reveal && !prep.is_rim &&
            !prep.is_acrylic && !prep.is_inset && !prep.is_metaball &&
            prep.max_r <= 0.0f && prim.border_width <= 0.0f && !prep.has_shadow) {
            const float cr = prim.color_r, cg = prim.color_g, cb = prim.color_b, ca = prim.color_a;
            for (uint32_t ly = ly0; ly < ly1; ++ly) {
                const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
                const float dy = std::abs(p_y - prep.center_y) - prep.half_h;
                const float cov_y = std::clamp(0.5f - dy, 0.0f, 1.0f);
                if (cov_y <= 0.0001f) continue;
                const uint32_t row_off = ly * 16u;
                for (uint32_t lx = lx0; lx < lx1; ++lx) {
                    const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                    const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                    if (clip_a <= 0.0001f) continue;
                    const float dx = std::abs(p_x - prep.center_x) - prep.half_w;
                    const float cov = std::min(cov_y, std::clamp(0.5f - dx, 0.0f, 1.0f)) * clip_a;
                    if (cov <= 0.0001f) continue;
                    const float sa = ca * cov;
                    const float one_minus_sa = 1.0f - sa;
                    const uint32_t idx = row_off + lx;
                    tile_r[idx] = cr * cov + tile_r[idx] * one_minus_sa;
                    tile_g[idx] = cg * cov + tile_g[idx] * one_minus_sa;
                    tile_b[idx] = cb * cov + tile_b[idx] * one_minus_sa;
                    tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                }
            }
            continue;
        }

        // 通道 R3: 指针跟随径向高光 / 边缘揭示通道
        if (prep.is_reveal) {
            const float reveal_radius = prim.shadow_blur;
            if (reveal_radius <= 0.001f) continue;
            const float cursor_x = prim.shadow_offset_x;
            const float cursor_y = prim.shadow_offset_y;
            const float inv_radius = 1.0f / reveal_radius;
            const float intensity = (prim.shadow_spread > 0.0f) ? prim.shadow_spread : 1.0f;
            const bool is_edge = (prim.border_width > 0.0f) ||
                                 ((prim.flags & static_cast<uint16_t>(core::PrimitiveFlags::HasBorder)) != 0);
            const float bw = (prim.border_width > 0.0f) ? prim.border_width : 1.0f;
            const float half_b = bw * 0.5f;

            bool use_clip_bounds = false;
            float clip_cx = 0.0f, clip_cy = 0.0f, clip_hx = 0.0f, clip_hy = 0.0f, clip_r = 0.0f;
            if (prim.clip_index != core::INVALID_INDEX_U16 && prim.clip_index < clip_rects.size()) {
                const auto& cl = clip_rects[prim.clip_index];
                if ((cl.flags & core::kClipFlagRevealBounds) != 0u) {
                    use_clip_bounds = true;
                    clip_cx = (cl.min_x + cl.max_x) * 0.5f;
                    clip_cy = (cl.min_y + cl.max_y) * 0.5f;
                    clip_hx = (cl.max_x - cl.min_x) * 0.5f;
                    clip_hy = (cl.max_y - cl.min_y) * 0.5f;
                    clip_r = cl.corner_radius;
                }
            }

            const float cr = prim.color_r, cg = prim.color_g, cb = prim.color_b, ca = prim.color_a;
            for (uint32_t ly = ly0; ly < ly1; ++ly) {
                const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
                if (p_y < prim.min_y || p_y > prim.max_y) continue;
                const uint32_t row_off = ly * 16u;
                const float dy_r = p_y - cursor_y;
                const bool y_gt = (p_y > prep.center_y);
                const float r_left = y_gt ? prep.r_bl : prep.r_tl;
                const float r_right = y_gt ? prep.r_br : prep.r_tr;
                const float eval_cy = use_clip_bounds ? clip_cy : prep.center_y;
                const float eval_cx = use_clip_bounds ? clip_cx : prep.center_x;
                const float eval_hx = use_clip_bounds ? clip_hx : prep.half_w;
                const float eval_hy = use_clip_bounds ? clip_hy : prep.half_h;
                const float abs_dy = std::abs(p_y - eval_cy);

                for (uint32_t lx = lx0; lx < lx1; ++lx) {
                    const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                    if (p_x < prim.min_x || p_x > prim.max_x) continue;
                    const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                    if (clip_a <= 0.0001f) continue;
                    const float dx_r = p_x - cursor_x;
                    const float dist_r = std::sqrt(dx_r * dx_r + dy_r * dy_r);
                    if (dist_r >= reveal_radius) continue;
                    const float u = std::clamp(1.0f - dist_r * inv_radius, 0.0f, 1.0f);
                    const float radial = u * u;
                    const float eval_r = use_clip_bounds ? clip_r : ((p_x > prep.center_x) ? r_right : r_left);
                    float dist = eval_squircle_box_distance(std::abs(p_x - eval_cx), abs_dy, eval_hx, eval_hy, eval_r);
                    if (is_edge) {
                        dist = std::abs(dist + half_b) - half_b;
                    }
                    const float geom_cov = evaluate_coverage(dist);
                    const float cov = std::clamp(geom_cov * radial * intensity, 0.0f, 1.0f) * clip_a;
                    if (cov > 0.0001f) {
                        const float sa = ca * cov;
                        const float one_minus_sa = 1.0f - sa;
                        const uint32_t idx = row_off + lx;
                        tile_r[idx] = cr * cov + tile_r[idx] * one_minus_sa;
                        tile_g[idx] = cg * cov + tile_g[idx] * one_minus_sa;
                        tile_b[idx] = cb * cov + tile_b[idx] * one_minus_sa;
                        tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                    }
                }
            }
            continue;
        }

        // 通道 Rim: 方向性玻璃高光内描边 (Directional Fresnel Rim Highlight)
        if (prep.is_rim) {
            const float rim_w = std::max(prim.border_width, 1.0f);
            const float half_rim = rim_w * 0.5f;
            const float fresnel_glow = std::max(prim.shadow_spread, 0.0f);
            float lx_dir = prim.shadow_offset_x;
            float ly_dir = prim.shadow_offset_y;
            const float l_len = std::hypot(lx_dir, ly_dir);
            if (l_len > 1e-4f) {
                lx_dir /= l_len;
                ly_dir /= l_len;
            } else {
                lx_dir = 0.0f;
                ly_dir = -1.0f;
            }
            const float tr = prim.color_r, tg = prim.color_g, tb = prim.color_b, ta = prim.color_a;
            const float br = static_cast<float>(prim.atlas_u0) * (1.0f / 65535.0f);
            const float bg = static_cast<float>(prim.atlas_v0) * (1.0f / 65535.0f);
            const float bb = static_cast<float>(prim.atlas_u1) * (1.0f / 65535.0f);
            const float ba = static_cast<float>(prim.atlas_v1) * (1.0f / 65535.0f);

            auto sample_dist = [&](float px, float py) noexcept -> float {
                const float r = (px > prep.center_x)
                    ? ((py > prep.center_y) ? prep.r_br : prep.r_tr)
                    : ((py > prep.center_y) ? prep.r_bl : prep.r_tl);
                return eval_squircle_box_distance(
                    std::abs(px - prep.center_x), std::abs(py - prep.center_y),
                    prep.half_w, prep.half_h, r);
            };

            for (uint32_t ly = ly0; ly < ly1; ++ly) {
                const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
                const uint32_t row_off = ly * 16u;
                for (uint32_t lx = lx0; lx < lx1; ++lx) {
                    const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                    const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                    if (clip_a <= 0.0001f) continue;

                    const float d = sample_dist(p_x, p_y);
                    const float d_rim = std::abs(d + half_rim) - half_rim;
                    const float stroke_cov = evaluate_coverage(d_rim);
                    const float glow_cov = (d <= 0.0f && fresnel_glow > 0.001f)
                        ? std::exp(d * 0.55f) * fresnel_glow * 0.35f
                        : 0.0f;
                    if (stroke_cov <= 0.0001f && glow_cov <= 0.0001f) continue;

                    const float nx_raw = sample_dist(p_x + 0.5f, p_y) - sample_dist(p_x - 0.5f, p_y);
                    const float ny_raw = sample_dist(p_x, p_y + 0.5f) - sample_dist(p_x, p_y - 0.5f);
                    const float n_len = std::hypot(nx_raw, ny_raw);
                    const float nx = (n_len > 1e-5f) ? (nx_raw / n_len) : 0.0f;
                    const float ny = (n_len > 1e-5f) ? (ny_raw / n_len) : -1.0f;

                    const float ndotl = std::clamp(nx * lx_dir + ny * ly_dir, -1.0f, 1.0f);
                    const float top_factor = std::pow(std::clamp(ndotl * 0.5f + 0.5f, 0.0f, 1.0f), 1.8f);
                    const float grazing = 1.0f - std::abs(ndotl);
                    const float top_w = std::clamp(top_factor + 0.22f * grazing * std::clamp(ndotl + 0.35f, 0.0f, 1.0f), 0.0f, 1.0f);
                    const float bot_w = std::pow(std::clamp(-ndotl, 0.0f, 1.0f), 1.4f);

                    const float rim_a = std::clamp(ta * top_w + ba * bot_w, 0.0f, 1.0f);
                    const float rim_r = std::clamp(tr * top_w + br * bot_w, 0.0f, rim_a);
                    const float rim_g = std::clamp(tg * top_w + bg * bot_w, 0.0f, rim_a);
                    const float rim_b = std::clamp(tb * top_w + bb * bot_w, 0.0f, rim_a);

                    const float st_r = rim_r * stroke_cov;
                    const float st_g = rim_g * stroke_cov;
                    const float st_b = rim_b * stroke_cov;
                    const float st_a = rim_a * stroke_cov;

                    const float gl_f = std::clamp(ndotl, 0.0f, 1.0f) * glow_cov;
                    const float gl_a = std::clamp(ta * gl_f, 0.0f, 1.0f);
                    const float gl_r = std::clamp(tr * gl_f, 0.0f, gl_a);
                    const float gl_g = std::clamp(tg * gl_f, 0.0f, gl_a);
                    const float gl_b = std::clamp(tb * gl_f, 0.0f, gl_a);

                    const float oma_st = 1.0f - st_a;
                    const float base_a = std::clamp(st_a + gl_a * oma_st, 0.0f, 1.0f);
                    const float base_r = std::clamp(st_r + gl_r * oma_st, 0.0f, base_a);
                    const float base_g = std::clamp(st_g + gl_g * oma_st, 0.0f, base_a);
                    const float base_b = std::clamp(st_b + gl_b * oma_st, 0.0f, base_a);
                    const float src_a = base_a * clip_a;
                    const float src_r = base_r * clip_a;
                    const float src_g = base_g * clip_a;
                    const float src_b = base_b * clip_a;

                    const float one_minus_sa = 1.0f - src_a;
                    const uint32_t idx = row_off + lx;
                    tile_r[idx] = src_r + tile_r[idx] * one_minus_sa;
                    tile_g[idx] = src_g + tile_g[idx] * one_minus_sa;
                    tile_b[idx] = src_b + tile_b[idx] * one_minus_sa;
                    tile_a[idx] = src_a + tile_a[idx] * one_minus_sa;
                }
            }
            continue;
        }

        // 通道 InsetShadow: 解析高斯内阴影 (Analytic SDF Inset Shadow)
        if (prep.is_inset) {
            const float cr = prim.color_r, cg = prim.color_g, cb = prim.color_b, ca = prim.color_a;
            const float ox = prim.shadow_offset_x;
            const float oy = prim.shadow_offset_y;
            const float spread = prim.shadow_spread;
            const float inv_s = 1.0f / (std::max(prim.shadow_blur, 0.25f) * 1.41421356f);

            auto sample_dist = [&](float px, float py) noexcept -> float {
                const float r = (px > prep.center_x)
                    ? ((py > prep.center_y) ? prep.r_br : prep.r_tr)
                    : ((py > prep.center_y) ? prep.r_bl : prep.r_tl);
                return eval_squircle_box_distance(
                    std::abs(px - prep.center_x), std::abs(py - prep.center_y),
                    prep.half_w, prep.half_h, r);
            };
            auto sample_shifted = [&](float px, float py) noexcept -> float {
                const float dx = px - ox - prep.center_x;
                const float dy = py - oy - prep.center_y;
                const float r = (dx > 0.0f)
                    ? ((dy > 0.0f) ? prep.r_br : prep.r_tr)
                    : ((dy > 0.0f) ? prep.r_bl : prep.r_tl);
                return eval_squircle_box_distance(
                    std::abs(dx), std::abs(dy), prep.half_w, prep.half_h, r);
            };

            for (uint32_t ly = ly0; ly < ly1; ++ly) {
                const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
                const uint32_t row_off = ly * 16u;
                for (uint32_t lx = lx0; lx < lx1; ++lx) {
                    const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                    const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                    if (clip_a <= 0.0001f) continue;

                    const float cov_outer = evaluate_coverage(sample_dist(p_x, p_y));
                    if (cov_outer <= 0.0001f) continue;

                    const float d_in = sample_shifted(p_x, p_y) + spread;
                    const float erfv = erf_approx(d_in * inv_s);
                    const float inset_a = std::clamp(0.5f * (1.0f + erfv), 0.0f, 1.0f) * cov_outer * clip_a;
                    if (inset_a <= 0.0001f) continue;

                    const float sa = ca * inset_a;
                    const float one_minus_sa = 1.0f - sa;
                    const uint32_t idx = row_off + lx;
                    tile_r[idx] = cr * inset_a + tile_r[idx] * one_minus_sa;
                    tile_g[idx] = cg * inset_a + tile_g[idx] * one_minus_sa;
                    tile_b[idx] = cb * inset_a + tile_b[idx] * one_minus_sa;
                    tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                }
            }
            continue;
        }

        // 通道 Metaball: 灵动岛式液态融合 (SDF Smooth-Minimum Metaballs)
        if (prep.is_metaball) {
            const float ax0 = prim.radius_top_left;
            const float ay0 = prim.radius_top_right;
            const float ax1 = prim.radius_bottom_right;
            const float ay1 = prim.radius_bottom_left;
            const float acx = (ax0 + ax1) * 0.5f;
            const float acy = (ay0 + ay1) * 0.5f;
            const float ahw = std::max(0.0f, (ax1 - ax0) * 0.5f);
            const float ahh = std::max(0.0f, (ay1 - ay0) * 0.5f);

            const float pw = prim.max_x - prim.min_x;
            const float ph = prim.max_y - prim.min_y;
            const float bx0 = prim.min_x + (static_cast<float>(prim.atlas_u0) * (1.0f / 65535.0f)) * pw;
            const float by0 = prim.min_y + (static_cast<float>(prim.atlas_v0) * (1.0f / 65535.0f)) * ph;
            const float bx1 = prim.min_x + (static_cast<float>(prim.atlas_u1) * (1.0f / 65535.0f)) * pw;
            const float by1 = prim.min_y + (static_cast<float>(prim.atlas_v1) * (1.0f / 65535.0f)) * ph;
            const float bcx = (bx0 + bx1) * 0.5f;
            const float bcy = (by0 + by1) * 0.5f;
            const float bhw = std::max(0.0f, (bx1 - bx0) * 0.5f);
            const float bhh = std::max(0.0f, (by1 - by0) * 0.5f);

            const float ra = std::max(prim.shadow_spread, 0.0f);
            const float rb = std::max(prim.shadow_offset_x, 0.0f);
            const float k = std::max(prim.shadow_blur, 0.0f);
            const float bw = prim.border_width;
            const float half_bw = bw * 0.5f;

            const bool two_tone = (prim.shadow_offset_y > 0.5f);
            math::OkLab meta_lab_a{}, meta_lab_b{};
            float ba_srgb = prim.color_a;
            if (two_tone) {
                const uint32_t pk = static_cast<uint32_t>(std::lround(prim.shadow_offset_y)) - 1u;
                ba_srgb = static_cast<float>((pk >> 18u) & 63u) * (1.0f / 63.0f);
                const math::Color s_b{
                    static_cast<float>(pk & 63u) * (1.0f / 63.0f),
                    static_cast<float>((pk >> 6u) & 63u) * (1.0f / 63.0f),
                    static_cast<float>((pk >> 12u) & 63u) * (1.0f / 63.0f),
                    ba_srgb
                };
                const math::Color s_a = (prim.color_a > 1e-4f)
                    ? math::Color{std::clamp(prim.color_r / prim.color_a, 0.0f, 1.0f),
                                  std::clamp(prim.color_g / prim.color_a, 0.0f, 1.0f),
                                  std::clamp(prim.color_b / prim.color_a, 0.0f, 1.0f), prim.color_a}
                    : math::Color{0.0f, 0.0f, 0.0f, 0.0f};
                const math::Color eff_a = (s_a.a <= 1e-4f && s_b.a > 1e-4f) ? s_b : s_a;
                const math::Color eff_b = (s_b.a <= 1e-4f && s_a.a > 1e-4f) ? s_a : s_b;
                meta_lab_a = math::srgb_to_oklab(eff_a);
                meta_lab_b = math::srgb_to_oklab(eff_b);
            }

            const float cr = prim.color_r, cg = prim.color_g, cb = prim.color_b, ca = prim.color_a;
            for (uint32_t ly = ly0; ly < ly1; ++ly) {
                const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
                const uint32_t row_off = ly * 16u;
                const float ady = std::abs(p_y - acy);
                const float bdy = std::abs(p_y - bcy);
                for (uint32_t lx = lx0; lx < lx1; ++lx) {
                    const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                    const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                    if (clip_a <= 0.0001f) continue;

                    const float dA = eval_squircle_box_distance(std::abs(p_x - acx), ady, ahw, ahh, ra);
                    const float dB = eval_squircle_box_distance(std::abs(p_x - bcx), bdy, bhw, bhh, rb);
                    const float h = (k > 0.001f)
                        ? std::clamp(0.5f + 0.5f * (dB - dA) / k, 0.0f, 1.0f)
                        : ((dA <= dB) ? 1.0f : 0.0f);
                    float d_fused = dB + (dA - dB) * h - k * h * (1.0f - h);
                    if (bw > 0.0f) {
                        d_fused = std::abs(d_fused + half_bw) - half_bw;
                    }
                    const float cov = evaluate_coverage(d_fused) * clip_a;
                    if (cov <= 0.0001f) continue;

                    float sr, sg, sb, sa_base;
                    if (two_tone) {
                        sa_base = std::clamp(ba_srgb + (ca - ba_srgb) * h, 0.0f, 1.0f);
                        const math::OkLab mid_lab{
                            meta_lab_b.L + (meta_lab_a.L - meta_lab_b.L) * h,
                            meta_lab_b.a + (meta_lab_a.a - meta_lab_b.a) * h,
                            meta_lab_b.b + (meta_lab_a.b - meta_lab_b.b) * h
                        };
                        const math::Color mid_srgb = math::oklab_to_srgb(mid_lab, sa_base);
                        sr = mid_srgb.r * sa_base;
                        sg = mid_srgb.g * sa_base;
                        sb = mid_srgb.b * sa_base;
                    } else {
                        sr = cr; sg = cg; sb = cb; sa_base = ca;
                    }
                    const float sa = sa_base * cov;
                    const float one_minus_sa = 1.0f - sa;
                    const uint32_t idx = row_off + lx;
                    tile_r[idx] = sr * cov + tile_r[idx] * one_minus_sa;
                    tile_g[idx] = sg * cov + tile_g[idx] * one_minus_sa;
                    tile_b[idx] = sb * cov + tile_b[idx] * one_minus_sa;
                    tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                }
            }
            continue;
        }

        // 通道 E: 通用圆角 SDF / 描边 / 解析高斯阴影 / 线性渐变 / 亚克力微颗粒通道 (仅遍历包围盒内有效像素)
        const bool has_border = (prim.border_width > 0.0f);
        const float half_border = prim.border_width * 0.5f;
        const float cr = prim.color_r, cg = prim.color_g, cb = prim.color_b, ca = prim.color_a;
        const float acrylic_scale = prep.is_acrylic ? std::max(prim.shadow_blur, 1.0f) : 1.0f;
        const float acrylic_amp = prep.is_acrylic ? std::clamp(prim.shadow_spread, 0.0f, 0.25f) : 0.0f;

        float gx0 = prim.shadow_offset_x, gy0 = prim.shadow_offset_y;
        float dx_g = prim.shadow_blur - gx0;
        float dy_g = prim.shadow_spread - gy0;
        float len2_g = dx_g * dx_g + dy_g * dy_g;
        if (prep.is_gradient && len2_g <= 1e-6f) {
            gx0 = prim.min_x;
            gy0 = prim.min_y;
            dx_g = 0.0f;
            dy_g = prim.max_y - prim.min_y;
            len2_g = dy_g * dy_g;
        }
        const float inv_len2_g = (len2_g > 1e-6f) ? (1.0f / len2_g) : 0.0f;
        const float c1r = static_cast<float>(prim.atlas_u0) * (1.0f / 65535.0f);
        const float c1g = static_cast<float>(prim.atlas_v0) * (1.0f / 65535.0f);
        const float c1b = static_cast<float>(prim.atlas_u1) * (1.0f / 65535.0f);
        const float c1a = static_cast<float>(prim.atlas_v1) * (1.0f / 65535.0f);

        // 预计算列级阴影 fx (每列仅算 1 次，免除 16 行重复计算 erf)
        float col_shadow_fx[16];
        if (prep.has_shadow) {
            for (uint32_t lx = lx0; lx < lx1; ++lx) {
                float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                col_shadow_fx[lx] = 0.5f * (erf_approx((p_x - prep.s_min_x) * prep.inv_sigma) -
                                            erf_approx((p_x - prep.s_max_x) * prep.inv_sigma));
            }
        }

        for (uint32_t ly = ly0; ly < ly1; ++ly) {
            const float p_y = tile_f_start_y + static_cast<float>(ly) + 0.5f;
            const uint32_t row_off = ly * 16u;
            const float row_sy = prep.has_shadow
                ? 0.5f * (erf_approx((p_y - prep.s_min_y) * prep.inv_sigma) -
                          erf_approx((p_y - prep.s_max_y) * prep.inv_sigma))
                : 0.0f;
            const bool y_gt = (p_y > prep.center_y);
            const float r_left = y_gt ? prep.r_bl : prep.r_tl;
            const float r_right = y_gt ? prep.r_br : prep.r_tr;
            const float abs_dy = std::abs(p_y - prep.center_y);

            // 圆角矩形中部直边带极速路径：留足 1.36x 超椭圆延伸过渡带，远离上下圆角区时直接退化为 1D 水平覆盖率
            const float corner_reach = std::min(std::min(prep.half_w, prep.half_h), prep.max_r * 1.36f);
            if (!prep.is_gradient && !prep.is_acrylic && !prep.has_shadow && !has_border &&
                p_y >= prim.min_y + corner_reach + 0.5f && p_y <= prim.max_y - corner_reach - 0.5f)
            {
                for (uint32_t lx = lx0; lx < lx1; ++lx) {
                    const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                    const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                    if (clip_a <= 0.0001f) continue;
                    const float geom_cov = std::clamp(0.5f - (std::abs(p_x - prep.center_x) - prep.half_w), 0.0f, 1.0f) * clip_a;
                    if (geom_cov <= 0.0001f) continue;
                    const float sa = ca * geom_cov;
                    const float one_minus_sa = 1.0f - sa;
                    const uint32_t idx = row_off + lx;
                    tile_r[idx] = cr * geom_cov + tile_r[idx] * one_minus_sa;
                    tile_g[idx] = cg * geom_cov + tile_g[idx] * one_minus_sa;
                    tile_b[idx] = cb * geom_cov + tile_b[idx] * one_minus_sa;
                    tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                }
                continue;
            }

            for (uint32_t lx = lx0; lx < lx1; ++lx) {
                const float p_x = tile_f_start_x + static_cast<float>(lx) + 0.5f;
                const float clip_a = need_clip ? eval_clip_alpha(p_x, p_y, prim.clip_index, clip_rects) : 1.0f;
                if (clip_a <= 0.0001f) continue;
                const uint32_t idx = row_off + lx;

                if (prep.has_shadow) {
                    float shadow_intensity = col_shadow_fx[lx] * row_sy;
                    if (shadow_intensity > 0.0005f) {
                        float src_a = ca * shadow_intensity * clip_a;
                        float one_minus_sa = 1.0f - src_a;
                        tile_r[idx] *= one_minus_sa;
                        tile_g[idx] *= one_minus_sa;
                        tile_b[idx] *= one_minus_sa;
                        tile_a[idx] = src_a + tile_a[idx] * one_minus_sa;
                    }
                }

                float r = (p_x > prep.center_x) ? r_right : r_left;
                float dist = eval_squircle_box_distance(
                    std::abs(p_x - prep.center_x), abs_dy, prep.half_w, prep.half_h, r);
                if (has_border) {
                    dist = std::abs(dist + half_border) - half_border;
                }
                float geom_cov = evaluate_coverage(dist) * clip_a;
                if (geom_cov > 0.0001f) {
                    if (prep.is_acrylic) {
                        const float g_off = eval_acrylic_grain(p_x, p_y, acrylic_scale) * acrylic_amp * ca;
                        const float br = std::clamp(cr + g_off, 0.0f, ca);
                        const float bg = std::clamp(cg + g_off, 0.0f, ca);
                        const float bb = std::clamp(cb + g_off, 0.0f, ca);
                        const float sa = ca * geom_cov;
                        const float one_minus_sa = 1.0f - sa;
                        tile_r[idx] = br * geom_cov + tile_r[idx] * one_minus_sa;
                        tile_g[idx] = bg * geom_cov + tile_g[idx] * one_minus_sa;
                        tile_b[idx] = bb * geom_cov + tile_b[idx] * one_minus_sa;
                        tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                    } else if (prep.is_gradient) {
                        const float t = (inv_len2_g > 0.0f)
                            ? std::clamp(((p_x - gx0) * dx_g + (p_y - gy0) * dy_g) * inv_len2_g, 0.0f, 1.0f)
                            : 0.0f;
                        float gr, gg, gb, ga;
                        if (prep.is_oklab_gradient) {
                            ga = std::clamp(ca + (c1a - ca) * t, 0.0f, 1.0f);
                            const math::OkLab mid_lab{
                                prep.lab0.L + (prep.lab1.L - prep.lab0.L) * t,
                                prep.lab0.a + (prep.lab1.a - prep.lab0.a) * t,
                                prep.lab0.b + (prep.lab1.b - prep.lab0.b) * t
                            };
                            const math::Color mid_srgb = math::oklab_to_srgb(mid_lab, ga);
                            gr = mid_srgb.r * ga;
                            gg = mid_srgb.g * ga;
                            gb = mid_srgb.b * ga;
                        } else {
                            gr = cr + (c1r - cr) * t;
                            gg = cg + (c1g - cg) * t;
                            gb = cb + (c1b - cb) * t;
                            ga = ca + (c1a - ca) * t;
                        }
                        const float sa = ga * geom_cov;
                        const float one_minus_sa = 1.0f - sa;
                        tile_r[idx] = gr * geom_cov + tile_r[idx] * one_minus_sa;
                        tile_g[idx] = gg * geom_cov + tile_g[idx] * one_minus_sa;
                        tile_b[idx] = gb * geom_cov + tile_b[idx] * one_minus_sa;
                        tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                    } else {
                        float sa = ca * geom_cov;
                        float one_minus_sa = 1.0f - sa;
                        tile_r[idx] = cr * geom_cov + tile_r[idx] * one_minus_sa;
                        tile_g[idx] = cg * geom_cov + tile_g[idx] * one_minus_sa;
                        tile_b[idx] = cb * geom_cov + tile_b[idx] * one_minus_sa;
                        tile_a[idx] = sa + tile_a[idx] * one_minus_sa;
                    }
                }
            }
        }
    }

    // 4. 紧凑单遍 32-bit 打包写回目标 RGBA8 帧缓冲 (相比 4 次逐字节写入减少 4x 访存指令)
    if (enable_dithering) {
        constexpr float kInv255 = 1.0f / 255.0f;
        for (uint32_t ly = 0; ly < tile_h; ++ly) {
            const uint32_t row_off = ly * 16u;
            const uint32_t py = start_y + ly;
            uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(py) * width + start_x) * 4);
            for (uint32_t lx = 0; lx < tile_w; ++lx) {
                const uint32_t idx = row_off + lx;
                const float dither = (tile_a[idx] > 0.0f) ? ((eval_ign(start_x + lx, py) - 0.5f) * kInv255) : 0.0f;
                uint32_t r = static_cast<uint32_t>(std::clamp(tile_r[idx] + dither, 0.0f, 1.0f) * 255.0f + 0.5f);
                uint32_t g = static_cast<uint32_t>(std::clamp(tile_g[idx] + dither, 0.0f, 1.0f) * 255.0f + 0.5f);
                uint32_t b = static_cast<uint32_t>(std::clamp(tile_b[idx] + dither, 0.0f, 1.0f) * 255.0f + 0.5f);
                uint32_t a = static_cast<uint32_t>(std::clamp(tile_a[idx], 0.0f, 1.0f) * 255.0f + 0.5f);
                row_dst[lx] = r | (g << 8) | (b << 16) | (a << 24);
            }
        }
    } else {
        for (uint32_t ly = 0; ly < tile_h; ++ly) {
            const uint32_t row_off = ly * 16u;
            uint32_t* row_dst = reinterpret_cast<uint32_t*>(dst_framebuffer + (static_cast<size_t>(start_y + ly) * width + start_x) * 4);
            for (uint32_t lx = 0; lx < tile_w; ++lx) {
                const uint32_t idx = row_off + lx;
                uint32_t r = static_cast<uint32_t>(std::clamp(tile_r[idx], 0.0f, 1.0f) * 255.0f + 0.5f);
                uint32_t g = static_cast<uint32_t>(std::clamp(tile_g[idx], 0.0f, 1.0f) * 255.0f + 0.5f);
                uint32_t b = static_cast<uint32_t>(std::clamp(tile_b[idx], 0.0f, 1.0f) * 255.0f + 0.5f);
                uint32_t a = static_cast<uint32_t>(std::clamp(tile_a[idx], 0.0f, 1.0f) * 255.0f + 0.5f);
                row_dst[lx] = r | (g << 8) | (b << 16) | (a << 24);
            }
        }
    }
}

void apply_backdrop_blur_prim(
    const core::UiPrimitive& prim,
    uint32_t width,
    uint32_t height,
    uint8_t* dst_framebuffer)
{
    const int bx0 = std::clamp(static_cast<int>(std::floor(prim.min_x)), 0, static_cast<int>(width));
    const int by0 = std::clamp(static_cast<int>(std::floor(prim.min_y)), 0, static_cast<int>(height));
    const int bx1 = std::clamp(static_cast<int>(std::ceil(prim.max_x)), 0, static_cast<int>(width));
    const int by1 = std::clamp(static_cast<int>(std::ceil(prim.max_y)), 0, static_cast<int>(height));
    if (bx1 <= bx0 || by1 <= by0) return;

    const int pad = std::clamp(static_cast<int>(std::ceil(prim.shadow_blur * 1.25f)), 24, 72);
    const int rx0 = std::max(0, bx0 - pad);
    const int ry0 = std::max(0, by0 - pad);
    const int rx1 = std::min(static_cast<int>(width), bx1 + pad);
    const int ry1 = std::min(static_cast<int>(height), by1 + pad);
    const uint32_t rw = static_cast<uint32_t>(rx1 - rx0);
    const uint32_t rh = static_cast<uint32_t>(ry1 - ry0);
    if (rw == 0 || rh == 0) return;

    std::vector<uint8_t> crop(static_cast<size_t>(rw) * rh * 4u);
    for (uint32_t y = 0; y < rh; ++y) {
        const uint8_t* src = dst_framebuffer + (static_cast<size_t>(ry0 + y) * width + rx0) * 4u;
        std::memcpy(crop.data() + static_cast<size_t>(y) * rw * 4u, src, static_cast<size_t>(rw) * 4u);
    }

    std::vector<uint8_t> blurred(crop.size());
    fx::BlurParams bp{};
    bp.quality = fx::BlurQuality::High;
    bp.radius = std::max(28.0f, prim.shadow_blur);
    bp.offset_scale = 1.6f;
    bp.custom_levels = 4u;
    fx::DualKawaseBlur::blur_cpu(crop.data(), rw, rh, blurred.data(), bp);

    const float cx = (prim.min_x + prim.max_x) * 0.5f;
    const float cy = (prim.min_y + prim.max_y) * 0.5f;
    const float hw = (prim.max_x - prim.min_x) * 0.5f;
    const float hh = (prim.max_y - prim.min_y) * 0.5f;
    const float cap_r = std::min(hw, hh);
    const float rtl = std::clamp(prim.radius_top_left, 0.0f, cap_r);
    const float rtr = std::clamp(prim.radius_top_right, 0.0f, cap_r);
    const float rbr = std::clamp(prim.radius_bottom_right, 0.0f, cap_r);
    const float rbl = std::clamp(prim.radius_bottom_left, 0.0f, cap_r);
    const float inv_tint_a = 1.0f - prim.color_a;
    constexpr float kInv255 = 1.0f / 255.0f;

    for (int py = by0; py < by1; ++py) {
        const float dy = (static_cast<float>(py) + 0.5f) - cy;
        const float ady = std::abs(dy);
        uint8_t* dst_row = dst_framebuffer + (static_cast<size_t>(py) * width + bx0) * 4u;
        const uint8_t* blur_row = blurred.data() + (static_cast<size_t>(py - ry0) * rw + (bx0 - rx0)) * 4u;
        for (int px = bx0; px < bx1; ++px) {
            const float dx = (static_cast<float>(px) + 0.5f) - cx;
            const float raw_r = (dx >= 0.0f) ? ((dy >= 0.0f) ? rbr : rtr) : ((dy >= 0.0f) ? rbl : rtl);
            const float cov = std::clamp(0.5f - eval_squircle_box_distance(std::abs(dx), ady, hw, hh, raw_r), 0.0f, 1.0f);
            if (cov > 0.001f) {
                const float br = static_cast<float>(blur_row[0]) * kInv255;
                const float bg = static_cast<float>(blur_row[1]) * kInv255;
                const float bb = static_cast<float>(blur_row[2]) * kInv255;
                const float ba = static_cast<float>(blur_row[3]) * kInv255;
                const float fr = prim.color_r + br * inv_tint_a;
                const float fg = prim.color_g + bg * inv_tint_a;
                const float fb = prim.color_b + bb * inv_tint_a;
                const float fa = prim.color_a + ba * inv_tint_a;
                const float inv_c = 1.0f - cov;
                const float dr = static_cast<float>(dst_row[0]) * kInv255;
                const float dg = static_cast<float>(dst_row[1]) * kInv255;
                const float db = static_cast<float>(dst_row[2]) * kInv255;
                const float da = static_cast<float>(dst_row[3]) * kInv255;
                dst_row[0] = static_cast<uint8_t>(std::clamp(fr * cov + dr * inv_c, 0.0f, 1.0f) * 255.0f + 0.5f);
                dst_row[1] = static_cast<uint8_t>(std::clamp(fg * cov + dg * inv_c, 0.0f, 1.0f) * 255.0f + 0.5f);
                dst_row[2] = static_cast<uint8_t>(std::clamp(fb * cov + db * inv_c, 0.0f, 1.0f) * 255.0f + 0.5f);
                dst_row[3] = static_cast<uint8_t>(std::clamp(fa * cov + da * inv_c, 0.0f, 1.0f) * 255.0f + 0.5f);
            }
            dst_row += 4;
            blur_row += 4;
        }
    }
}
} // namespace

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
    uint32_t atlas_height,
    bool mica_enabled,
    const std::array<uint32_t, 16>* mica_grid,
    const uint8_t* color_atlas_pixels,
    uint32_t color_atlas_width,
    uint32_t color_atlas_height,
    bool enable_dithering,
    bool enable_font_gamma,
    bool enable_acrylic_grain)
{
    render_tile_range(
        tile_x, tile_y, tile_id, width, height,
        primitives, tile_indices, tile_headers, clip_rects, dst_framebuffer,
        atlas_pixels, atlas_width, atlas_height, mica_enabled, mica_grid,
        color_atlas_pixels, color_atlas_width, color_atlas_height,
        enable_dithering, enable_font_gamma, enable_acrylic_grain,
        0u, UINT32_MAX, false);
}

void CpuTileRasterizer::render(
    const RenderParams& params,
    uint8_t* dst_framebuffer)
{
    if (params.width == 0 || params.height == 0 || !dst_framebuffer) return;

    const uint32_t dirty_count = static_cast<uint32_t>(params.dirty_tiles.size());
    if (dirty_count == 0) return;

    const uint32_t tiles_x = (params.width + 15) / 16;

    uint32_t first_backdrop_id = UINT32_MAX;
    for (uint32_t i = 0; i < static_cast<uint32_t>(params.primitives.size()); ++i) {
        if ((params.primitives[i].flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0) {
            first_backdrop_id = i;
            break;
        }
    }

    auto dispatch_tiles = [&](uint32_t min_p, uint32_t max_p, bool load_dst) {
        auto run_tile = [&](uint32_t i) {
            const uint32_t tile_id = params.dirty_tiles[i];
            const uint32_t tx = tile_id % tiles_x;
            const uint32_t ty = tile_id / tiles_x;
            render_tile_range(tx, ty, tile_id, params.width, params.height,
                              params.primitives, params.tile_indices,
                              params.tile_headers, params.clip_rects,
                              dst_framebuffer,
                              params.atlas_pixels, params.atlas_width, params.atlas_height,
                              params.mica_enabled, &params.mica_grid,
                              params.color_atlas_pixels, params.color_atlas_width, params.color_atlas_height,
                              params.enable_dithering, params.enable_font_gamma, params.enable_acrylic_grain,
                              min_p, max_p, load_dst);
        };
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
        for (uint32_t i = 0; i < dirty_count; ++i) {
            run_tile(i);
        }
#else
        const uint32_t cpu_cores = std::thread::hardware_concurrency();
        const uint32_t threads = (params.thread_count > 0)
            ? params.thread_count
            : calculate_worker_threads(dirty_count, cpu_cores);
        if (threads <= 1u || dirty_count <= 8u) {
            for (uint32_t i = 0; i < dirty_count; ++i) {
                run_tile(i);
            }
            return;
        }
        CpuTileThreadPool::instance().parallel_for_tiles(dirty_count, threads, run_tile);
#endif
    };

    if (first_backdrop_id == UINT32_MAX) {
        dispatch_tiles(0u, UINT32_MAX, false);
        return;
    }

    // Pass 1: 渲染背景场景图元 (prim_id < first_backdrop_id)
    dispatch_tiles(0u, first_backdrop_id, false);

    // Pass 2: 对所有背景毛玻璃弹出层区域执行 DualKawaseBlur 高质量重度模糊与圆角合成
    for (uint32_t i = first_backdrop_id; i < static_cast<uint32_t>(params.primitives.size()); ++i) {
        if ((params.primitives[i].flags & static_cast<uint16_t>(core::PrimitiveFlags::IsBackdropFilter)) != 0) {
            apply_backdrop_blur_prim(params.primitives[i], params.width, params.height, dst_framebuffer);
        }
    }

    // Pass 3: 在重度模糊毛玻璃底板之上合成弹出层前景（边框、高光、文字、列表项）
    dispatch_tiles(first_backdrop_id + 1u, UINT32_MAX, true);
}

} // namespace kernetile::cpu

#pragma once

#include <ktui/animation/frame_clock.hpp>
#include <ktui/pal/font_resolver.hpp>
#include <ktui/pal/platform_types.hpp>
#include <kernetile/renderer.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string_view>

#if !defined(KTUI_NOINLINE)
#if defined(_MSC_VER)
#define KTUI_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define KTUI_NOINLINE __attribute__((noinline))
#else
#define KTUI_NOINLINE
#endif
#endif

namespace ktui {

// UI 绘制上下文 (PaintContext)
// 负责在场景树遍历过程中累加坐标变换、级联裁剪栈，并向底层 KerneTile 引擎发射 80B 紧致图元
class PaintContext {
public:
    explicit PaintContext(kernetile::KerneTileRenderer* renderer = nullptr,
                          float render_scale = 1.0f,
                          bool pixel_snapping = false) noexcept
        : m_renderer(renderer), m_render_scale(render_scale), m_pixel_snapping(pixel_snapping) {}

    void set_pixel_snapping(bool enabled) noexcept { m_pixel_snapping = enabled; }
    [[nodiscard]] bool pixel_snapping() const noexcept { return m_pixel_snapping; }

    // 平移作用域守卫
    class OffsetScope {
    public:
        OffsetScope(PaintContext& ctx, Vec2 delta) noexcept
            : m_ctx(ctx), m_prev_offset(ctx.current_offset()) {
            m_ctx.set_current_offset(m_prev_offset + delta);
        }

        ~OffsetScope() noexcept {
            m_ctx.set_current_offset(m_prev_offset);
        }

        OffsetScope(const OffsetScope&) = delete;
        OffsetScope& operator=(const OffsetScope&) = delete;

    private:
        PaintContext& m_ctx;
        Vec2 m_prev_offset;
    };

    // 绘制实心或带边框矩形
    KTUI_NOINLINE void draw_rect(Rect rect, Color color, float corner_radius = 0.0f, float border_width = 0.0f) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            Rect px = to_pixels(abs_rect);
            const float cr_px = corner_radius * m_render_scale;
            const float bw_px = snap_stroke_px(border_width);
            if (m_pixel_snapping) {
                px = snap_rect_px(px, bw_px, cr_px);
            }
            m_renderer->draw_rect(px, color, cr_px, bw_px);
        }
        m_primitive_count++;
    }

    // 绘制填充 + 描边组合盒 (减少重复双调用代码体积)
    KTUI_NOINLINE void draw_box(Rect rect, Color fill, Color border, float corner_radius = 4.0f, float border_width = 1.0f) {
        if (fill.a > 0.001f) draw_rect(rect, fill, corner_radius, 0.0f);
        if (border.a > 0.001f && border_width > 0.0f) draw_rect(rect, border, corner_radius, border_width);
    }

    // 绘制四角独立圆角矩形 (支持可选柔和投影与边框，如 5px 5px 0px 0px 顶部圆角输入框或单角圆角描边)
    KTUI_NOINLINE void draw_rounded_rect(Rect rect, Color color,
                           float radius_tl, float radius_tr,
                           float radius_br, float radius_bl,
                           float shadow_blur = 0.0f,
                           Vec2 shadow_offset = Vec2{0.0f, 0.0f},
                           float border_width = 0.0f) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            Rect px = to_pixels(abs_rect);
            const float rtl_px = radius_tl * m_render_scale;
            const float rtr_px = radius_tr * m_render_scale;
            const float rbr_px = radius_br * m_render_scale;
            const float rbl_px = radius_bl * m_render_scale;
            const float bw_px = (m_pixel_snapping && shadow_blur <= 0.0f)
                ? snap_stroke_px(border_width)
                : border_width * m_render_scale;
            if (m_pixel_snapping && shadow_blur <= 0.0f) {
                const float max_r_px = std::max({rtl_px, rtr_px, rbr_px, rbl_px});
                px = snap_rect_px(px, bw_px, max_r_px);
            }
            kernetile::core::UiPrimitive prim{};
            prim.min_x = px.min_x;
            prim.min_y = px.min_y;
            prim.max_x = px.max_x;
            prim.max_y = px.max_y;
            prim.radius_top_left = rtl_px;
            prim.radius_top_right = rtr_px;
            prim.radius_bottom_right = rbr_px;
            prim.radius_bottom_left = rbl_px;
            prim.color_r = color.r * color.a;
            prim.color_g = color.g * color.a;
            prim.color_b = color.b * color.a;
            prim.color_a = color.a;
            prim.border_width = bw_px;
            prim.shadow_blur = shadow_blur * m_render_scale;
            prim.shadow_offset_x = shadow_offset.x * m_render_scale;
            prim.shadow_offset_y = shadow_offset.y * m_render_scale;
            m_renderer->push_primitive(prim);
        }
        m_primitive_count++;
    }

    // 绘制线性渐变矩形或渐变边框 (支持 R1 IGN 抖动降噪与 R2 物理像素对齐)
    KTUI_NOINLINE void draw_linear_gradient(Rect rect, Color start_color, Color end_color,
                                            PointF start_pt, PointF end_pt,
                                            float corner_radius = 0.0f,
                                            float border_width = 0.0f) {
        Rect abs_rect = to_absolute_rect(rect);
        PointF abs_start = start_pt + m_current_offset;
        PointF abs_end = end_pt + m_current_offset;
        if (m_renderer) {
            Rect px = to_pixels(abs_rect);
            const float cr_px = corner_radius * m_render_scale;
            const float bw_px = snap_stroke_px(border_width);
            if (m_pixel_snapping) {
                px = snap_rect_px(px, bw_px, cr_px);
            }
            m_renderer->draw_linear_gradient(
                px, start_color, end_color,
                abs_start * m_render_scale, abs_end * m_render_scale,
                cr_px, bw_px);
        }
        m_primitive_count++;
    }

    // 绘制指针跟随高光 Reveal (单色直通重载，直接对齐 KerneTileRenderer::draw_pointer_reveal)
    KTUI_NOINLINE void draw_pointer_reveal(Rect widget_bounds, PointF cursor_pos, float radius,
                                           Color color, float corner_radius = 0.0f,
                                           float border_width = 0.0f, float edge_intensity = 1.0f) {
        if (radius <= 0.001f || color.a <= 0.0005f || widget_bounds.is_empty()) return;
        Rect abs_bounds = to_absolute_rect(widget_bounds);
        PointF abs_cursor = cursor_pos + m_current_offset;
        if (m_renderer) {
            Rect px_bounds = to_pixels(abs_bounds);
            const float cr_px = corner_radius * m_render_scale;
            const float bw_px = snap_stroke_px(border_width);
            if (m_pixel_snapping) {
                px_bounds = snap_rect_px(px_bounds, bw_px, cr_px);
            }
            m_renderer->draw_pointer_reveal(
                px_bounds, abs_cursor * m_render_scale, radius * m_render_scale,
                color, cr_px, bw_px, edge_intensity);
        }
        m_primitive_count++;
    }

    // 绘制指针跟随高光 Reveal (表面高光 + 边框高光双层便捷重载，供交互控件使用)
    KTUI_NOINLINE void draw_pointer_reveal(Rect widget_bounds, PointF cursor_pos, float radius,
                                           Color surface_color, Color edge_color,
                                           float corner_radius = 0.0f, float border_width = 1.0f) {
        if (radius <= 0.001f || widget_bounds.is_empty()) return;
        if (surface_color.a > 0.0005f) {
            draw_pointer_reveal(widget_bounds, cursor_pos, radius, surface_color, corner_radius, 0.0f, 1.0f);
        }
        if (edge_color.a > 0.0005f && border_width > 0.0f) {
            draw_pointer_reveal(widget_bounds, cursor_pos, radius, edge_color, corner_radius, border_width, 1.0f);
        }
    }

    // 绘制带软阴影与圆角的卡片盒
    KTUI_NOINLINE void draw_shadowed_box(Rect rect, Color color, float corner_radius,
                           float shadow_blur, float shadow_spread, Vec2 shadow_offset) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            m_renderer->draw_shadowed_box(to_pixels(abs_rect), color, corner_radius * m_render_scale,
                                          shadow_blur * m_render_scale, shadow_spread * m_render_scale,
                                          shadow_offset * m_render_scale);
        }
        m_primitive_count += 2; // 阴影图元 + 本体图元
    }

    // 绘制 WinUI 3 / Fluent 2 双层解析高斯软阴影 (Key Shadow + Ambient Shadow)
    // 通过 KerneTile 引擎原生 erf() 解析高斯核渲染，内缩 1.5px 避免面板抗锯齿边缘黑边与圆角露角
    KTUI_NOINLINE void draw_elevation_shadow(Rect rect, float corner_radius, bool is_dark,
                               float elevation = 32.0f, float opacity_scale = 1.0f,
                               Vec2 direction_offset = Vec2{0.0f, 1.0f}) {
        if (opacity_scale <= 0.001f || elevation <= 0.0f) return;
        float w = rect.width();
        float h = rect.height();
        if (w <= 2.0f || h <= 2.0f) return;

        float inset = std::min({1.5f, w * 0.25f, h * 0.25f});
        Rect sr = Rect::from_xywh(rect.min_x + inset, rect.min_y + inset,
                                  w - inset * 2.0f, h - inset * 2.0f);
        float sr_radius = std::max(0.0f, corner_radius - inset);

        // KerneTile shadow_blur = Gaussian sigma (约为 CSS blur-radius 的 0.5x)
        float key_sigma = std::clamp(elevation * 0.28f, 4.5f, 20.0f);
        float key_dist  = std::clamp(elevation * 0.18f, 2.5f, 12.0f);
        float key_alpha = (is_dark ? 0.44f : 0.17f) * opacity_scale;

        float amb_sigma = std::clamp(elevation * 0.09f, 2.5f, 6.0f);
        float amb_dist  = std::clamp(elevation * 0.035f, 1.0f, 2.5f);
        float amb_alpha = (is_dark ? 0.24f : 0.09f) * opacity_scale;

        draw_shadowed_box(sr, Color{0.0f, 0.0f, 0.0f, key_alpha},
                          sr_radius, key_sigma, 0.0f,
                          Vec2{direction_offset.x * key_dist, direction_offset.y * key_dist});
        draw_shadowed_box(sr, Color{0.0f, 0.0f, 0.0f, amb_alpha},
                          sr_radius, amb_sigma, 0.0f,
                          Vec2{direction_offset.x * amb_dist, direction_offset.y * amb_dist});
    }

    // 绘制文本 (支持 WinUI 3 字重、斜体与自定义字体族，如 "monospace" / "Cascadia Mono" / "Consolas" 及等宽数字 tabular_nums)
    KTUI_NOINLINE void draw_text(std::string_view text, PointF pos, float font_size, Color color,
                   FontWeight weight = FontWeight::Regular, bool italic = false,
                   std::string_view font_family = "", bool tabular_nums = false) {
        PointF abs_pos = pos + m_current_offset;
        if (m_renderer) {
            m_renderer->draw_text_scaled(text, abs_pos.x, abs_pos.y, font_size, color,
                                         m_render_scale, static_cast<uint16_t>(weight), italic, font_family, tabular_nums);
        }
        m_primitive_count++;
    }

    [[nodiscard]] float render_scale() const noexcept { return m_render_scale; }

    [[nodiscard]] bool has_cached_mask(uint64_t cache_key) const noexcept {
        return m_renderer ? m_renderer->has_cached_mask(cache_key) : false;
    }

    // 绘制 R8 Alpha 蒙版 (供 NanoSVG 矢量图标与路径渲染使用)
    KTUI_NOINLINE void draw_alpha_mask(uint64_t cache_key, Rect dst_rect, uint32_t mask_w, uint32_t mask_h,
                         const uint8_t* alpha_pixels, Color tint) {
        Rect abs_rect = to_absolute_rect(dst_rect);
        if (m_renderer) {
            m_renderer->draw_alpha_mask(cache_key, to_pixels(abs_rect), mask_w, mask_h, alpha_pixels, tint);
        }
        m_primitive_count++;
    }

    [[nodiscard]] bool has_cached_image(uint64_t cache_key) const noexcept {
        return m_renderer ? m_renderer->has_cached_image(cache_key) : false;
    }

    void invalidate_cached_image(uint64_t cache_key) {
        if (m_renderer) {
            m_renderer->invalidate_cached_image(cache_key);
        }
    }

    // 绘制彩色 RGBA8 位图/纹理 (支持圆角裁剪、着色/透明度调制、子区域 UV 裁剪与动图原地帧更新)
    KTUI_NOINLINE void draw_image(uint64_t cache_key, Rect dst_rect, uint32_t img_w, uint32_t img_h,
                    const uint8_t* rgba8_pixels, float corner_radius = 0.0f,
                    Color tint = Color{1.0f, 1.0f, 1.0f, 1.0f},
                    Rect uv_rect = Rect{0.0f, 0.0f, 1.0f, 1.0f},
                    uint32_t frame_revision = 0) {
        Rect abs_rect = to_absolute_rect(dst_rect);
        if (m_renderer) {
            m_renderer->draw_image(cache_key, to_pixels(abs_rect), img_w, img_h, rgba8_pixels,
                                   corner_radius * m_render_scale, tint, uv_rect, frame_revision);
        }
        m_primitive_count++;
    }

    // 绘制线段 (支持水平/垂直轴对齐极速路径与斜向高品质圆角端点胶囊步进插值)
    KTUI_NOINLINE void draw_line(PointF p1, PointF p2, Color color, float stroke_width = 1.0f) {
        if (color.a <= 0.001f || stroke_width <= 0.0f) return;
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.001f) {
            draw_rect(Rect::from_xywh(p1.x - stroke_width * 0.5f, p1.y - stroke_width * 0.5f, stroke_width, stroke_width), color, stroke_width * 0.5f, 0.0f);
            return;
        }

        // 水平线快速路径
        if (std::abs(dy) < 0.001f) {
            if (m_pixel_snapping) {
                const float w_phys = std::max(1.0f, std::round(stroke_width * m_render_scale));
                const int w_int = static_cast<int>(w_phys);
                const float cy_raw = (p1.y + m_current_offset.y) * m_render_scale;
                const float cy = (w_int & 1) ? (std::floor(cy_raw) + 0.5f) : std::round(cy_raw);
                const float x0 = std::round((std::min(p1.x, p2.x) + m_current_offset.x) * m_render_scale);
                float x1 = std::round((std::max(p1.x, p2.x) + m_current_offset.x) * m_render_scale);
                if (x1 - x0 < 1.0f) x1 = x0 + 1.0f;
                if (m_renderer) {
                    m_renderer->draw_rect(Rect{x0, cy - w_phys * 0.5f, x1, cy + w_phys * 0.5f}, color, 0.0f, 0.0f);
                }
                m_primitive_count++;
                return;
            }
            float min_x = std::min(p1.x, p2.x);
            float w = std::abs(dx);
            draw_rect(Rect::from_xywh(min_x, p1.y - stroke_width * 0.5f, w, stroke_width), color, 0.0f, 0.0f);
            return;
        }

        // 垂直线快速路径
        if (std::abs(dx) < 0.001f) {
            if (m_pixel_snapping) {
                const float w_phys = std::max(1.0f, std::round(stroke_width * m_render_scale));
                const int w_int = static_cast<int>(w_phys);
                const float cx_raw = (p1.x + m_current_offset.x) * m_render_scale;
                const float cx = (w_int & 1) ? (std::floor(cx_raw) + 0.5f) : std::round(cx_raw);
                const float y0 = std::round((std::min(p1.y, p2.y) + m_current_offset.y) * m_render_scale);
                float y1 = std::round((std::max(p1.y, p2.y) + m_current_offset.y) * m_render_scale);
                if (y1 - y0 < 1.0f) y1 = y0 + 1.0f;
                if (m_renderer) {
                    m_renderer->draw_rect(Rect{cx - w_phys * 0.5f, y0, cx + w_phys * 0.5f, y1}, color, 0.0f, 0.0f);
                }
                m_primitive_count++;
                return;
            }
            float min_y = std::min(p1.y, p2.y);
            float h = std::abs(dy);
            draw_rect(Rect::from_xywh(p1.x - stroke_width * 0.5f, min_y, stroke_width, h), color, 0.0f, 0.0f);
            return;
        }

        // 斜向线段高质量圆角胶囊采样 (完全规避锯齿)
        float step = std::max(0.6f, stroke_width * 0.45f);
        int steps = std::max(1, static_cast<int>(std::ceil(len / step)));
        float rad = stroke_width * 0.5f;
        for (int i = 0; i <= steps; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(steps);
            float cx = p1.x + dx * t;
            float cy = p1.y + dy * t;
            draw_rect(Rect::from_xywh(cx - rad, cy - rad, stroke_width, stroke_width), color, rad, 0.0f);
        }
    }

    // 推入局部裁剪矩形
    KTUI_NOINLINE void push_clip(Rect rect, float corner_radius = 0.0f) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            Rect px = to_pixels(abs_rect);
            if (m_pixel_snapping && !abs_rect.is_empty()) {
                px.min_x = std::round(px.min_x);
                px.min_y = std::round(px.min_y);
                px.max_x = std::round(px.max_x);
                px.max_y = std::round(px.max_y);
                if (px.max_x < px.min_x + 1.0f) px.max_x = px.min_x + 1.0f;
                if (px.max_y < px.min_y + 1.0f) px.max_y = px.min_y + 1.0f;
            }
            m_renderer->push_clip(px, corner_radius * m_render_scale);
        }
        m_clip_stack.push_back(abs_rect);
    }

    // 弹出裁剪矩形
    KTUI_NOINLINE void pop_clip() {
        if (!m_clip_stack.empty()) {
            m_clip_stack.pop_back();
            if (m_renderer) {
                m_renderer->pop_clip();
            }
        }
    }

    [[nodiscard]] Vec2 current_offset() const noexcept { return m_current_offset; }
    void set_current_offset(Vec2 off) noexcept { m_current_offset = off; }

    [[nodiscard]] size_t primitive_count() const noexcept { return m_primitive_count; }
    void reset_primitive_count() noexcept { m_primitive_count = 0; }

    [[nodiscard]] double frame_time_sec() const noexcept { return FrameClock::global().frame_time_sec(); }
    [[nodiscard]] float frame_dt() const noexcept { return FrameClock::global().dt(); }

    [[nodiscard]] kernetile::KerneTileRenderer* renderer() const noexcept { return m_renderer; }

private:
    [[nodiscard]] Rect to_absolute_rect(const Rect& r) const noexcept {
        return Rect::from_xywh(r.min_x + m_current_offset.x, r.min_y + m_current_offset.y, r.width(), r.height());
    }

    [[nodiscard]] Rect to_pixels(const Rect& rect) const noexcept {
        return {rect.min_x * m_render_scale, rect.min_y * m_render_scale,
                rect.max_x * m_render_scale, rect.max_y * m_render_scale};
    }

    [[nodiscard]] float snap_stroke_px(float logical_w) const noexcept {
        const float scaled = logical_w * m_render_scale;
        if (!m_pixel_snapping || logical_w <= 0.0f) return scaled;
        return std::max(1.0f, std::round(scaled));
    }

    [[nodiscard]] static Rect snap_rect_px(Rect px, float bw_px, float max_r_px = 0.0f) noexcept {
        const float w_orig = px.max_x - px.min_x;
        const float h_orig = px.max_y - px.min_y;
        if (w_orig <= 0.0f || h_orig <= 0.0f) return px;
        if (bw_px <= 0.0f && max_r_px >= w_orig * 0.499f && std::abs(w_orig - h_orig) < 0.01f) {
            return px; // Preserve circular dots
        }
        auto snap_axis = [&](float& a, float& b) noexcept {
            const float ext = b - a;
            if (bw_px <= 0.0f && ext < 2.0f) {
                const float w = std::max(1.0f, std::round(ext));
                const float mid = (a + b) * 0.5f;
                const int w_int = static_cast<int>(w);
                const float center = (w_int & 1) ? (std::floor(mid) + 0.5f) : std::round(mid);
                a = center - w * 0.5f;
                b = center + w * 0.5f;
            } else {
                a = std::round(a);
                b = std::round(b);
                const float min_ext = (bw_px > 0.0f) ? std::max(1.0f, bw_px) : 1.0f;
                if (b - a < min_ext) {
                    b = a + min_ext;
                }
            }
        };
        snap_axis(px.min_x, px.max_x);
        snap_axis(px.min_y, px.max_y);
        return px;
    }

    kernetile::KerneTileRenderer* m_renderer{nullptr};
    float m_render_scale{1.0f};
    bool m_pixel_snapping{false};
    Vec2 m_current_offset{0.0f, 0.0f};
    std::vector<Rect> m_clip_stack;
    size_t m_primitive_count{0};
};

} // namespace ktui

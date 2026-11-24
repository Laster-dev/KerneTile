#pragma once

#include <ktui/pal/platform_types.hpp>
#include <kernetile/renderer.hpp>
#include <vector>
#include <string_view>

namespace ktui {

// UI 绘制上下文 (PaintContext)
// 负责在场景树遍历过程中累加坐标变换、级联裁剪栈，并向底层 KerneTile 引擎发射 80B 紧致图元
class PaintContext {
public:
    explicit PaintContext(kernetile::KerneTileRenderer* renderer = nullptr, float render_scale = 1.0f) noexcept
        : m_renderer(renderer), m_render_scale(render_scale) {}

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
    void draw_rect(Rect rect, Color color, float corner_radius = 0.0f, float border_width = 0.0f) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            m_renderer->draw_rect(to_pixels(abs_rect), color, corner_radius * m_render_scale,
                                  border_width * m_render_scale);
        }
        m_primitive_count++;
    }

    // 绘制四角独立圆角矩形 (支持可选柔和投影，如 5px 5px 0px 0px 顶部圆角输入框)
    void draw_rounded_rect(Rect rect, Color color,
                           float radius_tl, float radius_tr,
                           float radius_br, float radius_bl,
                           float shadow_blur = 0.0f,
                           Vec2 shadow_offset = Vec2{0.0f, 0.0f}) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            Rect px = to_pixels(abs_rect);
            kernetile::core::UiPrimitive prim{};
            prim.min_x = px.min_x;
            prim.min_y = px.min_y;
            prim.max_x = px.max_x;
            prim.max_y = px.max_y;
            prim.radius_top_left = radius_tl * m_render_scale;
            prim.radius_top_right = radius_tr * m_render_scale;
            prim.radius_bottom_right = radius_br * m_render_scale;
            prim.radius_bottom_left = radius_bl * m_render_scale;
            prim.color_r = color.r * color.a;
            prim.color_g = color.g * color.a;
            prim.color_b = color.b * color.a;
            prim.color_a = color.a;
            prim.shadow_blur = shadow_blur * m_render_scale;
            prim.shadow_offset_x = shadow_offset.x * m_render_scale;
            prim.shadow_offset_y = shadow_offset.y * m_render_scale;
            m_renderer->push_primitive(prim);
        }
        m_primitive_count++;
    }

    // 绘制带软阴影与圆角的卡片盒
    void draw_shadowed_box(Rect rect, Color color, float corner_radius,
                           float shadow_blur, float shadow_spread, Vec2 shadow_offset) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            m_renderer->draw_shadowed_box(to_pixels(abs_rect), color, corner_radius * m_render_scale,
                                          shadow_blur * m_render_scale, shadow_spread * m_render_scale,
                                          shadow_offset * m_render_scale);
        }
        m_primitive_count += 2; // 阴影图元 + 本体图元
    }

    // 绘制文本
    void draw_text(std::string_view text, PointF pos, float font_size, Color color) {
        PointF abs_pos = pos + m_current_offset;
        if (m_renderer) {
            m_renderer->draw_text_scaled(text, abs_pos.x, abs_pos.y, font_size, color, m_render_scale);
        }
        m_primitive_count++;
    }

    [[nodiscard]] float render_scale() const noexcept { return m_render_scale; }

    [[nodiscard]] bool has_cached_mask(uint64_t cache_key) const noexcept {
        return m_renderer ? m_renderer->has_cached_mask(cache_key) : false;
    }

    // 绘制 R8 Alpha 蒙版 (供 NanoSVG 矢量图标与路径渲染使用)
    void draw_alpha_mask(uint64_t cache_key, Rect dst_rect, uint32_t mask_w, uint32_t mask_h,
                         const uint8_t* alpha_pixels, Color tint) {
        Rect abs_rect = to_absolute_rect(dst_rect);
        if (m_renderer) {
            m_renderer->draw_alpha_mask(cache_key, to_pixels(abs_rect), mask_w, mask_h, alpha_pixels, tint);
        }
        m_primitive_count++;
    }

    // 绘制线段 (支持水平/垂直轴对齐极速路径与斜向高品质圆角端点胶囊步进插值)
    void draw_line(PointF p1, PointF p2, Color color, float stroke_width = 1.0f) {
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
            float min_x = std::min(p1.x, p2.x);
            float w = std::abs(dx);
            draw_rect(Rect::from_xywh(min_x, p1.y - stroke_width * 0.5f, w, stroke_width), color, 0.0f, 0.0f);
            return;
        }

        // 垂直线快速路径
        if (std::abs(dx) < 0.001f) {
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
    void push_clip(Rect rect, float corner_radius = 0.0f) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            m_renderer->push_clip(to_pixels(abs_rect), corner_radius * m_render_scale);
        }
        m_clip_stack.push_back(abs_rect);
    }

    // 弹出裁剪矩形
    void pop_clip() {
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

    [[nodiscard]] kernetile::KerneTileRenderer* renderer() const noexcept { return m_renderer; }

private:
    [[nodiscard]] Rect to_absolute_rect(const Rect& r) const noexcept {
        return Rect::from_xywh(r.min_x + m_current_offset.x, r.min_y + m_current_offset.y, r.width(), r.height());
    }

    [[nodiscard]] Rect to_pixels(const Rect& rect) const noexcept {
        return {rect.min_x * m_render_scale, rect.min_y * m_render_scale,
                rect.max_x * m_render_scale, rect.max_y * m_render_scale};
    }

    kernetile::KerneTileRenderer* m_renderer{nullptr};
    float m_render_scale{1.0f};
    Vec2 m_current_offset{0.0f, 0.0f};
    std::vector<Rect> m_clip_stack;
    size_t m_primitive_count{0};
};

} // namespace ktui

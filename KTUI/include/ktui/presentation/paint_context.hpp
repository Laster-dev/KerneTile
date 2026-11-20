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
    explicit PaintContext(kernetile::KerneTileRenderer* renderer = nullptr) noexcept
        : m_renderer(renderer) {}

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
            m_renderer->draw_rect(abs_rect, color, corner_radius, border_width);
        }
        m_primitive_count++;
    }

    // 绘制带软阴影与圆角的卡片盒
    void draw_shadowed_box(Rect rect, Color color, float corner_radius,
                           float shadow_blur, float shadow_spread, Vec2 shadow_offset) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            m_renderer->draw_shadowed_box(abs_rect, color, corner_radius, shadow_blur, shadow_spread, shadow_offset);
        }
        m_primitive_count += 2; // 阴影图元 + 本体图元
    }

    // 绘制文本
    void draw_text(std::string_view text, PointF pos, float font_size, Color color) {
        PointF abs_pos = pos + m_current_offset;
        if (m_renderer) {
            m_renderer->draw_text(text, abs_pos.x, abs_pos.y, font_size, color);
        }
        m_primitive_count++;
    }

    // 推入局部裁剪矩形
    void push_clip(Rect rect, float corner_radius = 0.0f) {
        Rect abs_rect = to_absolute_rect(rect);
        if (m_renderer) {
            m_renderer->push_clip(abs_rect, corner_radius);
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

    kernetile::KerneTileRenderer* m_renderer{nullptr};
    Vec2 m_current_offset{0.0f, 0.0f};
    std::vector<Rect> m_clip_stack;
    size_t m_primitive_count{0};
};

} // namespace ktui

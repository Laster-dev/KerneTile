#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <algorithm>
#include <memory>

namespace ktui {

// 桌面级紧凑滚动视图渲染盒 (RenderScrollView)
class RenderScrollView : public RenderBox {
public:
    RenderScrollView() noexcept {
        setup_behavior();
    }

    ~RenderScrollView() override = default;

    void set_properties(float scrollbar_w, bool show_bar) {
        m_scrollbar_width = scrollbar_w;
        m_show_scrollbar = show_bar;
        mark_needs_layout();
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child_box == child) return;
        if (m_child_box) {
            remove_child(m_child_box.get());
        }
        m_child_box = child;
        if (m_child_box) {
            append_child(m_child_box.get());
        }
        mark_needs_layout();
    }

    void scroll_by(float delta_y) {
        set_scroll_offset(m_scroll_offset_y + delta_y);
    }

    void set_scroll_offset(float offset_y) {
        float max_scroll = std::max(0.0f, m_content_height - m_size.y);
        float clamped = std::clamp(offset_y, 0.0f, max_scroll);
        if (m_scroll_offset_y != clamped) {
            m_scroll_offset_y = clamped;
            if (m_child_box) {
                m_child_box->set_offset(Vec2{0.0f, -m_scroll_offset_y});
            }
            mark_needs_paint();
        }
    }

    [[nodiscard]] float scroll_offset() const noexcept { return m_scroll_offset_y; }
    [[nodiscard]] float content_height() const noexcept { return m_content_height; }

    void perform_layout() override {
        m_size = m_constraints.biggest();

        if (m_child_box) {
            // 子节点在纵向上不受限 (无限大)，宽度受限于视口宽度
            BoxConstraints child_constraints(0.0f, m_size.x, 0.0f, 100000.0f);
            m_child_box->layout(child_constraints, true);
            m_content_height = m_child_box->size().y;
            m_child_box->set_offset(Vec2{0.0f, -m_scroll_offset_y});
        } else {
            m_content_height = 0.0f;
        }
    }

    void paint(PaintContext& context) override {
        // 1. 推入视口硬件裁剪矩形
        context.push_clip(bounds(), 0.0f);

        // 2. 绘制子内容
        if (m_child_box) {
            PaintContext::OffsetScope scope(context, m_child_box->offset());
            m_child_box->paint(context);
        }

        // 3. 弹出裁剪
        context.pop_clip();

        // 4. 绘制精致紧凑滚动条 (6px 紧凑宽，微圆角 3px，半透明)
        if (m_show_scrollbar && m_content_height > m_size.y && m_size.y > 0.0f) {
            float ratio = m_size.y / m_content_height;
            float thumb_h = std::max(20.0f, m_size.y * ratio);
            float scroll_progress = m_scroll_offset_y / (m_content_height - m_size.y);
            float thumb_y = scroll_progress * (m_size.y - thumb_h);
            float thumb_x = m_size.x - m_scrollbar_width - 1.0f;

            Rect thumb_rect = Rect::from_xywh(thumb_x, thumb_y, m_scrollbar_width, thumb_h);
            context.draw_rect(thumb_rect, Color{1.0f, 1.0f, 1.0f, 0.25f}, 3.0f, 0.0f);
        }
    }

private:
    void setup_behavior() {
        // 滚轮事件
        on_pointer_move = [this](PointerEvent&) {
            // 可扩展滚轮或者拖拽滚动条
        };
    }

    float m_scrollbar_width{6.0f};  // 桌面紧凑 6px 细滚动条
    bool m_show_scrollbar{true};
    float m_scroll_offset_y{0.0f};
    float m_content_height{0.0f};
    std::shared_ptr<RenderBox> m_child_box{nullptr};
};

class ScrollViewWidget;

// C++20 指定初始化器滚动视图 (ScrollView)
// 桌面专业标准：视口裁剪，6px 紧凑半透明微细滚动条
struct ScrollView {
    WidgetPtr content{nullptr};
    float scrollbar_width{6.0f}; // 桌面 6px 紧凑细滚动条
    bool show_scrollbar{true};

    operator WidgetPtr() const;
};

// ScrollViewWidget 包装实现
class ScrollViewWidget : public Widget {
public:
    explicit ScrollViewWidget(const ScrollView& desc) : m_desc(desc) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderScrollView>();
        m_child_render_box = m_desc.content ? m_desc.content->create_render_node() : nullptr;
        if (m_desc.content && m_child_render_box) {
            m_desc.content->set_render_node(m_child_render_box);
        }
        node->set_child_box(m_child_render_box);
        node->set_properties(m_desc.scrollbar_width, m_desc.show_scrollbar);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto scroll = std::dynamic_pointer_cast<RenderScrollView>(node)) {
            if (m_desc.content) {
                if (!m_child_render_box) {
                    m_child_render_box = m_desc.content->create_render_node();
                    m_desc.content->set_render_node(m_child_render_box);
                    scroll->set_child_box(m_child_render_box);
                } else {
                    m_desc.content->update_render_node(m_child_render_box);
                }
            } else if (m_child_render_box) {
                m_child_render_box = nullptr;
                scroll->set_child_box(nullptr);
            }

            scroll->set_properties(m_desc.scrollbar_width, m_desc.show_scrollbar);
        }
    }

    [[nodiscard]] const ScrollView& desc() const noexcept { return m_desc; }

private:
    ScrollView m_desc;
    std::shared_ptr<RenderBox> m_child_render_box{nullptr};
};

inline ScrollView::operator WidgetPtr() const {
    return std::make_shared<ScrollViewWidget>(*this);
}

} // namespace ktui

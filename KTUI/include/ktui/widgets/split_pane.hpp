#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <memory>
#include <functional>
#include <algorithm>

namespace ktui {

enum class SplitDirection {
    Horizontal, // 左右横向分栏 (可左右拖拽调节宽度)
    Vertical    // 上下纵向分栏 (可上下拖拽调节高度)
};

// 桌面高信息密度可拖拽调节分栏盒 (RenderSplitPane)
class RenderSplitPane : public RenderBox {
public:
    RenderSplitPane(std::shared_ptr<RenderBox> first, std::shared_ptr<RenderBox> second,
                    SplitDirection dir, float split_pos, float min_1, float min_2, float thick,
                    std::function<void(float)> on_ch)
        : m_first_box(std::move(first)), m_second_box(std::move(second)),
          m_direction(dir), m_split_position(split_pos),
          m_min_first(min_1), m_min_second(min_2), m_splitter_thickness(thick),
          m_on_change(std::move(on_ch)) {
        if (m_first_box) append_child(m_first_box.get());
        if (m_second_box) append_child(m_second_box.get());
        setup_behavior();
    }

    void set_split_position(float pos) {
        if (m_split_position != pos) {
            m_split_position = pos;
            mark_needs_layout();
        }
    }

    [[nodiscard]] float split_position() const noexcept { return m_split_position; }

    void perform_layout() override {
        Vec2 sz = m_constraints.constrain(Vec2{m_constraints.max_width, m_constraints.max_height});
        m_size = sz;

        if (m_direction == SplitDirection::Horizontal) {
            float max_first = std::max(m_min_first, sz.x - m_splitter_thickness - m_min_second);
            float actual_split = std::clamp(m_split_position, m_min_first, max_first);

            if (m_first_box) {
                m_first_box->layout(BoxConstraints::tight(Vec2{actual_split, sz.y}));
                m_first_box->set_offset(Vec2{0.0f, 0.0f});
            }

            m_splitter_rect = Rect::from_xywh(actual_split, 0.0f, m_splitter_thickness, sz.y);

            if (m_second_box) {
                float second_w = std::max(0.0f, sz.x - actual_split - m_splitter_thickness);
                m_second_box->layout(BoxConstraints::tight(Vec2{second_w, sz.y}));
                m_second_box->set_offset(Vec2{actual_split + m_splitter_thickness, 0.0f});
            }
        } else {
            float max_first = std::max(m_min_first, sz.y - m_splitter_thickness - m_min_second);
            float actual_split = std::clamp(m_split_position, m_min_first, max_first);

            if (m_first_box) {
                m_first_box->layout(BoxConstraints::tight(Vec2{sz.x, actual_split}));
                m_first_box->set_offset(Vec2{0.0f, 0.0f});
            }

            m_splitter_rect = Rect::from_xywh(0.0f, actual_split, sz.x, m_splitter_thickness);

            if (m_second_box) {
                float second_h = std::max(0.0f, sz.y - actual_split - m_splitter_thickness);
                m_second_box->layout(BoxConstraints::tight(Vec2{sz.x, second_h}));
                m_second_box->set_offset(Vec2{0.0f, actual_split + m_splitter_thickness});
            }
        }
    }

    void paint(PaintContext& context) override {
        // 绘制子节点
        RenderBox::paint(context);

        // 绘制分割栏与居中 1px 细线
        Color split_bg = m_is_dragging ? Color{0.18f, 0.35f, 0.65f, 0.50f}
                                       : (m_is_splitter_hovered ? Color{0.24f, 0.24f, 0.28f, 0.40f} : Color::transparent());
        context.draw_rect(m_splitter_rect, split_bg, 0.0f, 0.0f);

        if (m_direction == SplitDirection::Horizontal) {
            float mid_x = m_splitter_rect.min_x + (m_splitter_thickness - 1.0f) * 0.5f;
            Color line_col = m_is_dragging ? Color{0.25f, 0.58f, 0.95f, 1.0f}
                                           : (m_is_splitter_hovered ? Color{0.40f, 0.40f, 0.45f, 1.0f} : Color{0.22f, 0.22f, 0.26f, 1.0f});
            context.draw_rect(Rect::from_xywh(mid_x, 0.0f, 1.0f, m_size.y), line_col, 0.0f, 0.0f);
        } else {
            float mid_y = m_splitter_rect.min_y + (m_splitter_thickness - 1.0f) * 0.5f;
            Color line_col = m_is_dragging ? Color{0.25f, 0.58f, 0.95f, 1.0f}
                                           : (m_is_splitter_hovered ? Color{0.40f, 0.40f, 0.45f, 1.0f} : Color{0.22f, 0.22f, 0.26f, 1.0f});
            context.draw_rect(Rect::from_xywh(0.0f, mid_y, m_size.x, 1.0f), line_col, 0.0f, 0.0f);
        }
    }

private:
    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            bool in_splitter = m_splitter_rect.contains(ev.local_position);
            if (in_splitter != m_is_splitter_hovered) {
                m_is_splitter_hovered = in_splitter;
                cursor_shape = in_splitter ? (m_direction == SplitDirection::Horizontal ? CursorShape::ResizeEW : CursorShape::ResizeNS)
                                           : CursorShape::Arrow;
                mark_needs_paint();
            }

            if (m_is_dragging) {
                float new_pos = (m_direction == SplitDirection::Horizontal) ? ev.local_position.x : ev.local_position.y;
                set_split_position(new_pos);
                if (m_on_change) m_on_change(m_split_position);
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary && m_splitter_rect.contains(ev.local_position)) {
                m_is_dragging = true;
                mark_needs_paint();
            }
        };

        on_pointer_up = [this](PointerEvent& ev) {
            if (m_is_dragging) {
                m_is_dragging = false;
                mark_needs_paint();
            }
        };
    }

    std::shared_ptr<RenderBox> m_first_box;
    std::shared_ptr<RenderBox> m_second_box;
    SplitDirection m_direction{SplitDirection::Horizontal};
    float m_split_position{240.0f};
    float m_min_first{60.0f};
    float m_min_second{60.0f};
    float m_splitter_thickness{5.0f};
    std::function<void(float)> m_on_change{nullptr};

    Rect m_splitter_rect;
    bool m_is_splitter_hovered{false};
    bool m_is_dragging{false};
};

class SplitPaneWidget;

// C++20 指定初始化器声明式分栏容器 (SplitPane)
struct SplitPane {
    WidgetPtr first{nullptr};
    WidgetPtr second{nullptr};
    SplitDirection direction{SplitDirection::Horizontal};
    float split_position{240.0f};
    BindRef<float> bind_split_position{nullptr};
    float min_first{60.0f};
    float min_second{60.0f};
    float splitter_thickness{5.0f};
    std::function<void(float)> on_split_changed{nullptr};

    operator WidgetPtr() const;
};

class SplitPaneWidget : public Widget {
public:
    explicit SplitPaneWidget(const SplitPane& desc) : m_desc(desc) {}
    ~SplitPaneWidget() override { unsubscribe(); }

    std::shared_ptr<RenderBox> create_render_node() override {
        float init_pos = m_desc.bind_split_position ? m_desc.bind_split_position->get() : m_desc.split_position;
        auto f_node = m_desc.first ? m_desc.first->create_render_node() : nullptr;
        auto s_node = m_desc.second ? m_desc.second->create_render_node() : nullptr;

        auto on_ch = [this](float pos) {
            if (m_desc.bind_split_position) {
                m_desc.bind_split_position->set(pos);
            }
            if (m_desc.on_split_changed) {
                m_desc.on_split_changed(pos);
            }
        };

        auto node = std::make_shared<RenderSplitPane>(
            f_node, s_node, m_desc.direction, init_pos,
            m_desc.min_first, m_desc.min_second, m_desc.splitter_thickness, on_ch);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto split_box = std::dynamic_pointer_cast<RenderSplitPane>(node)) {
            float cur_pos = m_desc.bind_split_position ? m_desc.bind_split_position->get() : m_desc.split_position;
            split_box->set_split_position(cur_pos);
            setup_binding(split_box);
        }
    }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_split_position) {
            m_desc.bind_split_position->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderSplitPane> node) {
        unsubscribe();
        if (m_desc.bind_split_position) {
            m_binding_token = m_desc.bind_split_position->subscribe([node_weak = std::weak_ptr<RenderSplitPane>(node)](float pos) {
                if (auto n = node_weak.lock()) {
                    n->set_split_position(pos);
                }
            });
        }
    }

    SplitPane m_desc;
    uint64_t m_binding_token{0};
};

inline SplitPane::operator WidgetPtr() const {
    return std::make_shared<SplitPaneWidget>(*this);
}

} // namespace ktui

#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <algorithm>

namespace ktui {

// 树节点数据结构 (TreeNode)
struct TreeNode {
    std::string label{""};
    std::string icon{""};
    bool is_expanded{false};
    bool is_selected{false};
    std::vector<TreeNode> children;
    std::function<void()> on_select{nullptr};
};

// 扁平化可视树条目
struct FlattenedTreeItem {
    TreeNode* original_node{nullptr};
    std::string label;
    std::string icon;
    size_t depth{0};
    bool has_children{false};
    bool is_expanded{false};
    bool is_selected{false};
};

// 桌面高信息密度树状视图渲染盒 (RenderTreeView)
class RenderTreeView : public RenderBox {
public:
    explicit RenderTreeView(std::vector<TreeNode> roots, float item_h = 22.0f, float font_sz = 12.5f)
        : m_roots(std::move(roots)), m_item_height(item_h), m_font_size(font_sz) {
        cursor_shape = CursorShape::PointingHand;
        rebuild_flattened_items();
        setup_behavior();
    }

    void set_roots(std::vector<TreeNode> roots) {
        m_roots = std::move(roots);
        rebuild_flattened_items();
        mark_needs_layout();
    }

    [[nodiscard]] const std::vector<TreeNode>& roots() const noexcept { return m_roots; }

    void perform_layout() override {
        float total_h = m_flattened.size() * m_item_height;
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 200.0f;
        m_size = Vec2{w, total_h};
    }

    void paint(PaintContext& context) override {
        // 背景
        context.draw_rect(bounds(), m_bg_color, 0.0f, 0.0f);

        for (size_t i = 0; i < m_flattened.size(); ++i) {
            const auto& item = m_flattened[i];
            Rect row_rect = Rect::from_xywh(0.0f, i * m_item_height, m_size.x, m_item_height);

            bool is_hov = (i == m_hovered_index);
            bool is_sel = item.is_selected;

            if (is_sel) {
                context.draw_rect(row_rect, m_selected_bg, 2.0f, 0.0f);
            } else if (is_hov) {
                context.draw_rect(row_rect, m_hover_bg, 2.0f, 0.0f);
            }

            float indent_x = item.depth * 14.0f + 6.0f;
            float text_y = row_rect.min_y + (m_item_height - m_font_size * 1.30f) * 0.5f;

            // 折叠/展开指示三角
            if (item.has_children) {
                const char* arrow = item.is_expanded ? "v" : ">";
                context.draw_text(arrow, PointF{indent_x, text_y}, 10.0f, Color{0.65f, 0.65f, 0.70f, 1.0f});
            }

            // 节点名称
            float label_x = indent_x + (item.has_children ? 14.0f : 10.0f);
            Color text_col = is_sel ? Color::white() : (is_hov ? Color{0.95f, 0.95f, 0.98f, 1.0f} : m_text_color);
            context.draw_text(item.label, PointF{label_x, text_y}, m_font_size, text_col);
        }
    }

private:
    void rebuild_flattened_items() {
        m_flattened.clear();
        for (auto& root : m_roots) {
            flatten_node(&root, 0);
        }
    }

    void flatten_node(TreeNode* node, size_t depth) {
        if (!node) return;
        FlattenedTreeItem item;
        item.original_node = node;
        item.label = node->label;
        item.icon = node->icon;
        item.depth = depth;
        item.has_children = !node->children.empty();
        item.is_expanded = node->is_expanded;
        item.is_selected = node->is_selected;
        m_flattened.push_back(item);

        if (node->is_expanded) {
            for (auto& child : node->children) {
                flatten_node(&child, depth + 1);
            }
        }
    }

    void clear_all_selection(std::vector<TreeNode>& nodes) {
        for (auto& n : nodes) {
            n.is_selected = false;
            clear_all_selection(n.children);
        }
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& ev) {
            size_t idx = static_cast<size_t>(ev.local_position.y / m_item_height);
            if (idx >= m_flattened.size()) idx = static_cast<size_t>(-1);
            if (idx != m_hovered_index) {
                m_hovered_index = idx;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            size_t idx = static_cast<size_t>(ev.local_position.y / m_item_height);
            if (idx < m_flattened.size()) {
                auto* orig = m_flattened[idx].original_node;
                if (!orig) return;

                float indent_x = m_flattened[idx].depth * 14.0f + 6.0f;
                // 若点击在左侧折叠箭头区域
                if (m_flattened[idx].has_children && ev.local_position.x < indent_x + 16.0f) {
                    orig->is_expanded = !orig->is_expanded;
                } else {
                    // 选中节点
                    clear_all_selection(m_roots);
                    orig->is_selected = true;
                    if (orig->on_select) {
                        orig->on_select();
                    }
                }
                rebuild_flattened_items();
                mark_needs_layout();
            }
        };
    }

    std::vector<TreeNode> m_roots;
    std::vector<FlattenedTreeItem> m_flattened;
    float m_item_height{22.0f};
    float m_font_size{12.5f};
    size_t m_hovered_index{static_cast<size_t>(-1)};

    Color m_bg_color{Color{0.11f, 0.11f, 0.13f, 1.0f}};
    Color m_hover_bg{Color{0.18f, 0.18f, 0.22f, 1.0f}};
    Color m_selected_bg{Color{0.16f, 0.35f, 0.65f, 1.0f}}; // 桌面蓝高亮
    Color m_text_color{Color{0.82f, 0.82f, 0.86f, 1.0f}};
};

class TreeViewWidget;

// C++20 指定初始化器声明式树状图控件 (TreeView)
struct TreeView {
    std::vector<TreeNode> roots;
    float item_height{22.0f};
    float font_size{12.5f};

    operator WidgetPtr() const;
};

class TreeViewWidget : public Widget {
public:
    explicit TreeViewWidget(const TreeView& desc) : m_desc(desc) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return std::make_shared<RenderTreeView>(m_desc.roots, m_desc.item_height, m_desc.font_size);
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto tree = std::dynamic_pointer_cast<RenderTreeView>(node)) {
            tree->set_roots(m_desc.roots);
        }
    }

private:
    TreeView m_desc;
};

inline TreeView::operator WidgetPtr() const {
    return std::make_shared<TreeViewWidget>(*this);
}

} // namespace ktui

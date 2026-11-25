#pragma once

#include "checkbox.hpp"
#include "common/fluent_icons.hpp"
#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ktui {

enum class TreeViewSelectionMode : uint8_t {
    Single = 0,
    Multiple = 1,
    None = 2
};

struct TreeViewNode {
    std::string id;
    std::string label;
    std::string svg_path;
    bool is_expanded{false};
    bool is_checked{false};
    std::vector<TreeViewNode> children;

    bool operator==(const TreeViewNode&) const = default;
};

// WinUI 3 TreeView 树形视图渲染盒
// 支持多级缩进折叠/展开、Fluent 选中指示条、Multiple 模式父子三态复选框联动、滚轮与触控滚动
class RenderTreeView : public RenderBox {
public:
    RenderTreeView() noexcept {
        cursor_shape = CursorShape::Arrow;
        setup_behavior();
    }

    ~RenderTreeView() override = default;

    void set_properties(std::vector<TreeViewNode> nodes,
                        TreeViewSelectionMode selection_mode,
                        std::string selected_id,
                        float width,
                        float height,
                        Fn<void(const std::string&, const std::string&)> on_selection_changed,
                        Fn<void(const std::vector<std::string>&)> on_checked_changed) {
        m_nodes = std::move(nodes);
        m_selection_mode = selection_mode;
        m_selected_id = std::move(selected_id);
        m_width = width;
        m_height = height;
        m_on_selection_changed = std::move(on_selection_changed);
        m_on_checked_changed = std::move(on_checked_changed);
        rebuild_flat_rows();
        mark_needs_layout();
    }

    void set_selected_id_value(const std::string& id) {
        if (m_selected_id != id) {
            m_selected_id = id;
            mark_needs_paint();
        }
    }

    [[nodiscard]] size_t visible_row_count() const noexcept { return m_rows.size(); }
    [[nodiscard]] const std::string& selected_id() const noexcept { return m_selected_id; }

    bool toggle_expand_by_id(const std::string& id) {
        if (TreeViewNode* n = find_node(m_nodes, id)) {
            if (!n->children.empty()) {
                n->is_expanded = !n->is_expanded;
                rebuild_flat_rows();
                mark_needs_layout();
                return true;
            }
        }
        return false;
    }

    bool toggle_check_by_id(const std::string& id) {
        if (TreeViewNode* n = find_node(m_nodes, id)) {
            CheckState st = compute_check_state(*n);
            bool next = (st != CheckState::Checked);
            set_subtree_checked(*n, next);
            rebuild_flat_rows(true);
            mark_needs_paint();
            if (m_on_checked_changed) m_on_checked_changed(checked_ids());
            return true;
        }
        return false;
    }

    [[nodiscard]] std::vector<std::string> checked_ids() const {
        std::vector<std::string> out;
        collect_checked(m_nodes, out);
        return out;
    }

    void perform_layout() override {
        float w = (m_width > 0.0f)
            ? m_width
            : (std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 320.0f);
        float content_h = static_cast<float>(m_rows.size()) * kRowH + 8.0f;
        float h = (m_height > 0.0f) ? m_height : std::max(44.0f, content_h);
        set_size(m_constraints.constrain(Vec2{w, h}));
        clamp_scroll();
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        Color panel_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.025f} : Color{0.0f, 0.0f, 0.0f, 0.018f};
        Color border = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
        Rect panel = Rect::from_xywh(0.0f, 0.0f, w, h);
        ctx.draw_rect(panel, panel_bg, 6.0f, 0.0f);
        ctx.draw_rect(panel, border, 6.0f, 1.0f);

        Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f} : Color{0.0f, 0.0f, 0.0f, 0.90f};
        Color chev_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.62f} : Color{0.0f, 0.0f, 0.0f, 0.58f};
        Color hov_bg   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.055f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
        Color sel_bg   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.085f} : Color{0.0f, 0.0f, 0.0f, 0.058f};

        ctx.push_clip(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), std::max(0.0f, h - 2.0f)));

        bool any_cb_animating = false;
        float y = 4.0f - m_scroll_offset;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const FlatRow& row = m_rows[i];
            Rect rr = Rect::from_xywh(4.0f, y, std::max(0.0f, w - 8.0f), kRowH);
            if (rr.max_y >= 0.0f && rr.min_y <= h) {
                bool is_sel = (m_selection_mode == TreeViewSelectionMode::Single &&
                               !m_selected_id.empty() && row.node->id == m_selected_id);
                bool is_hov = (static_cast<int>(i) == m_hovered_row);

                if (is_sel) {
                    ctx.draw_rect(rr, sel_bg, 4.0f, 0.0f);
                    ctx.draw_rect(Rect::from_xywh(rr.min_x + 2.0f, rr.min_y + 8.0f, 3.0f, kRowH - 16.0f),
                                  theme.colors.primary, 1.5f, 0.0f);
                } else if (is_hov) {
                    ctx.draw_rect(rr, hov_bg, 4.0f, 0.0f);
                }

                float x = rr.min_x + 10.0f + static_cast<float>(row.depth) * kIndent;

                if (!row.node->children.empty()) {
                    Rect cr = Rect::from_xywh(std::round(x + 2.0f), std::round(rr.min_y + (kRowH - 12.0f) * 0.5f), 12.0f, 12.0f);
                    SvgEngine::draw_svg_xml(ctx, row.node->is_expanded ? fluent_icons::kChevronDown : fluent_icons::kChevronRight, cr, chev_col);
                }
                x += 22.0f;

                // 复用现有 RenderCheckbox 控件 (自带 CUI 三态样式、对勾生长与半选横杠动效)
                if (m_selection_mode == TreeViewSelectionMode::Multiple) {
                    if (auto* cb = find_checkbox(row.node->id)) {
                        cb->clear_needs_paint();
                        PaintContext::OffsetScope scope(ctx, Vec2{std::round(x), std::round(rr.min_y + (kRowH - 18.0f) * 0.5f)});
                        cb->paint(ctx);
                        if (cb->needs_paint()) any_cb_animating = true;
                    }
                    x += 26.0f;
                }

                if (!row.node->svg_path.empty()) {
                    Rect ir = Rect::from_xywh(std::round(x), std::round(rr.min_y + (kRowH - 15.0f) * 0.5f), 15.0f, 15.0f);
                    SvgEngine::draw_svg_path(ctx, row.node->svg_path, 24.0f, 24.0f, ir, is_sel ? theme.colors.primary : text_col);
                    x += 22.0f;
                }

                Vec2 ts = TextMeasure::measure(row.node->label, 13.5f);
                ctx.draw_text(row.node->label,
                              PointF{std::round(x), std::round(rr.min_y + (kRowH - ts.y) * 0.5f)},
                              13.5f, text_col);
            }
            y += kRowH;
        }

        float content_h = static_cast<float>(m_rows.size()) * kRowH + 8.0f;
        if (content_h > h + 1.0f) {
            float ratio = h / content_h;
            float thumb_h = std::max(24.0f, (h - 8.0f) * ratio);
            float max_scroll = content_h - h;
            float thumb_y = 4.0f + (m_scroll_offset / max_scroll) * (h - 8.0f - thumb_h);
            Color sb_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.28f} : Color{0.0f, 0.0f, 0.0f, 0.26f};
            ctx.draw_rect(Rect::from_xywh(w - 5.0f, thumb_y, 3.0f, thumb_h), sb_col, 1.5f, 0.0f);
        }

        ctx.pop_clip();
        if (any_cb_animating) mark_needs_paint();
    }

private:
    static constexpr float kRowH = 34.0f;
    static constexpr float kIndent = 20.0f;

    struct FlatRow {
        TreeViewNode* node{nullptr};
        int depth{0};
        CheckState check_state{CheckState::Unchecked};
    };

    static TreeViewNode* find_node(std::vector<TreeViewNode>& list, const std::string& id) {
        for (auto& n : list) {
            if (n.id == id) return &n;
            if (auto* sub = find_node(n.children, id)) return sub;
        }
        return nullptr;
    }

    static void set_subtree_checked(TreeViewNode& node, bool checked) {
        node.is_checked = checked;
        for (auto& c : node.children) set_subtree_checked(c, checked);
    }

    static CheckState compute_check_state(const TreeViewNode& node) {
        if (node.children.empty()) return node.is_checked ? CheckState::Checked : CheckState::Unchecked;
        bool all_checked = true;
        bool any_checked = false;
        for (const auto& c : node.children) {
            CheckState cs = compute_check_state(c);
            if (cs != CheckState::Checked) all_checked = false;
            if (cs != CheckState::Unchecked) any_checked = true;
        }
        if (all_checked) return CheckState::Checked;
        if (any_checked) return CheckState::Indeterminate;
        return CheckState::Unchecked;
    }

    static void collect_checked(const std::vector<TreeViewNode>& list, std::vector<std::string>& out) {
        for (const auto& n : list) {
            if (n.children.empty()) {
                if (n.is_checked) out.push_back(n.id);
            } else {
                if (compute_check_state(n) == CheckState::Checked) out.push_back(n.id);
                collect_checked(n.children, out);
            }
        }
    }

    RenderCheckbox* find_checkbox(const std::string& id) const noexcept {
        for (const auto& kv : m_checkboxes) {
            if (kv.first == id) return kv.second.get();
        }
        return nullptr;
    }

    void sync_checkbox_node(const std::string& id, CheckState cs, bool animate) {
        auto* existing = find_checkbox(id);
        if (!existing) {
            auto cb = make_render_node<RenderCheckbox>();
            cb->set_three_state(true);
            cb->layout(BoxConstraints::tight(Vec2{18.0f, 18.0f}));
            cb->set_state(cs);
            m_checkboxes.emplace_back(id, std::move(cb));
        } else if (existing->state() != cs) {
            (void)animate;
            existing->set_state(cs);
        }
    }

    void flatten_recursive(std::vector<TreeViewNode>& list, int depth, bool animate_cb) {
        for (auto& n : list) {
            CheckState cs = (m_selection_mode == TreeViewSelectionMode::Multiple)
                ? compute_check_state(n) : CheckState::Unchecked;
            if (m_selection_mode == TreeViewSelectionMode::Multiple) {
                sync_checkbox_node(n.id, cs, animate_cb);
            }
            m_rows.push_back(FlatRow{&n, depth, cs});
            if (n.is_expanded && !n.children.empty()) {
                flatten_recursive(n.children, depth + 1, animate_cb);
            }
        }
    }

    void rebuild_flat_rows(bool animate_cb = false) {
        m_rows.clear();
        flatten_recursive(m_nodes, 0, animate_cb);
        clamp_scroll();
    }

    void clamp_scroll() {
        float content_h = static_cast<float>(m_rows.size()) * kRowH + 8.0f;
        float max_scroll = std::max(0.0f, content_h - size().y);
        m_scroll_offset = std::clamp(m_scroll_offset, 0.0f, max_scroll);
    }

    int hit_row(Vec2 local) const noexcept {
        if (local.x < 0.0f || local.x > size().x || local.y < 4.0f || local.y > size().y - 4.0f) return -1;
        int idx = static_cast<int>(std::floor((local.y - 4.0f + m_scroll_offset) / kRowH));
        if (idx < 0 || idx >= static_cast<int>(m_rows.size())) return -1;
        return idx;
    }

    void setup_behavior() {
        on_pointer_move = [this](PointerEvent& e) {
            if (e.type == PlatformEventType::PointerScroll) {
                float content_h = static_cast<float>(m_rows.size()) * kRowH + 8.0f;
                if (content_h > size().y) {
                    float prev = m_scroll_offset;
                    m_scroll_offset -= e.delta.y * 34.0f;
                    clamp_scroll();
                    if (std::abs(m_scroll_offset - prev) > 0.01f) {
                        mark_needs_paint();
                        e.stop_propagation();
                    }
                }
                return;
            }
            if (m_touch_dragging && e.device_kind == PointerDeviceKind::Touch) {
                float dy = e.local_position.y - m_touch_last_y;
                m_touch_last_y = e.local_position.y;
                if (std::abs(e.local_position.y - m_touch_start_y) > 6.0f) {
                    m_touch_moved = true;
                }
                m_scroll_offset -= dy;
                clamp_scroll();
                mark_needs_paint();
                return;
            }
            int idx = hit_row(e.local_position);
            cursor_shape = (idx >= 0) ? CursorShape::PointingHand : CursorShape::Arrow;
            if (idx != m_hovered_row) {
                update_checkbox_hover(m_hovered_row, false);
                m_hovered_row = idx;
                update_checkbox_hover(m_hovered_row, true);
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_hovered_row != -1) {
                update_checkbox_hover(m_hovered_row, false);
                m_hovered_row = -1;
                mark_needs_paint();
            }
            m_touch_dragging = false;
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            if (e.device_kind == PointerDeviceKind::Touch) {
                m_touch_dragging = true;
                m_touch_moved = false;
                m_touch_start_y = e.local_position.y;
                m_touch_last_y = e.local_position.y;
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            bool was_drag = m_touch_dragging && m_touch_moved;
            m_touch_dragging = false;
            m_touch_moved = false;
            if (was_drag || e.button != PointerButton::Primary) return;

            int idx = hit_row(e.local_position);
            if (idx < 0 || idx >= static_cast<int>(m_rows.size())) return;

            FlatRow& row = m_rows[static_cast<size_t>(idx)];
            float chev_x0 = 4.0f + 10.0f + static_cast<float>(row.depth) * kIndent;
            float chev_x1 = chev_x0 + 22.0f;

            if (!row.node->children.empty() && e.local_position.x >= chev_x0 - 4.0f && e.local_position.x <= chev_x1) {
                row.node->is_expanded = !row.node->is_expanded;
                rebuild_flat_rows();
                mark_needs_layout();
                e.stop_propagation();
                return;
            }

            if (m_selection_mode == TreeViewSelectionMode::Multiple) {
                CheckState st = compute_check_state(*row.node);
                set_subtree_checked(*row.node, st != CheckState::Checked);
                rebuild_flat_rows(true);
                mark_needs_paint();
                if (m_on_checked_changed) m_on_checked_changed(checked_ids());
                e.stop_propagation();
                return;
            }

            if (m_selection_mode == TreeViewSelectionMode::Single) {
                m_selected_id = row.node->id;
                mark_needs_paint();
                if (m_on_selection_changed) m_on_selection_changed(row.node->id, row.node->label);
                e.stop_propagation();
            }
        };
    }

    void update_checkbox_hover(int row_idx, bool hovered) {
        if (m_selection_mode != TreeViewSelectionMode::Multiple) return;
        if (row_idx < 0 || row_idx >= static_cast<int>(m_rows.size())) return;
        auto* cb = find_checkbox(m_rows[static_cast<size_t>(row_idx)].node->id);
        if (!cb) return;
        PointerEvent pe;
        if (hovered && cb->on_pointer_enter) cb->on_pointer_enter(pe);
        else if (!hovered && cb->on_pointer_exit) cb->on_pointer_exit(pe);
    }

    std::vector<TreeViewNode> m_nodes;
    std::vector<FlatRow> m_rows;
    std::vector<std::pair<std::string, std::shared_ptr<RenderCheckbox>>> m_checkboxes;
    TreeViewSelectionMode m_selection_mode{TreeViewSelectionMode::Single};
    std::string m_selected_id;
    float m_width{0.0f};
    float m_height{0.0f};
    float m_scroll_offset{0.0f};
    int m_hovered_row{-1};
    bool m_touch_dragging{false};
    bool m_touch_moved{false};
    float m_touch_start_y{0.0f};
    float m_touch_last_y{0.0f};

    Fn<void(const std::string&, const std::string&)> m_on_selection_changed;
    Fn<void(const std::vector<std::string>&)> m_on_checked_changed;

public:
    void wrap_on_selection_changed(Fn<void(const std::string&, const std::string&)> extra) {
        auto prev = std::move(m_on_selection_changed);
        m_on_selection_changed = [prev = std::move(prev), extra = std::move(extra)](const std::string& id, const std::string& label) {
            if (extra) extra(id, label);
            if (prev) prev(id, label);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 TreeView
struct TreeView : BindableMixin<TreeView, RenderTreeView> {
    std::vector<TreeViewNode> nodes;
    TreeViewSelectionMode selection_mode{TreeViewSelectionMode::Single};
    std::string selected_id{std::string{}};
    float width{0.0f};
    float height{0.0f};
    Fn<void(const std::string&, const std::string&)> on_selection_changed{nullptr};
    Fn<void(const std::vector<std::string>&)> on_checked_changed{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderTreeView>& node, Property<std::string>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_selected_id_value(prop.get());
    std::weak_ptr<RenderTreeView> weak = node;
    bg.subscribe(prop, [weak, guard](const std::string& id) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_selected_id_value(id);
        }
    });
    Property<std::string>* p = &prop;
    node->wrap_on_selection_changed([p, guard](const std::string& id, const std::string&) {
        *guard = true;
        p->set(id);
        *guard = false;
    });
}

class TreeViewWidget : public Widget {
public:
    explicit TreeViewWidget(const TreeView& desc) : m_desc(desc) {}
    explicit TreeViewWidget(TreeView&& desc) noexcept : m_desc(std::move(desc)) {}
    ~TreeViewWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTreeView>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderTreeView>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderTreeView>& node) {
        node->set_properties(
            m_desc.nodes,
            m_desc.selection_mode,
            m_desc.selected_id,
            m_desc.width,
            m_desc.height,
            m_desc.on_selection_changed,
            m_desc.on_checked_changed);
    }

    TreeView m_desc;
};

inline TreeView::operator WidgetPtr() const& {
    return make_widget<TreeViewWidget>(*this);
}
inline TreeView::operator WidgetPtr() && {
    return make_widget<TreeViewWidget>(std::move(*this));
}

} // namespace ktui

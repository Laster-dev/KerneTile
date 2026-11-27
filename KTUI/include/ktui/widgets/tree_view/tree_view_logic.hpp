#pragma once

#include "tree_view_data.hpp"
#include "tree_view_style.hpp"
#include "../common/fluent_icons.hpp"
#include "../scroll/scrollbar.hpp"
#include "../scroll/smooth_scroll_physics.hpp"
#include <ktui/event/event_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ktui {

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
                        Fn<void(const std::vector<std::string>&)> on_checked_changed,
                        std::vector<TreeListColumn> columns = {}) {
        m_nodes = std::move(nodes);
        m_columns = std::move(columns);
        m_selection_mode = selection_mode;
        m_selected_id = std::move(selected_id);
        m_width = width;
        m_height = height;
        m_on_selection_changed = std::move(on_selection_changed);
        m_on_checked_changed = std::move(on_checked_changed);
        m_user_resized = false;
        sync_expand_anim_targets(m_nodes, true);
        rebuild_flat_rows();
        mark_needs_layout();
    }

    void set_selected_id_value(const std::string& id) {
        if (m_selected_id != id) {
            m_selected_id = id;
            mark_needs_paint();
        }
    }

    void set_nodes(std::vector<TreeViewNode> nodes) {
        m_nodes = std::move(nodes);
        sync_expand_anim_targets(m_nodes, true);
        rebuild_flat_rows();
        mark_needs_layout();
    }

    [[nodiscard]] size_t visible_row_count() const noexcept {
        size_t cnt = 0;
        for (const auto& r : m_rows) {
            if (!r.collapsing) ++cnt;
        }
        return cnt;
    }
    [[nodiscard]] const std::string& selected_id() const noexcept { return m_selected_id; }

    bool toggle_expand_by_id(const std::string& id) {
        if (TreeViewNode* n = find_node(m_nodes, id)) {
            if (!n->children.empty()) {
                n->is_expanded = !n->is_expanded;
                m_expand_anim_active = true;
                m_last_expand_time = std::chrono::steady_clock::now();
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
        float content_h = header_height() + total_rows_height() + 8.0f;
        float h = (m_height > 0.0f) ? m_height : std::max(44.0f, content_h);
        set_size(m_constraints.constrain(Vec2{w, h}));
        if (!m_user_resized) compute_column_widths();
        clamp_scroll();
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const auto resolved_style = TreeViewStyleResolver::resolve(theme);
        const float w = size().x;
        const float h = size().y;

        // 推进折叠/展开平滑高度动画、惯性平滑滚动与滚动条淡入淡出
        bool anim_exp = step_expand_animations();
        if (anim_exp) {
            rebuild_flat_rows();
        }
        bool anim_scroll = m_physics.update();
        bool anim_bar = m_scrollbar.update_animations(m_viewport_hovered);
        if (anim_exp || anim_scroll || anim_bar) {
            mark_needs_paint();
        }

        Color panel_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.025f} : Color{0.0f, 0.0f, 0.0f, 0.018f};
        Color border = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
        Rect panel = Rect::from_xywh(0.0f, 0.0f, w, h);
        ctx.draw_rect(panel, panel_bg, 6.0f, 0.0f);
        ctx.draw_rect(panel, border, 6.0f, 1.0f);

        Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.94f} : Color{0.0f, 0.0f, 0.0f, 0.90f};
        Color sub_col  = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.70f} : Color{0.0f, 0.0f, 0.0f, 0.65f};
        Color chev_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.64f} : Color{0.0f, 0.0f, 0.0f, 0.60f};
        Color hov_bg   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.055f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
        Color sel_bg   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.085f} : Color{0.0f, 0.0f, 0.0f, 0.058f};

        const float hdr_h = header_height();
        if (!m_columns.empty()) {
            Color head_bg = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.042f} : Color{0.0f, 0.0f, 0.0f, 0.032f};
            ctx.push_clip(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), hdr_h), 5.0f);
            ctx.draw_rect(Rect::from_xywh(1.0f, 1.0f, std::max(0.0f, w - 2.0f), hdr_h), head_bg, 0.0f, 0.0f);
            float cx = 4.0f;
            for (size_t c = 0; c < m_columns.size(); ++c) {
                float cw = (c < m_col_widths.size()) ? m_col_widths[c] : 140.0f;
                ctx.push_clip(Rect::from_xywh(cx + 6.0f, 1.0f, std::max(0.0f, cw - 12.0f), hdr_h));
                Vec2 ts = TextMeasure::measure(m_columns[c].title, 12.0f, FontWeight::SemiBold);
                ctx.draw_text(m_columns[c].title,
                              PointF{std::round(cx + 10.0f), std::round(1.0f + (hdr_h - ts.y) * 0.5f)},
                              12.0f, sub_col, FontWeight::SemiBold);
                ctx.pop_clip();
                if (c + 1 < m_columns.size()) {
                    bool div_hot = (m_resizing_col == static_cast<int>(c) || m_hover_divider == static_cast<int>(c));
                    Color dc = div_hot ? resolved_style.accent : theme.colors.divider_stroke;
                    ctx.draw_rect(Rect::from_xywh(cx + cw - 1.0f, div_hot ? 2.0f : 7.0f,
                                                  div_hot ? 2.0f : 1.0f, hdr_h - (div_hot ? 4.0f : 14.0f)),
                                  dc, 0.0f, 0.0f);
                }
                cx += cw;
            }
            ctx.pop_clip();
            ctx.draw_rect(Rect::from_xywh(1.0f, hdr_h, std::max(0.0f, w - 2.0f), 1.0f),
                          theme.colors.divider_stroke, 0.0f, 0.0f);
        }

        Rect body_vp = body_rect();
        const float scroll_y = m_physics.offset();
        float top_f = 0.0f;
        float bot_f = 0.0f;
        if (m_edge_feather && m_feather_size > 0.5f && body_vp.height() > 8.0f) {
            const float cap = std::min(m_feather_size, body_vp.height() * 0.35f);
            top_f = std::clamp(scroll_y, 0.0f, cap);
            const float rem = std::max(0.0f, m_physics.max_scroll() - scroll_y);
            bot_f = std::clamp(rem, 0.0f, cap);
        }
        ctx.push_feathered_clip(body_vp, 0.0f, top_f, bot_f);

        bool any_cb_animating = false;
        float y = body_vp.min_y + 4.0f - scroll_y;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const FlatRow& row = m_rows[i];
            float rh = kRowH * row.height_scale;
            if (rh < 0.5f) continue;

            Rect rr = Rect::from_xywh(4.0f, y, std::max(0.0f, w - 8.0f), rh);
            if (rr.max_y >= body_vp.min_y && rr.min_y <= body_vp.max_y) {
                bool clip_row = (row.height_scale < 0.99f);
                if (clip_row) ctx.push_clip(rr);

                float alpha_s = std::clamp(row.height_scale, 0.0f, 1.0f);
                bool is_sel = (m_selection_mode == TreeViewSelectionMode::Single &&
                               !m_selected_id.empty() && row.node->id == m_selected_id);
                bool is_hov = (static_cast<int>(i) == m_hovered_row);

                if (is_sel) {
                    Color sb = sel_bg; sb.a *= alpha_s;
                    Color ac = resolved_style.accent; ac.a *= alpha_s;
                    ctx.draw_rect(rr, sb, 4.0f, 0.0f);
                    ctx.draw_rect(Rect::from_xywh(rr.min_x + 2.0f, rr.min_y + 8.0f, 3.0f, std::max(4.0f, rh - 16.0f)),
                                  ac, 1.5f, 0.0f);
                } else if (is_hov) {
                    Color hb = hov_bg; hb.a *= alpha_s;
                    ctx.draw_rect(rr, hb, 4.0f, 0.0f);
                } else if (!m_columns.empty() && (i & 1u) != 0) {
                    Color zb = theme.is_dark ? Color{1, 1, 1, 0.016f * alpha_s} : Color{0, 0, 0, 0.016f * alpha_s};
                    ctx.draw_rect(rr, zb, 0.0f, 0.0f);
                }

                float col0_max_x = (!m_columns.empty() && !m_col_widths.empty()) ? (4.0f + m_col_widths[0] - 6.0f) : (rr.max_x - 6.0f);
                float row_cy = rr.min_y + kRowH * 0.5f;
                float x = rr.min_x + 10.0f + static_cast<float>(row.depth) * kIndent;

                // 平滑旋转折叠箭头 (0° = 右指 >, 90° = 下指 v)
                if (!row.node->children.empty()) {
                    float exp_p = get_expand_anim(row.node->id, row.node->is_expanded);
                    float ang = exp_p * 1.5707963f; // 0 .. pi/2
                    float ca = std::cos(ang);
                    float sa = std::sin(ang);
                    float cx = x + 7.0f;
                    float cy = row_cy;
                    auto rot = [&](float px, float py) -> PointF {
                        return PointF{cx + px * ca - py * sa, cy + px * sa + py * ca};
                    };
                    Color cc = chev_col; cc.a *= alpha_s;
                    PointF p0 = rot(-1.8f, -3.5f);
                    PointF p1 = rot(2.0f, 0.0f);
                    PointF p2 = rot(-1.8f, 3.5f);
                    ctx.draw_line(p0, p1, cc, 1.35f);
                    ctx.draw_line(p1, p2, cc, 1.35f);
                }
                x += 22.0f;

                // 复用现有 RenderCheckbox 控件
                if (m_selection_mode == TreeViewSelectionMode::Multiple) {
                    if (auto* cb = find_checkbox(row.node->id)) {
                        cb->clear_needs_paint();
                        PaintContext::OffsetScope scope(ctx, Vec2{std::round(x), row_cy - 9.0f});
                        cb->paint(ctx);
                        if (cb->needs_paint()) any_cb_animating = true;
                    }
                    x += 26.0f;
                }

                Color tc = text_col; tc.a *= alpha_s;
                if (!row.node->svg_path.empty()) {
                    Rect ir = Rect::from_xywh(std::round(x), row_cy - 7.5f, 15.0f, 15.0f);
                    Color ic = is_sel ? resolved_style.accent : text_col;
                    ic.a *= alpha_s;
                    SvgEngine::draw_svg_path(ctx, row.node->svg_path, 24.0f, 24.0f, ir, ic);
                    x += 22.0f;
                }

                float label_w = std::max(8.0f, col0_max_x - x);
                ctx.push_clip(Rect::from_xywh(x, rr.min_y, label_w, rh));
                Vec2 ts = TextMeasure::measure(row.node->label, 13.5f);
                ctx.draw_text(row.node->label, PointF{std::round(x), row_cy - ts.y * 0.5f}, 13.5f, tc);
                ctx.pop_clip();

                // TreeListView 附加列绘制
                if (!m_columns.empty()) {
                    float col_x = 4.0f + (!m_col_widths.empty() ? m_col_widths[0] : 180.0f);
                    Color sc = sub_col; sc.a *= alpha_s;
                    for (size_t c = 1; c < m_columns.size(); ++c) {
                        float cw = (c < m_col_widths.size()) ? m_col_widths[c] : 120.0f;
                        if (c - 1 < row.node->sub_columns.size() && !row.node->sub_columns[c - 1].empty()) {
                            const std::string& cell = row.node->sub_columns[c - 1];
                            ctx.push_clip(Rect::from_xywh(col_x + 6.0f, rr.min_y, std::max(0.0f, cw - 12.0f), rh));
                            Vec2 cs = TextMeasure::measure(cell, 12.5f);
                            ctx.draw_text(cell, PointF{std::round(col_x + 10.0f), row_cy - cs.y * 0.5f}, 12.5f, sc);
                            ctx.pop_clip();
                        }
                        col_x += cw;
                    }
                }

                if (clip_row) ctx.pop_clip();
            }
            y += rh;
        }

        ctx.pop_clip();

        if (m_scroll_hairline && scroll_y > 0.25f) {
            const float t = std::clamp(scroll_y / std::max(1.0f, m_hairline_fade_range), 0.0f, 1.0f);
            const Color hl = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, 0.14f * t}
                : Color{0.0f, 0.0f, 0.0f, 0.12f * t};
            ctx.draw_rect(Rect::from_xywh(body_vp.min_x, body_vp.min_y, body_vp.width(), 1.0f), hl, 0.0f, 0.0f);
        }

        // 统一复用 WinUIScrollBar 控件
        ctx.push_clip(body_vp);
        float total_h = total_rows_height() + 8.0f;
        m_scrollbar.paint(ctx, body_vp, total_h, scroll_y, theme);
        ctx.pop_clip();
        if (any_cb_animating) mark_needs_paint();
    }

    void set_rubber_band_enabled(bool enabled) noexcept {
        if (m_physics.is_rubber_band_enabled() != enabled) {
            m_physics.set_rubber_band_enabled(enabled);
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool rubber_band_enabled() const noexcept { return m_physics.is_rubber_band_enabled(); }

    void set_edge_feather(bool enabled, float feather_px = 20.0f) noexcept {
        const float f = std::max(0.0f, feather_px);
        if (m_edge_feather != enabled || std::abs(m_feather_size - f) > 0.01f) {
            m_edge_feather = enabled;
            m_feather_size = f;
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool edge_feather_enabled() const noexcept { return m_edge_feather; }

    void set_scroll_hairline(bool enabled, float fade_range = 18.0f) noexcept {
        const float fr = std::max(1.0f, fade_range);
        if (m_scroll_hairline != enabled || std::abs(m_hairline_fade_range - fr) > 0.01f) {
            m_scroll_hairline = enabled;
            m_hairline_fade_range = fr;
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool scroll_hairline_enabled() const noexcept { return m_scroll_hairline; }

private:
    static constexpr float kRowH = 34.0f;
    static constexpr float kHeaderH = 32.0f;
    static constexpr float kIndent = 20.0f;

    struct FlatRow {
        TreeViewNode* node{nullptr};
        int depth{0};
        CheckState check_state{CheckState::Unchecked};
        float height_scale{1.0f};
        bool collapsing{false};
    };

    [[nodiscard]] float header_height() const noexcept {
        return m_columns.empty() ? 0.0f : kHeaderH;
    }

    [[nodiscard]] Rect body_rect() const noexcept {
        float hh = header_height();
        return Rect::from_xywh(1.0f, 1.0f + hh, std::max(0.0f, size().x - 2.0f), std::max(0.0f, size().y - hh - 2.0f));
    }

    [[nodiscard]] float total_rows_height() const noexcept {
        float sum = 0.0f;
        for (const auto& r : m_rows) sum += kRowH * r.height_scale;
        return sum;
    }

    void compute_column_widths() {
        m_col_widths.assign(m_columns.size(), 120.0f);
        if (m_columns.empty()) return;
        float avail = std::max(60.0f, size().x - 18.0f);
        float fixed_sum = 0.0f;
        int flex_cnt = 0;
        for (size_t i = 0; i < m_columns.size(); ++i) {
            if (m_columns[i].width > 0.0f) {
                m_col_widths[i] = m_columns[i].width;
                fixed_sum += m_columns[i].width;
            } else {
                ++flex_cnt;
            }
        }
        if (flex_cnt > 0) {
            float flex_w = std::max(80.0f, (avail - fixed_sum) / static_cast<float>(flex_cnt));
            for (size_t i = 0; i < m_columns.size(); ++i) {
                if (m_columns[i].width <= 0.0f) m_col_widths[i] = flex_w;
            }
        }
    }

    int hit_header_divider(Vec2 local) const noexcept {
        if (m_columns.empty() || local.y < 1.0f || local.y > kHeaderH + 2.0f) return -1;
        float x = 4.0f;
        for (size_t i = 0; i + 1 < m_col_widths.size(); ++i) {
            x += m_col_widths[i];
            if (std::abs(local.x - x) <= 4.5f) return static_cast<int>(i);
        }
        return -1;
    }

    float get_expand_anim(const std::string& id, bool fallback_expanded) const noexcept {
        for (const auto& kv : m_expand_anim) {
            if (kv.first == id) return kv.second;
        }
        return fallback_expanded ? 1.0f : 0.0f;
    }

    void set_expand_anim(const std::string& id, float v) {
        for (auto& kv : m_expand_anim) {
            if (kv.first == id) { kv.second = v; return; }
        }
        m_expand_anim.emplace_back(id, v);
    }

    void sync_expand_anim_targets(const std::vector<TreeViewNode>& list, bool snap) {
        for (const auto& n : list) {
            if (!n.children.empty()) {
                if (snap) set_expand_anim(n.id, n.is_expanded ? 1.0f : 0.0f);
                sync_expand_anim_targets(n.children, snap);
            }
        }
    }

    bool step_expand_recursive(const std::vector<TreeViewNode>& list, float k) {
        bool moving = false;
        for (const auto& n : list) {
            if (!n.children.empty()) {
                float target = n.is_expanded ? 1.0f : 0.0f;
                float cur = get_expand_anim(n.id, n.is_expanded);
                if (std::abs(cur - target) > 0.008f) {
                    set_expand_anim(n.id, cur + (target - cur) * k);
                    moving = true;
                } else if (cur != target) {
                    set_expand_anim(n.id, target);
                }
                if (step_expand_recursive(n.children, k)) moving = true;
            }
        }
        return moving;
    }

    bool step_expand_animations() {
        if (!m_expand_anim_active) return false;
        auto now = std::chrono::steady_clock::now();
        float dt = std::clamp(std::chrono::duration<float>(now - m_last_expand_time).count(), 0.001f, 0.05f);
        m_last_expand_time = now;
        float k = 1.0f - std::exp(-dt * 18.0f);
        bool moving = step_expand_recursive(m_nodes, k);
        m_expand_anim_active = moving;
        return moving;
    }

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

    void flatten_recursive(std::vector<TreeViewNode>& list, int depth, bool animate_cb, float parent_scale, bool parent_collapsing) {
        for (auto& n : list) {
            CheckState cs = (m_selection_mode == TreeViewSelectionMode::Multiple)
                ? compute_check_state(n) : CheckState::Unchecked;
            if (m_selection_mode == TreeViewSelectionMode::Multiple) {
                sync_checkbox_node(n.id, cs, animate_cb);
            }
            m_rows.push_back(FlatRow{&n, depth, cs, parent_scale, parent_collapsing});
            if (!n.children.empty()) {
                float anim = get_expand_anim(n.id, n.is_expanded);
                if (n.is_expanded || anim > 0.01f) {
                    float child_scale = parent_scale * (n.is_expanded ? std::max(0.02f, anim) : anim);
                    flatten_recursive(n.children, depth + 1, animate_cb, child_scale, parent_collapsing || !n.is_expanded);
                }
            }
        }
    }

    void rebuild_flat_rows(bool animate_cb = false) {
        m_rows.clear();
        flatten_recursive(m_nodes, 0, animate_cb, 1.0f, false);
        clamp_scroll();
    }

    void clamp_scroll() {
        float body_h = std::max(1.0f, size().y - header_height() - 2.0f);
        float max_scroll = std::max(0.0f, total_rows_height() + 8.0f - body_h);
        m_physics.set_max_scroll(max_scroll);
    }

    int hit_row(Vec2 local) const noexcept {
        Rect vp = body_rect();
        if (!vp.contains(local)) return -1;
        float target_y = local.y - vp.min_y - 4.0f + m_physics.offset();
        if (target_y < 0.0f) return -1;
        float acc = 0.0f;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            float rh = kRowH * m_rows[i].height_scale;
            if (target_y >= acc && target_y < acc + rh) return static_cast<int>(i);
            acc += rh;
        }
        return -1;
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            m_viewport_hovered = true;
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& e) {
            Rect vp = body_rect();
            float total_h = total_rows_height() + 8.0f;

            if (e.type == PlatformEventType::PointerScroll) {
                if (m_physics.max_scroll() > 0.5f || m_physics.is_rubber_band_enabled()) {
                    m_physics.add_wheel_delta(e.delta.y);
                    m_scrollbar.notify_activity();
                    mark_needs_paint();
                    e.stop_propagation();
                }
                return;
            }
            if (m_resizing_col >= 0 && m_resizing_col < static_cast<int>(m_col_widths.size())) {
                float dx = e.local_position.x - m_resize_start_x;
                m_col_widths[static_cast<size_t>(m_resizing_col)] = std::max(64.0f, m_resize_start_w + dx);
                m_user_resized = true;
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            bool sb_consumed = false;
            if (m_scrollbar.handle_pointer_move(e.local_position, vp, total_h, m_physics, sb_consumed)) {
                mark_needs_paint();
            }
            if (sb_consumed) { e.stop_propagation(); return; }

            if (m_touch_dragging && e.device_kind == PointerDeviceKind::Touch) {
                float dy = e.local_position.y - m_touch_last_y;
                m_touch_last_y = e.local_position.y;
                if (std::abs(e.local_position.y - m_touch_start_y) > 6.0f) m_touch_moved = true;
                m_physics.apply_touch_drag(dy);
                m_scrollbar.notify_activity();
                mark_needs_paint();
                return;
            }
            int hdiv = hit_header_divider(e.local_position);
            int idx = (hdiv >= 0) ? -1 : hit_row(e.local_position);
            cursor_shape = (hdiv >= 0) ? CursorShape::ResizeEW : ((idx >= 0) ? CursorShape::PointingHand : CursorShape::Arrow);
            if (hdiv != m_hover_divider || idx != m_hovered_row) {
                m_hover_divider = hdiv;
                update_checkbox_hover(m_hovered_row, false);
                m_hovered_row = idx;
                update_checkbox_hover(m_hovered_row, true);
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_scrollbar.handle_pointer_exit();
            m_hover_divider = -1;
            if (m_hovered_row != -1) {
                update_checkbox_hover(m_hovered_row, false);
                m_hovered_row = -1;
            }
            m_touch_dragging = false;
            mark_needs_paint();
        };
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int hdiv = hit_header_divider(e.local_position);
            if (hdiv >= 0 && hdiv < static_cast<int>(m_col_widths.size())) {
                m_resizing_col = hdiv;
                m_resize_start_x = e.local_position.x;
                m_resize_start_w = m_col_widths[static_cast<size_t>(hdiv)];
                e.stop_propagation();
                return;
            }
            Rect vp = body_rect();
            float total_h = total_rows_height() + 8.0f;
            if (m_scrollbar.handle_pointer_down(e.local_position, vp, total_h, m_physics, kRowH * 2.0f)) {
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (e.device_kind == PointerDeviceKind::Touch) {
                m_touch_dragging = true;
                m_touch_moved = false;
                m_touch_start_y = e.local_position.y;
                m_touch_last_y = e.local_position.y;
                m_physics.begin_touch_drag();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_resizing_col >= 0) { m_resizing_col = -1; mark_needs_paint(); return; }
            if (m_scrollbar.handle_pointer_up()) { mark_needs_paint(); return; }
            bool was_drag = m_touch_dragging && m_touch_moved;
            if (m_touch_dragging && e.device_kind == PointerDeviceKind::Touch) {
                m_physics.end_touch_drag();
                mark_needs_paint();
            }
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
                m_expand_anim_active = true;
                m_last_expand_time = std::chrono::steady_clock::now();
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
    std::vector<TreeListColumn> m_columns;
    std::vector<float> m_col_widths;
    std::vector<FlatRow> m_rows;
    std::vector<std::pair<std::string, std::shared_ptr<RenderCheckbox>>> m_checkboxes;
    std::vector<std::pair<std::string, float>> m_expand_anim;
    bool m_expand_anim_active{false};
    std::chrono::steady_clock::time_point m_last_expand_time{};
    TreeViewSelectionMode m_selection_mode{TreeViewSelectionMode::Single};
    std::string m_selected_id;
    float m_width{0.0f};
    float m_height{0.0f};
    bool m_viewport_hovered{false};
    int m_hovered_row{-1};
    int m_hover_divider{-1};
    int m_resizing_col{-1};
    float m_resize_start_x{0.0f};
    float m_resize_start_w{0.0f};
    bool m_user_resized{false};
    bool m_touch_dragging{false};
    bool m_touch_moved{false};
    float m_touch_start_y{0.0f};
    float m_touch_last_y{0.0f};
    SmoothScrollPhysics m_physics;
    WinUIScrollBar m_scrollbar;

    Fn<void(const std::string&, const std::string&)> m_on_selection_changed;
    Fn<void(const std::vector<std::string>&)> m_on_checked_changed;

    bool m_edge_feather{true};
    float m_feather_size{20.0f};
    bool m_scroll_hairline{false};
    float m_hairline_fade_range{18.0f};

public:
    void wrap_on_selection_changed(Fn<void(const std::string&, const std::string&)> extra) {
        auto prev = std::move(m_on_selection_changed);
        m_on_selection_changed = [prev = std::move(prev), extra = std::move(extra)](const std::string& id, const std::string& label) {
            if (extra) extra(id, label);
            if (prev) prev(id, label);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 TreeView (支持 TreeListView 多列模式)
struct TreeView : BindableMixin<TreeView, RenderTreeView> {
    std::vector<TreeViewNode> nodes;
    TreeViewSelectionMode selection_mode{TreeViewSelectionMode::Single};
    std::string selected_id{std::string{}};
    float width{0.0f};
    float height{0.0f};
    Fn<void(const std::string&, const std::string&)> on_selection_changed{nullptr};
    Fn<void(const std::vector<std::string>&)> on_checked_changed{nullptr};
    std::vector<TreeListColumn> columns{}; // 非空时自动启用多列 TreeListView 模式
    bool rubber_band_overscroll{true};
    bool edge_feather{true};
    float feather_size{20.0f};
    bool scroll_hairline{false};
    float hairline_fade_range{18.0f};

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
            m_desc.on_checked_changed,
            m_desc.columns);
        node->set_rubber_band_enabled(m_desc.rubber_band_overscroll);
        node->set_edge_feather(m_desc.edge_feather, m_desc.feather_size);
        node->set_scroll_hairline(m_desc.scroll_hairline, m_desc.hairline_fade_range);
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
 // namespace ktui

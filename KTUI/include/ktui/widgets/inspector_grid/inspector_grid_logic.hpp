#pragma once

#include "inspector_grid_data.hpp"
#include "inspector_grid_style.hpp"
#include "../scroll/scrollbar.hpp"
#include <ktui/app/window.hpp>
#include <ktui/pal/clipboard.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ktui {

// 紧凑型键值对 / 属性检查器网格 (Compact Key-Value / Inspector Grid)
// - 两列高密度对齐（30px 行高），左侧键名 + 类型角标，右侧直接复用原生控件：
//   * bool  行：复用原生 RenderCheckbox（自带 Fluent 2 勾选笔触动效）
//   * int/float 行：复用单一共享原生 RenderNumberBox（支持步进按钮与表达式求值）
//   * str/hex 行：复用单一共享原生 RenderTextBox（零逐行控件膨胀）
// - 悬停一键复制单值（绿色 ✓ 动效）+ 非默认值一键恢复 + 垂直滚动条 + 滚轮/触控滚动
class RenderInspectorGrid : public RenderBox {
public:
    static constexpr float kRowH = 30.0f;

    RenderInspectorGrid(std::vector<InspectorRow> rows = {},
                        float key_column_ratio = 0.44f,
                        Fn<void(size_t, const std::string&)> on_value_changed = nullptr)
        : m_rows(std::move(rows)),
          m_key_ratio(std::clamp(key_column_ratio, 0.25f, 0.70f)),
          m_on_changed(std::move(on_value_changed)) {
        // 1. 共享文本编辑器 (RenderTextBox)
        m_text_editor = make_render_node<RenderTextBox>();
        m_text_editor->set_custom_background(Color::transparent());
        m_text_editor->set_custom_border_color(Color::transparent());
        m_text_editor->set_custom_focused_border_color(Color::transparent());
        m_text_editor->set_borderless_inline(true);
        m_text_editor->set_custom_height(24.0f);
        m_text_editor->set_custom_padding(EdgeInsets(2.0f, 4.0f));
        m_text_editor->set_custom_font_size(12.0f);
        m_text_editor->set_key_intercept([this](KeyEvent& ev) -> bool {
            if (ev.type == PlatformEventType::KeyDown) {
                if (ev.key_code == 0x0D) { commit_edit(); ev.stop_propagation(); return true; }
                if (ev.key_code == 0x1B) { cancel_edit(); ev.stop_propagation(); return true; }
            }
            return false;
        });
        auto prev_txt_focus = m_text_editor->on_focus;
        m_text_editor->on_focus = [this, prev_txt_focus](FocusEvent& ev) {
            if (prev_txt_focus) prev_txt_focus(ev);
            if (!ev.gained_focus && m_editing_row >= 0 && !m_editing_is_num) commit_edit();
        };
        append_child(m_text_editor.get());

        // 2. 共享数值编辑器 (RenderNumberBox)
        m_num_editor = make_render_node<RenderNumberBox>();
        if (auto& nb_in = m_num_editor->input_box()) {
            nb_in->set_custom_height(26.0f);
            nb_in->set_custom_font_size(12.0f);
            nb_in->set_custom_padding(EdgeInsets(2.0f, 34.0f, 2.0f, 6.0f));
            nb_in->set_key_intercept([this](KeyEvent& ev) -> bool {
                if (ev.type == PlatformEventType::KeyDown) {
                    if (ev.key_code == 0x0D) { commit_edit(); ev.stop_propagation(); return true; }
                    if (ev.key_code == 0x1B) { cancel_edit(); ev.stop_propagation(); return true; }
                }
                return false;
            });
            auto prev_num_focus = nb_in->on_focus;
            nb_in->on_focus = [this, prev_num_focus](FocusEvent& ev) {
                if (prev_num_focus) prev_num_focus(ev);
                if (!ev.gained_focus && m_editing_row >= 0 && m_editing_is_num) commit_edit();
            };
        }
        append_child(m_num_editor.get());

        rebuild_bool_checkboxes();
        setup_behavior();
    }

    ~RenderInspectorGrid() override {
        while (first_child()) remove_child(first_child());
    }

    [[nodiscard]] const std::vector<InspectorRow>& rows() const noexcept { return m_rows; }
    [[nodiscard]] float key_column_ratio() const noexcept { return m_key_ratio; }
    [[nodiscard]] RenderTextBox* text_editor() const noexcept { return m_text_editor.get(); }
    [[nodiscard]] RenderNumberBox* number_editor() const noexcept { return m_num_editor.get(); }

    void set_rows(std::vector<InspectorRow> rows) {
        cancel_edit();
        m_rows = std::move(rows);
        rebuild_bool_checkboxes();
        clamp_scroll();
        layout_children();
        mark_needs_paint();
    }

    void set_value(size_t idx, const std::string& val, bool notify = true) {
        if (idx >= m_rows.size() || m_rows[idx].value == val) return;
        m_rows[idx].value = val;
        sync_bool_checkbox(idx);
        mark_needs_paint();
        if (notify && m_on_changed) m_on_changed(idx, m_rows[idx].value);
    }

    void reset_all() {
        cancel_edit();
        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (!m_rows[i].default_value.empty() && m_rows[i].value != m_rows[i].default_value) {
                m_rows[i].value = m_rows[i].default_value;
                sync_bool_checkbox(i);
                if (m_on_changed) m_on_changed(i, m_rows[i].value);
            }
        }
        mark_needs_paint();
    }

    void set_on_value_changed(Fn<void(size_t, const std::string&)> cb) { m_on_changed = std::move(cb); }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 460.0f);
        float ideal_h = std::max(kRowH * 4.0f, static_cast<float>(m_rows.size()) * kRowH + 2.0f);
        float h = m_constraints.has_bounded_height() ? m_constraints.max_height : ideal_h;
        m_size = m_constraints.constrain(Vec2{w, h});
        clamp_scroll();
        layout_children();
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;

        if (is_on_divider(position.x)) {
            cursor_shape = CursorShape::ResizeEW;
            result.add(this, position);
            return true;
        }

        // 1. 优先命中当前激活的行内编辑器 (RenderNumberBox 或 RenderTextBox)
        if (m_editing_row >= 0) {
            RenderBox* ed = m_editing_is_num ? static_cast<RenderBox*>(m_num_editor.get())
                                             : static_cast<RenderBox*>(m_text_editor.get());
            if (ed) {
                PointF cp{position.x - ed->offset().x, position.y - ed->offset().y};
                if (ed->hit_test(result, cp)) {
                    result.add(this, position);
                    return true;
                }
            }
        }

        // 2. 命中可见 bool 行的原生 RenderCheckbox
        for (const auto& entry : m_bool_boxes) {
            if (!entry.box) continue;
            PointF cp{position.x - entry.box->offset().x, position.y - entry.box->offset().y};
            if (cp.x >= 0.0f && cp.y >= 0.0f && cp.x <= entry.box->size().x && cp.y <= entry.box->size().y) {
                if (entry.box->hit_test(result, cp)) {
                    result.add(this, position);
                    return true;
                }
            }
        }

        cursor_shape = CursorShape::Arrow;
        int row = row_at(position.y);
        if (row >= 0 && position.x >= m_size.x * m_key_ratio && !m_rows[static_cast<size_t>(row)].read_only) {
            cursor_shape = CursorShape::PointingHand;
        }
        result.add(this, position);
        return true;
    }

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        if (is_on_divider(position.x, true)) {
            result.add(this, position);
            return true;
        }
        if (m_editing_row >= 0) {
            RenderBox* ed = m_editing_is_num ? static_cast<RenderBox*>(m_num_editor.get())
                                             : static_cast<RenderBox*>(m_text_editor.get());
            if (ed) {
                PointF cp{position.x - ed->offset().x, position.y - ed->offset().y};
                if (ed->hit_test_touch(result, cp)) {
                    result.add(this, position);
                    return true;
                }
            }
        }
        for (const auto& entry : m_bool_boxes) {
            if (!entry.box) continue;
            PointF cp{position.x - entry.box->offset().x, position.y - entry.box->offset().y};
            if (cp.x >= -10.0f && cp.y >= -10.0f && cp.x <= entry.box->size().x + 10.0f && cp.y <= entry.box->size().y + 10.0f) {
                if (entry.box->hit_test_touch(result, cp)) {
                    result.add(this, position);
                    return true;
                }
            }
        }
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        auto now = std::chrono::steady_clock::now();
        float dt = (m_last_anim.time_since_epoch().count() == 0)
            ? 0.016f : std::clamp(std::chrono::duration<float>(now - m_last_anim).count(), 0.001f, 0.05f);
        m_last_anim = now;

        bool animating = false;
        if (m_hovered_row >= 0) {
            float target_y = static_cast<float>(m_hovered_row) * kRowH - m_scroll_y + 1.0f;
            if (!m_hover_init) { m_hover_y = target_y; m_hover_init = true; }
            else {
                m_hover_y += (target_y - m_hover_y) * (1.0f - std::exp(-dt * 24.0f));
                if (std::abs(target_y - m_hover_y) > 0.35f) animating = true;
                else m_hover_y = target_y;
            }
        } else {
            m_hover_init = false;
        }
        bool copy_flash = (m_copied_row >= 0 && now < m_copied_until);
        if (copy_flash) animating = true;
        if (animating) mark_needs_paint();

        const Theme& t = ThemeContext::current();
        const auto resolved_style = InspectorGridStyleResolver::resolve(t);
        const bool is_dark = t.is_dark;
        const Color accent = resolved_style.accent;
        const Color bg = is_dark ? Color{1, 1, 1, 0.025f} : Color{1, 1, 1, 0.78f};
        const Color bd = is_dark ? Color{1, 1, 1, 0.09f}  : Color{0, 0, 0, 0.10f};
        const Color line = is_dark ? Color{1, 1, 1, 0.055f} : Color{0, 0, 0, 0.06f};

        context.draw_rect(bounds(), bg, 6.0f, 0.0f);
        context.push_clip(bounds(), 6.0f);

        const float split_x = std::round(m_size.x * m_key_ratio);
        const float sb_pad = (max_scroll() > 0.5f) ? 10.0f : 0.0f;

        if (m_hovered_row >= 0 && m_hover_init) {
            context.draw_rect(Rect::from_xywh(1.0f, m_hover_y, m_size.x - 2.0f - sb_pad, kRowH),
                              is_dark ? Color{1, 1, 1, 0.045f} : Color{0, 0, 0, 0.035f}, 0.0f, 0.0f);
        }

        const size_t first_vis = static_cast<size_t>(std::max(0.0f, std::floor(m_scroll_y / kRowH)));
        const size_t last_vis = std::min(m_rows.size(), first_vis + static_cast<size_t>(std::ceil(m_size.y / kRowH)) + 2u);

        for (size_t i = first_vis; i < last_vis; ++i) {
            const auto& r = m_rows[i];
            const float ry = 1.0f + static_cast<float>(i) * kRowH - m_scroll_y;
            const bool mod = r.is_modified();
            const bool hov = (m_hovered_row == static_cast<int>(i));

            if (mod) {
                context.draw_rect(Rect::from_xywh(2.0f, ry + 6.0f, 2.5f, kRowH - 12.0f), accent, 1.25f, 0.0f);
            }

            // 类型角标
            Color tag_c = (r.type_tag == "bool") ? Color{0.10f, 0.72f, 0.45f, 1.0f}
                        : (r.type_tag == "hex")  ? Color{0.88f, 0.56f, 0.14f, 1.0f}
                        : (r.type_tag == "int" || r.type_tag == "float") ? accent
                                                 : resolved_style.text_secondary;
            Vec2 ts = TextMeasure::measure(r.type_tag, 9.5f, FontWeight::SemiBold);
            float tw = std::max(28.0f, ts.x + 8.0f);
            Rect tag_r = Rect::from_xywh(10.0f, ry + (kRowH - 16.0f) * 0.5f, tw, 16.0f);
            context.draw_rect(tag_r, Color{tag_c.r, tag_c.g, tag_c.b, is_dark ? 0.16f : 0.11f}, 3.0f, 0.0f);
            context.draw_text(r.type_tag, PointF{std::round(tag_r.min_x + (tw - ts.x) * 0.5f),
                                                 std::round(tag_r.min_y + (16.0f - ts.y) * 0.5f)},
                              9.5f, tag_c, FontWeight::SemiBold);

            // 左侧键名
            float kx = tag_r.max_x + 7.0f;
            float kw = std::max(0.0f, split_x - kx - 6.0f);
            if (kw > 6.0f) {
                Vec2 ks = TextMeasure::measure(r.key, 12.0f, mod ? FontWeight::SemiBold : FontWeight::Regular);
                context.push_clip(Rect::from_xywh(kx, ry, kw, kRowH), 0.0f);
                context.draw_text(r.key, PointF{kx, std::round(ry + (kRowH - ks.y) * 0.5f)},
                                  12.0f, mod ? resolved_style.text_primary : resolved_style.text_secondary,
                                  mod ? FontWeight::SemiBold : FontWeight::Regular);
                context.pop_clip();
            }

            // 右侧值区：复用原生 RenderCheckbox / RenderNumberBox / RenderTextBox
            if (m_editing_row == static_cast<int>(i)) {
                if (m_editing_is_num && m_num_editor) {
                    PaintContext::OffsetScope sc(context, m_num_editor->offset());
                    m_num_editor->paint(context);
                } else if (!m_editing_is_num && m_text_editor) {
                    Rect ed_r = Rect::from_xywh(split_x + 4.0f, ry + 2.0f, std::max(24.0f, m_size.x - split_x - 8.0f - sb_pad), kRowH - 4.0f);
                    context.draw_rect(ed_r, is_dark ? Color{1, 1, 1, 0.08f} : Color{1, 1, 1, 0.98f}, 3.0f, 0.0f);
                    context.draw_rect(ed_r, accent, 3.0f, 1.2f);
                    PaintContext::OffsetScope sc(context, m_text_editor->offset());
                    m_text_editor->paint(context);
                }
            } else {
                if (r.type_tag == "bool") {
                    if (auto* cb = find_bool_checkbox(i)) {
                        PaintContext::OffsetScope sc(context, cb->offset());
                        cb->paint(context);
                    }
                } else {
                    float right_actions = (hov ? (mod ? 44.0f : 24.0f) : 8.0f) + sb_pad;
                    float vx = split_x + 10.0f;
                    float vw = std::max(0.0f, m_size.x - vx - right_actions);
                    if (vw > 6.0f) {
                        Vec2 vs = TextMeasure::measure(r.value, 12.0f, mod ? FontWeight::SemiBold : FontWeight::Regular);
                        context.push_clip(Rect::from_xywh(vx, ry, vw, kRowH), 0.0f);
                        context.draw_text(r.value, PointF{vx, std::round(ry + (kRowH - vs.y) * 0.5f)},
                                          12.0f, mod ? accent : resolved_style.text_primary,
                                          mod ? FontWeight::SemiBold : FontWeight::Regular);
                        context.pop_clip();
                    }
                }

                if (hov) {
                    Rect copy_r = copy_btn_rect(ry);
                    bool is_copied = (copy_flash && m_copied_row == static_cast<int>(i));
                    const char* cg = is_copied ? "✓" : "⧉";
                    Vec2 cs = TextMeasure::measure(cg, 11.5f);
                    context.draw_text(cg, PointF{std::round(copy_r.min_x + (copy_r.width() - cs.x) * 0.5f),
                                                 std::round(copy_r.min_y + (copy_r.height() - cs.y) * 0.5f)},
                                      11.5f, is_copied ? Color{0.10f, 0.72f, 0.45f, 1.0f} : resolved_style.text_secondary);
                    if (mod) {
                        Rect rst_r = reset_btn_rect(ry);
                        Vec2 rs = TextMeasure::measure("↺", 11.5f);
                        context.draw_text("↺", PointF{std::round(rst_r.min_x + (rst_r.width() - rs.x) * 0.5f),
                                                      std::round(rst_r.min_y + (rst_r.height() - rs.y) * 0.5f)},
                                          11.5f, accent);
                    }
                }
            }

            context.draw_rect(Rect::from_xywh(0.0f, ry + kRowH - 1.0f, m_size.x, 1.0f), line, 0.0f, 0.0f);
        }

        if (m_resizing_col || m_divider_hovered) {
            context.draw_rect(Rect::from_xywh(split_x - 1.0f, 0.0f, 2.5f, m_size.y),
                              Color{accent.r, accent.g, accent.b, m_resizing_col ? 0.90f : 0.65f}, 1.0f, 0.0f);
        } else {
            context.draw_rect(Rect::from_xywh(split_x, 0.0f, 1.0f, m_size.y), line, 0.0f, 0.0f);
        }

        // 统一复用 WinUIScrollBar 控件 (绝不自绘滚动条)
        if (m_scrollbar.update_animations(m_viewport_hovered)) mark_needs_paint();
        float total_h = static_cast<float>(m_rows.size()) * kRowH + 2.0f;
        m_scrollbar.paint(context, bounds(), total_h, m_scroll_y, t);

        context.pop_clip();
        context.draw_rect(bounds(), bd, 6.0f, 1.0f);
    }

private:
    struct BoolCell {
        size_t row_idx{0};
        std::shared_ptr<RenderCheckbox> box;
    };

    static bool is_numeric_tag(const std::string& tag) noexcept {
        return tag == "int" || tag == "float";
    }

    [[nodiscard]] bool is_on_divider(float x, bool is_touch = false) const noexcept {
        return std::abs(x - std::round(m_size.x * m_key_ratio)) <= (is_touch ? 12.0f : 4.5f);
    }

    [[nodiscard]] float max_scroll() const noexcept {
        return std::max(0.0f, static_cast<float>(m_rows.size()) * kRowH + 2.0f - m_size.y);
    }

    [[nodiscard]] Rect scrollbar_thumb_rect() const noexcept {
        float ms = max_scroll();
        float total_h = static_cast<float>(m_rows.size()) * kRowH + 2.0f;
        float track_h = std::max(1.0f, m_size.y - 8.0f);
        float thumb_h = std::clamp(track_h * (m_size.y / std::max(1.0f, total_h)), 22.0f, track_h);
        float ratio = (ms > 0.0f) ? (m_scroll_y / ms) : 0.0f;
        float ty = 4.0f + ratio * (track_h - thumb_h);
        return Rect::from_xywh(m_size.x - 8.0f, ty, 5.0f, thumb_h);
    }

    [[nodiscard]] Rect copy_btn_rect(float ry) const noexcept {
        float pad = (max_scroll() > 0.5f) ? 10.0f : 0.0f;
        return Rect::from_xywh(m_size.x - 22.0f - pad, ry + 4.0f, 18.0f, kRowH - 8.0f);
    }
    [[nodiscard]] Rect reset_btn_rect(float ry) const noexcept {
        float pad = (max_scroll() > 0.5f) ? 10.0f : 0.0f;
        return Rect::from_xywh(m_size.x - 42.0f - pad, ry + 4.0f, 18.0f, kRowH - 8.0f);
    }
    [[nodiscard]] int row_at(float y) const noexcept {
        int idx = static_cast<int>(std::floor((y + m_scroll_y - 1.0f) / kRowH));
        return (idx >= 0 && idx < static_cast<int>(m_rows.size())) ? idx : -1;
    }

    RenderCheckbox* find_bool_checkbox(size_t row_idx) const noexcept {
        for (const auto& e : m_bool_boxes) {
            if (e.row_idx == row_idx) return e.box.get();
        }
        return nullptr;
    }

    void sync_bool_checkbox(size_t row_idx) {
        if (row_idx >= m_rows.size() || m_rows[row_idx].type_tag != "bool") return;
        if (auto* cb = find_bool_checkbox(row_idx)) {
            bool on = (m_rows[row_idx].value == "true" || m_rows[row_idx].value == "1");
            if (cb->checked() != on) cb->set_checked(on);
            cb->set_label(on ? "true" : "false");
        }
    }

    void rebuild_bool_checkboxes() {
        for (auto& e : m_bool_boxes) {
            if (e.box && e.box->parent() == this) remove_child(e.box.get());
        }
        m_bool_boxes.clear();

        for (size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].type_tag != "bool") continue;
            auto cb = make_render_node<RenderCheckbox>();
            bool on = (m_rows[i].value == "true" || m_rows[i].value == "1");
            Color unset{-1.0f, -1.0f, -1.0f, -1.0f};
            cb->set_properties(on, on ? "true" : "false", 16.0f, 3.0f, unset, unset, unset, unset, unset, 12.0f,
                               [this, i](bool checked) {
                                   if (i < m_rows.size()) {
                                       set_value(i, checked ? "true" : "false", true);
                                   }
                               });
            cb->set_disabled(m_rows[i].read_only);
            append_child(cb.get());
            m_bool_boxes.push_back(BoolCell{i, std::move(cb)});
        }
    }

    void clamp_scroll() noexcept {
        m_scroll_y = std::clamp(m_scroll_y, 0.0f, max_scroll());
    }

    void layout_children() {
        const float split_x = std::round(m_size.x * m_key_ratio);
        const float sb_pad = (max_scroll() > 0.5f) ? 10.0f : 0.0f;
        const float val_w = std::max(24.0f, m_size.x - split_x - 10.0f - sb_pad);

        // 1. 布局所有 bool 行原生 RenderCheckbox
        for (auto& e : m_bool_boxes) {
            if (!e.box) continue;
            float ry = 1.0f + static_cast<float>(e.row_idx) * kRowH - m_scroll_y;
            if (ry + kRowH >= 0.0f && ry <= m_size.y) {
                e.box->layout(BoxConstraints::loose(Vec2{std::max(60.0f, val_w - 44.0f), kRowH - 4.0f}), true);
                float cy = ry + std::max(0.0f, (kRowH - e.box->size().y) * 0.5f);
                e.box->set_offset(Vec2{split_x + 10.0f, std::round(cy)});
            } else {
                e.box->set_offset(Vec2{-10000.0f, -10000.0f});
            }
        }

        // 2. 布局共享 RenderTextBox 与 RenderNumberBox
        if (m_editing_row >= 0 && m_editing_row < static_cast<int>(m_rows.size())) {
            float ry = 1.0f + static_cast<float>(m_editing_row) * kRowH - m_scroll_y;
            if (m_editing_is_num && m_num_editor) {
                m_num_editor->layout(BoxConstraints::tight(Vec2{val_w, kRowH - 2.0f}), true);
                m_num_editor->set_offset(Vec2{split_x + 5.0f, ry + 1.0f});
                if (m_text_editor) {
                    m_text_editor->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
                    m_text_editor->set_offset(Vec2{-10000.0f, -10000.0f});
                }
            } else if (m_text_editor) {
                m_text_editor->layout(BoxConstraints::tight(Vec2{val_w, kRowH - 4.0f}), true);
                m_text_editor->set_offset(Vec2{split_x + 5.0f, ry + 2.0f});
                if (m_num_editor) {
                    m_num_editor->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
                    m_num_editor->set_offset(Vec2{-10000.0f, -10000.0f});
                }
            }
        } else {
            if (m_text_editor) {
                m_text_editor->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
                m_text_editor->set_offset(Vec2{-10000.0f, -10000.0f});
            }
            if (m_num_editor) {
                m_num_editor->layout(BoxConstraints::tight(Vec2{0.0f, 0.0f}), true);
                m_num_editor->set_offset(Vec2{-10000.0f, -10000.0f});
            }
        }
    }

    void begin_edit(int row) {
        if (row < 0 || row >= static_cast<int>(m_rows.size()) || m_rows[static_cast<size_t>(row)].read_only) return;
        const auto& r = m_rows[static_cast<size_t>(row)];
        if (r.type_tag == "bool") return;

        m_editing_row = row;
        m_editing_is_num = is_numeric_tag(r.type_tag);
        if (m_editing_is_num && m_num_editor) {
            bool is_int = (r.type_tag == "int");
            double v = std::strtod(r.value.c_str(), nullptr);
            float sb_pad = (max_scroll() > 0.5f) ? 10.0f : 0.0f;
            float val_w = std::max(60.0f, m_size.x - std::round(m_size.x * m_key_ratio) - 10.0f - sb_pad);
            m_num_editor->set_properties(
                v, -1e9, 1e9, is_int ? 1.0 : 0.05, is_int ? 10.0 : 0.5, is_int ? 0 : 2,
                true, NumberBoxSpinButtonPlacementMode::Inline, "", "", val_w, false,
                [this, row](double nv) {
                    if (row >= 0 && row < static_cast<int>(m_rows.size())) {
                        char buf[64];
                        if (m_rows[static_cast<size_t>(row)].type_tag == "int") {
                            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(std::llround(nv)));
                        } else {
                            std::snprintf(buf, sizeof(buf), "%.2f", nv);
                        }
                        set_value(static_cast<size_t>(row), buf, true);
                    }
                });
            layout_children();
            if (auto& in = m_num_editor->input_box()) {
                if (Window* w = Window::active_window()) w->focus_manager().request_focus(in.get());
                in->select_all();
            }
        } else if (m_text_editor) {
            m_text_editor->set_text(r.value);
            layout_children();
            if (Window* w = Window::active_window()) w->focus_manager().request_focus(m_text_editor.get());
            m_text_editor->select_all();
        }
        mark_needs_paint();
    }

    void commit_edit() {
        if (m_editing_row < 0 || m_editing_row >= static_cast<int>(m_rows.size())) return;
        size_t idx = static_cast<size_t>(m_editing_row);
        std::string val;
        if (m_editing_is_num && m_num_editor) {
            if (auto& in = m_num_editor->input_box()) {
                double nv = std::strtod(in->text().c_str(), nullptr);
                char buf[64];
                if (m_rows[idx].type_tag == "int") {
                    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(std::llround(nv)));
                } else {
                    std::snprintf(buf, sizeof(buf), "%.2f", nv);
                }
                val = buf;
            } else {
                val = m_num_editor->display_text();
            }
        } else {
            val = m_text_editor ? m_text_editor->text() : "";
        }
        m_editing_row = -1;
        m_editing_is_num = false;
        layout_children();
        set_value(idx, val, true);
    }

    void cancel_edit() {
        if (m_editing_row >= 0) {
            m_editing_row = -1;
            m_editing_is_num = false;
            layout_children();
            mark_needs_paint();
        }
    }

    void setup_behavior() {
        on_pointer_enter = [this](PointerEvent&) {
            m_viewport_hovered = true;
            mark_needs_paint();
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_scrollbar.handle_pointer_exit();
            m_hovered_row = -1;
            m_divider_hovered = false;
            mark_needs_paint();
        };
        on_pointer_move = [this](PointerEvent& ev) {
            float total_h = static_cast<float>(m_rows.size()) * kRowH + 2.0f;
            if (ev.type == PlatformEventType::PointerScroll) {
                m_scroll_y -= ev.delta.y * 28.0f;
                clamp_scroll();
                m_scrollbar.notify_activity();
                layout_children();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            bool sb_consumed = false;
            if (m_scrollbar.handle_pointer_move_direct(ev.local_position, bounds(), total_h, m_scroll_y, max_scroll(), sb_consumed)) {
                layout_children();
                mark_needs_paint();
            }
            if (sb_consumed) {
                ev.stop_propagation();
                return;
            }
            if (m_resizing_col) {
                m_key_ratio = std::clamp(ev.local_position.x / std::max(1.0f, m_size.x), 0.20f, 0.75f);
                layout_children();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (m_touch_dragging) {
                float dy = ev.local_position.y - m_touch_prev_y;
                m_touch_prev_y = ev.local_position.y;
                m_scroll_y -= dy;
                clamp_scroll();
                m_scrollbar.notify_activity();
                layout_children();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            bool on_div = is_on_divider(ev.local_position.x);
            if (on_div != m_divider_hovered) {
                m_divider_hovered = on_div;
                mark_needs_paint();
            }
            int r = row_at(ev.local_position.y);
            if (r != m_hovered_row) { m_hovered_row = r; mark_needs_paint(); }
        };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button != PointerButton::Primary) return;
            float total_h = static_cast<float>(m_rows.size()) * kRowH + 2.0f;
            if (m_scrollbar.handle_pointer_down_direct(ev.local_position, bounds(), total_h, m_scroll_y, max_scroll(), kRowH * 2.0f)) {
                layout_children();
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (is_on_divider(ev.local_position.x, ev.is_touch())) {
                m_resizing_col = true;
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (ev.device_kind == PointerDeviceKind::Touch) {
                m_touch_dragging = true;
                m_touch_prev_y = ev.local_position.y;
            }
            int r = row_at(ev.local_position.y);
            if (r < 0) { commit_edit(); return; }
            float ry = 1.0f + static_cast<float>(r) * kRowH - m_scroll_y;
            auto& row = m_rows[static_cast<size_t>(r)];

            Rect copy_hit = ev.is_touch() ? touch::expand_hit_rect(copy_btn_rect(ry), 32.0f) : copy_btn_rect(ry);
            Rect rst_hit = ev.is_touch() ? touch::expand_hit_rect(reset_btn_rect(ry), 32.0f) : reset_btn_rect(ry);
            if (copy_hit.contains(ev.local_position)) {
                Clipboard::set_text(row.value);
                m_copied_row = r;
                m_copied_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
                mark_needs_paint();
                ev.stop_propagation();
                return;
            }
            if (row.is_modified() && rst_hit.contains(ev.local_position)) {
                set_value(static_cast<size_t>(r), row.default_value, true);
                ev.stop_propagation();
                return;
            }
            if (ev.local_position.x >= m_size.x * m_key_ratio && !row.read_only) {
                if (row.type_tag != "bool") {
                    begin_edit(r);
                    ev.stop_propagation();
                }
            } else {
                commit_edit();
            }
        };
        on_pointer_up = [this](PointerEvent&) {
            if (m_scrollbar.handle_pointer_up()) mark_needs_paint();
            m_touch_dragging = false;
            if (m_resizing_col) { m_resizing_col = false; mark_needs_paint(); }
        };
        on_pointer_cancel = [this](PointerEvent&) {
            m_viewport_hovered = false;
            m_hovered_row = -1;
            m_divider_hovered = false;
            m_resizing_col = false;
            m_touch_dragging = false;
            m_scrollbar.handle_pointer_exit();
            mark_needs_paint();
        };
    }

    std::vector<InspectorRow> m_rows;
    float m_key_ratio{0.44f};
    Fn<void(size_t, const std::string&)> m_on_changed;
    std::shared_ptr<RenderTextBox> m_text_editor;
    std::shared_ptr<RenderNumberBox> m_num_editor;
    std::vector<BoolCell> m_bool_boxes;
    WinUIScrollBar m_scrollbar;

    float m_scroll_y{0.0f};
    bool m_viewport_hovered{false};
    int m_hovered_row{-1};
    int m_editing_row{-1};
    bool m_editing_is_num{false};
    int m_copied_row{-1};
    std::chrono::steady_clock::time_point m_copied_until{};
    float m_hover_y{0.0f};
    bool m_hover_init{false};
    bool m_divider_hovered{false};
    bool m_resizing_col{false};
    bool m_touch_dragging{false};
    float m_touch_prev_y{0.0f};
    std::chrono::steady_clock::time_point m_last_anim{};
};

struct InspectorGrid : BindableMixin<InspectorGrid, RenderInspectorGrid> {
    std::vector<InspectorRow> rows{};
    float key_column_ratio{0.44f};
    Fn<void(size_t, const std::string&)> on_value_changed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class InspectorGridWidget : public Widget {
public:
    explicit InspectorGridWidget(const InspectorGrid& desc) : m_desc(desc) {}
    explicit InspectorGridWidget(InspectorGrid&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderInspectorGrid>(m_desc.rows, m_desc.key_column_ratio, m_desc.on_value_changed);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderInspectorGrid>(node)) {
            n->set_on_value_changed(m_desc.on_value_changed);
        }
    }
private:
    InspectorGrid m_desc;
};

inline InspectorGrid::operator WidgetPtr() const& { return make_widget<InspectorGridWidget>(*this); }
inline InspectorGrid::operator WidgetPtr() && { return make_widget<InspectorGridWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui

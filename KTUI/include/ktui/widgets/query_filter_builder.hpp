#pragma once

#include "textbox.hpp"
#include "menu_flyout.hpp"
#include <ktui/app/window.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// 结构化查询字段定义 (如 field="status", options={"open","in_progress","closed"})
struct QueryFilterField {
    std::string field;
    std::string label;
    std::vector<std::string> options;
    Color pill_color{0.0f, 0.0f, 0.0f, 0.0f}; // a==0 时自动使用主题强调色
};

// 已选中的结构化查询胶囊 (Pill)
struct QueryFilterToken {
    std::string field;
    std::string value;
    bool operator==(const QueryFilterToken&) const noexcept = default;
};

// 结构化查询构建器渲染盒 (直接继承单一 RenderTextBox，零层层嵌套)
// 胶囊与模式切换按钮直接绘制在同一 RenderTextBox 内部，通过动态 padding 让光标/文本/选区紧跟胶囊之后
class RenderQueryFilterBuilder : public RenderTextBox {
public:
    RenderQueryFilterBuilder(std::vector<QueryFilterField> schema = {},
                             std::vector<QueryFilterToken> initial_tokens = {},
                             std::string placeholder = "输入 status: 或点击选择筛选条件...",
                             Fn<void(const std::vector<QueryFilterToken>&, const std::string&)> on_changed = nullptr)
        : m_schema(std::move(schema)),
          m_tokens(std::move(initial_tokens)),
          m_placeholder(std::move(placeholder)),
          m_user_on_changed(std::move(on_changed)) {
        set_preferred_width(460.0f);
        set_custom_height(36.0f);
        set_custom_corner_radius(6.0f);
        set_custom_font_size(12.5f);
        sync_placeholder();
        setup_query_behavior();
    }

    ~RenderQueryFilterBuilder() override {
        close_popup();
    }

    void set_schema(std::vector<QueryFilterField> s) {
        m_schema = std::move(s);
        mark_needs_paint();
    }
    void set_on_changed(Fn<void(const std::vector<QueryFilterToken>&, const std::string&)> cb) {
        m_user_on_changed = std::move(cb);
    }

    [[nodiscard]] const std::vector<QueryFilterToken>& tokens() const noexcept { return m_tokens; }
    [[nodiscard]] bool is_raw_mode() const noexcept { return m_raw_mode; }

    // 序列化为纯文本表达式 (如 "status:open priority:high")
    [[nodiscard]] std::string to_expression() const {
        const std::string& cur = RenderTextBox::text();
        if (m_raw_mode) return cur;
        std::string out;
        for (size_t i = 0; i < m_tokens.size(); ++i) {
            if (i > 0) out += ' ';
            out += m_tokens[i].field;
            out += ':';
            out += m_tokens[i].value;
        }
        if (!cur.empty()) {
            if (!out.empty()) out += ' ';
            out += cur;
        }
        return out;
    }

    // 从纯文本表达式解析为结构化胶囊列表
    void set_expression(const std::string& expr) {
        m_tokens.clear();
        std::string leftover;
        size_t i = 0;
        const size_t n = expr.size();
        while (i < n) {
            while (i < n && std::isspace(static_cast<unsigned char>(expr[i]))) ++i;
            if (i >= n) break;
            size_t start = i;
            while (i < n && !std::isspace(static_cast<unsigned char>(expr[i]))) ++i;
            std::string word = expr.substr(start, i - start);
            size_t colon = word.find(':');
            if (colon != std::string::npos && colon > 0 && colon + 1 < word.size()) {
                m_tokens.push_back(QueryFilterToken{word.substr(0, colon), word.substr(colon + 1)});
            } else {
                if (!leftover.empty()) leftover += ' ';
                leftover += word;
            }
        }
        set_text_silent(leftover);
        m_selected_pill = -1;
        sync_placeholder();
        mark_needs_layout();
        notify_changed();
        mark_needs_paint();
    }

    void toggle_raw_mode() {
        close_popup();
        if (!m_raw_mode) {
            std::string expr = to_expression();
            m_raw_mode = true;
            m_selected_pill = -1;
            set_text_silent(expr);
        } else {
            std::string expr = RenderTextBox::text();
            m_raw_mode = false;
            set_expression(expr);
            return;
        }
        sync_placeholder();
        mark_needs_layout();
        notify_changed();
        mark_needs_paint();
    }

    void add_token(const std::string& field, const std::string& value) {
        if (field.empty() || value.empty()) return;
        for (auto& tk : m_tokens) {
            if (tk.field == field && tk.value == value) {
                set_text_silent("");
                mark_needs_layout();
                mark_needs_paint();
                return;
            }
        }
        m_tokens.push_back(QueryFilterToken{field, value});
        set_text_silent("");
        m_selected_pill = -1;
        sync_placeholder();
        mark_needs_layout();
        notify_changed();
        mark_needs_paint();
    }

    void remove_token(size_t index) {
        if (index >= m_tokens.size()) return;
        m_tokens.erase(m_tokens.begin() + static_cast<ptrdiff_t>(index));
        if (m_selected_pill >= static_cast<int>(m_tokens.size())) {
            m_selected_pill = static_cast<int>(m_tokens.size()) - 1;
        }
        sync_placeholder();
        mark_needs_layout();
        notify_changed();
        mark_needs_paint();
    }

    void clear_all() {
        m_tokens.clear();
        set_text_silent("");
        m_selected_pill = -1;
        close_popup();
        sync_placeholder();
        mark_needs_layout();
        notify_changed();
        mark_needs_paint();
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : std::max(m_constraints.min_width, 460.0f);
        float h = m_constraints.has_bounded_height() ? std::min(m_constraints.max_height, 36.0f) : 36.0f;
        Vec2 box_sz = m_constraints.constrain(Vec2{w, std::max(32.0f, h)});

        m_mode_btn = Rect::from_xywh(std::max(4.0f, box_sz.x - 54.0f), (box_sz.y - 22.0f) * 0.5f, 48.0f, 22.0f);
        m_clear_btn = Rect::from_xywh(std::max(4.0f, m_mode_btn.min_x - 22.0f), (box_sz.y - 20.0f) * 0.5f, 20.0f, 20.0f);

        const bool has_content = !m_tokens.empty() || !RenderTextBox::text().empty();
        const float right_pad = has_content ? 82.0f : 58.0f;
        const float right_limit = std::max(40.0f, box_sz.x - right_pad);

        m_pill_hits.clear();
        float x = 8.0f;
        if (!m_raw_mode) {
            const float ph = 22.0f;
            const float py = std::round((box_sz.y - ph) * 0.5f);
            for (size_t i = 0; i < m_tokens.size(); ++i) {
                const auto& tk = m_tokens[i];
                std::string key_str = tk.field + ":";
                Vec2 ks = TextMeasure::measure(key_str, 11.5f, FontWeight::Medium);
                Vec2 vs = TextMeasure::measure(tk.value, 11.5f, FontWeight::SemiBold);
                float pw = ks.x + vs.x + 28.0f;
                if (x + pw > right_limit - 48.0f) break;
                Rect pr = Rect::from_xywh(x, py, pw, ph);
                Rect xr = Rect::from_xywh(pr.max_x - 18.0f, py + 2.0f, 16.0f, 18.0f);
                m_pill_hits.push_back(PillHit{i, pr, xr});
                x += pw + 6.0f;
            }
        }

        // 动态调整自身 TextBox 的左右 padding，让胶囊与按钮直接位于同一文本框内，零嵌套！
        set_custom_padding(EdgeInsets(4.0f, right_pad, 4.0f, x));
        RenderTextBox::perform_layout();
        m_size = box_sz;
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!bounds().contains(position)) return false;
        const bool has_content = !m_tokens.empty() || !RenderTextBox::text().empty();
        bool on_btn = m_mode_btn.contains(position) || (has_content && m_clear_btn.contains(position));
        if (!on_btn && !m_raw_mode) {
            for (const auto& ph : m_pill_hits) {
                if (ph.rect.contains(position)) { on_btn = true; break; }
            }
        }
        cursor_shape = on_btn ? CursorShape::PointingHand : CursorShape::IBeam;
        result.add(this, position);
        return true;
    }

    void paint(PaintContext& context) override {
        // 1. 直接由单一 RenderTextBox 绘制背景、描边、文本、光标、选区、IME 与底部 2px 聚焦动效条
        RenderTextBox::paint(context);

        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const Color accent = t.colors.primary;

        // 2. 在文本框左侧内边距区域直接绘制结构化胶囊
        if (!m_raw_mode) {
            for (const auto& ph : m_pill_hits) {
                const auto& tk = m_tokens[ph.index];
                const bool sel = (m_selected_pill == static_cast<int>(ph.index));
                Color pc = accent;
                for (const auto& f : m_schema) {
                    if (f.field == tk.field && f.pill_color.a > 0.01f) { pc = f.pill_color; break; }
                }
                std::string key_str = tk.field + ":";
                Vec2 ks = TextMeasure::measure(key_str, 11.5f, FontWeight::Medium);

                Color pill_bg = sel ? Color{pc.r, pc.g, pc.b, 0.30f} : Color{pc.r, pc.g, pc.b, is_dark ? 0.18f : 0.13f};
                Color pill_bd = sel ? pc : Color{pc.r, pc.g, pc.b, 0.42f};
                context.draw_rect(ph.rect, pill_bg, 11.0f, 0.0f);
                context.draw_rect(ph.rect, pill_bd, 11.0f, sel ? 1.5f : 1.0f);

                float ty = std::round(ph.rect.min_y + (ph.rect.height() - ks.y) * 0.5f);
                context.draw_text(key_str, PointF{ph.rect.min_x + 8.0f, ty}, 11.5f, t.colors.text_secondary, FontWeight::Medium);
                context.draw_text(tk.value, PointF{ph.rect.min_x + 8.0f + ks.x + 2.0f, ty}, 11.5f, t.colors.text_primary, FontWeight::SemiBold);

                Vec2 xs = TextMeasure::measure("×", 12.0f);
                context.draw_text("×", PointF{std::round(ph.close_rect.min_x + (ph.close_rect.width() - xs.x) * 0.5f),
                                              std::round(ph.close_rect.min_y + (ph.close_rect.height() - xs.y) * 0.5f - 0.5f)},
                                  12.0f, t.colors.text_secondary);
            }
        }

        // 3. 在文本框右侧内边距区域直接绘制清空按钮与模式切换按钮
        const bool has_content = !m_tokens.empty() || !RenderTextBox::text().empty();
        if (has_content) {
            if (m_hover_clear) {
                context.draw_rect(m_clear_btn, is_dark ? Color{1, 1, 1, 0.08f} : Color{0, 0, 0, 0.06f}, 4.0f, 0.0f);
            }
            Vec2 xs = TextMeasure::measure("×", 13.0f);
            context.draw_text("×", PointF{std::round(m_clear_btn.min_x + (m_clear_btn.width() - xs.x) * 0.5f),
                                          std::round(m_clear_btn.min_y + (m_clear_btn.height() - xs.y) * 0.5f - 0.5f)},
                              13.0f, t.colors.text_secondary);
        }

        Color mbg = m_raw_mode
            ? Color{accent.r, accent.g, accent.b, 0.20f}
            : (m_hover_mode ? (is_dark ? Color{1, 1, 1, 0.09f} : Color{0, 0, 0, 0.07f})
                            : (is_dark ? Color{1, 1, 1, 0.05f} : Color{0, 0, 0, 0.04f}));
        context.draw_rect(m_mode_btn, mbg, 4.0f, 0.0f);
        std::string mlabel = m_raw_mode ? "胶囊" : "</>";
        Vec2 ms = TextMeasure::measure(mlabel, 11.0f, FontWeight::Medium);
        context.draw_text(mlabel,
                          PointF{std::round(m_mode_btn.min_x + (m_mode_btn.width() - ms.x) * 0.5f),
                                 std::round(m_mode_btn.min_y + (m_mode_btn.height() - ms.y) * 0.5f)},
                          11.0f, m_raw_mode ? accent : t.colors.text_secondary, FontWeight::Medium);
    }

private:
    struct PillHit { size_t index; Rect rect; Rect close_rect; };

    void set_text_silent(const std::string& s) {
        m_syncing = true;
        RenderTextBox::set_text(s);
        set_cursor_index(s.size());
        m_syncing = false;
    }

    void sync_placeholder() {
        set_placeholder((m_raw_mode || m_tokens.empty()) ? m_placeholder : "继续添加筛选...");
    }

    void notify_changed() {
        if (m_user_on_changed) m_user_on_changed(m_tokens, to_expression());
    }

    void close_popup() {
        if (m_popup_id != 0) {
            if (Window* w = Window::active_window()) w->hide_overlay(m_popup_id);
            m_popup_id = 0;
        }
    }

    void show_suggestions_popup() {
        if (m_raw_mode || m_schema.empty()) { close_popup(); return; }
        Window* win = Window::active_window();
        if (!win) return;

        close_popup();
        const std::string& cur = RenderTextBox::text();
        std::vector<MenuFlyoutItem> items;
        const size_t colon = cur.find(':');
        if (colon != std::string::npos) {
            std::string fld = cur.substr(0, colon);
            std::string val_prefix = cur.substr(colon + 1);
            for (const auto& f : m_schema) {
                if (f.field != fld) continue;
                for (const auto& opt : f.options) {
                    if (val_prefix.empty() || opt.find(val_prefix) != std::string::npos) {
                        items.push_back(MenuFlyoutItem{
                            .label = fld + ":" + opt,
                            .shortcut = f.label,
                            .on_click = [this, fld, opt]() {
                                m_popup_id = 0;
                                add_token(fld, opt);
                            }
                        });
                    }
                }
                break;
            }
        } else {
            for (const auto& f : m_schema) {
                if (cur.empty() || f.field.find(cur) != std::string::npos || f.label.find(cur) != std::string::npos) {
                    std::string fld = f.field;
                    items.push_back(MenuFlyoutItem{
                        .label = fld + ":",
                        .shortcut = f.label,
                        .on_click = [this, fld]() {
                            m_popup_id = 0;
                            set_text_silent(fld + ":");
                            show_suggestions_popup();
                        }
                    });
                }
            }
        }

        if (items.empty()) return;
        Vec2 g = global_offset();
        float pop_x = g.x + (m_pill_hits.empty() ? 8.0f : (m_pill_hits.back().rect.max_x + 6.0f));
        Vec2 pop_pos{pop_x, g.y + win->titlebar_height() + m_size.y + 4.0f};
        m_popup_id = MenuFlyout::show_at(std::move(items), pop_pos, [this]() { m_popup_id = 0; });
    }

    void setup_query_behavior() {
        set_on_change([this](const std::string& val) {
            if (m_syncing) return;
            m_selected_pill = -1;
            if (m_raw_mode) {
                notify_changed();
                return;
            }
            if (!val.empty() && val.back() == ' ') {
                std::string trimmed = val.substr(0, val.size() - 1);
                size_t colon = trimmed.find(':');
                if (colon != std::string::npos && colon > 0 && colon + 1 < trimmed.size()) {
                    add_token(trimmed.substr(0, colon), trimmed.substr(colon + 1));
                    close_popup();
                    return;
                }
            }
            if (val.find(':') != std::string::npos) show_suggestions_popup();
            else close_popup();
            notify_changed();
        });

        set_key_intercept([this](KeyEvent& ev) -> bool {
            if (ev.type != PlatformEventType::KeyDown) return false;
            const std::string& cur = RenderTextBox::text();

            if (ev.key_code == 0x08) { // Backspace
                if (!m_raw_mode && m_selected_pill >= 0) {
                    remove_token(static_cast<size_t>(m_selected_pill));
                    ev.stop_propagation();
                    return true;
                }
                if (!m_raw_mode && cur.empty() && !m_tokens.empty()) {
                    remove_token(m_tokens.size() - 1);
                    ev.stop_propagation();
                    return true;
                }
            } else if (ev.key_code == 0x25 && !m_raw_mode && cursor_index() == 0 && !has_selection()) {
                if (!m_tokens.empty()) {
                    if (m_selected_pill < 0) m_selected_pill = static_cast<int>(m_tokens.size()) - 1;
                    else if (m_selected_pill > 0) --m_selected_pill;
                    mark_needs_paint();
                    ev.stop_propagation();
                    return true;
                }
            } else if (ev.key_code == 0x27 && !m_raw_mode && m_selected_pill >= 0) {
                if (m_selected_pill + 1 < static_cast<int>(m_tokens.size())) ++m_selected_pill;
                else m_selected_pill = -1;
                mark_needs_paint();
                ev.stop_propagation();
                return true;
            } else if (ev.key_code == 0x0D) { // Enter
                if (!m_raw_mode) {
                    size_t colon = cur.find(':');
                    if (colon != std::string::npos && colon > 0 && colon + 1 < cur.size()) {
                        add_token(cur.substr(0, colon), cur.substr(colon + 1));
                        close_popup();
                        ev.stop_propagation();
                        return true;
                    }
                } else {
                    toggle_raw_mode();
                    ev.stop_propagation();
                    return true;
                }
            } else if (ev.key_code == 0x1B) { // Esc
                if (m_popup_id != 0 || m_selected_pill >= 0) {
                    close_popup();
                    m_selected_pill = -1;
                    mark_needs_paint();
                    ev.stop_propagation();
                    return true;
                }
            }
            return false;
        });

        auto prev_down = on_pointer_down;
        on_pointer_down = [this, prev_down](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                PointF p = ev.local_position;
                const bool has_content = !m_tokens.empty() || !RenderTextBox::text().empty();
                if (m_mode_btn.contains(p)) {
                    toggle_raw_mode();
                    ev.stop_propagation();
                    return;
                }
                if (has_content && m_clear_btn.contains(p)) {
                    clear_all();
                    ev.stop_propagation();
                    return;
                }
                if (!m_raw_mode) {
                    for (const auto& ph : m_pill_hits) {
                        if (ph.close_rect.contains(p)) {
                            remove_token(ph.index);
                            ev.stop_propagation();
                            return;
                        }
                        if (ph.rect.contains(p)) {
                            m_selected_pill = static_cast<int>(ph.index);
                            mark_needs_paint();
                            ev.stop_propagation();
                            return;
                        }
                    }
                }
                m_selected_pill = -1;
                if (prev_down) prev_down(ev);
                if (!m_raw_mode) show_suggestions_popup();
                return;
            }
            if (prev_down) prev_down(ev);
        };

        auto prev_move = on_pointer_move;
        on_pointer_move = [this, prev_move](PointerEvent& ev) {
            const bool has_content = !m_tokens.empty() || !RenderTextBox::text().empty();
            bool hm = m_mode_btn.contains(ev.local_position);
            bool hc = has_content && m_clear_btn.contains(ev.local_position);
            if (hm != m_hover_mode || hc != m_hover_clear) {
                m_hover_mode = hm;
                m_hover_clear = hc;
                mark_needs_paint();
            }
            if (prev_move) prev_move(ev);
        };

        auto prev_exit = on_pointer_exit;
        on_pointer_exit = [this, prev_exit](PointerEvent& ev) {
            if (m_hover_mode || m_hover_clear) {
                m_hover_mode = false;
                m_hover_clear = false;
                mark_needs_paint();
            }
            if (prev_exit) prev_exit(ev);
        };

        auto prev_focus = on_focus;
        on_focus = [this, prev_focus](FocusEvent& ev) {
            if (prev_focus) prev_focus(ev);
            if (!ev.gained_focus) m_selected_pill = -1;
        };
    }

    std::vector<QueryFilterField> m_schema;
    std::vector<QueryFilterToken> m_tokens;
    std::string m_placeholder;
    Fn<void(const std::vector<QueryFilterToken>&, const std::string&)> m_user_on_changed;

    bool m_raw_mode{false};
    bool m_syncing{false};
    bool m_hover_mode{false};
    bool m_hover_clear{false};
    int m_selected_pill{-1};
    uint64_t m_popup_id{0};
    Rect m_mode_btn{};
    Rect m_clear_btn{};
    std::vector<PillHit> m_pill_hits;
};

struct QueryFilterBuilder : BindableMixin<QueryFilterBuilder, RenderQueryFilterBuilder> {
    std::vector<QueryFilterField> schema{};
    std::vector<QueryFilterToken> initial_tokens{};
    std::string placeholder{"输入 status: 或点击选择筛选条件..."};
    Fn<void(const std::vector<QueryFilterToken>&, const std::string&)> on_changed{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class QueryFilterBuilderWidget : public Widget {
public:
    explicit QueryFilterBuilderWidget(const QueryFilterBuilder& desc) : m_desc(desc) {}
    explicit QueryFilterBuilderWidget(QueryFilterBuilder&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderQueryFilterBuilder>(
            m_desc.schema, m_desc.initial_tokens, m_desc.placeholder, m_desc.on_changed);
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderQueryFilterBuilder>(node)) {
            n->set_schema(m_desc.schema);
            n->set_on_changed(m_desc.on_changed);
        }
    }
private:
    QueryFilterBuilder m_desc;
};

inline QueryFilterBuilder::operator WidgetPtr() const& { return make_widget<QueryFilterBuilderWidget>(*this); }
inline QueryFilterBuilder::operator WidgetPtr() && { return make_widget<QueryFilterBuilderWidget>(std::move(*this)); }

} // namespace ktui

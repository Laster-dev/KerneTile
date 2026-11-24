#pragma once

#include "common/fluent_chrome.hpp"
#include "common/fluent_icons.hpp"
#include "common/text_edit_core.hpp"
#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/svg_engine.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <string>

namespace ktui {

// WinUI 3 PasswordRevealMode
enum class PasswordRevealMode : uint8_t {
    Peek = 0,    // 默认：输入非空时右侧显示眼睛按钮，按住可临时预览明文，松开恢复掩码
    Hidden = 1,  // 始终隐藏，不显示眼睛按钮
    Visible = 2  // 始终以明文显示
};

// WinUI 3 PasswordBox 渲染盒 (支持按住窥视、安全禁复制、中心展开 2px 焦点底线、完整键盘/触控交互)
class RenderPasswordBox : public RenderBox, public ITextInputClient {
public:
    RenderPasswordBox() noexcept {
        is_focusable = true;
        cursor_shape = CursorShape::IBeam;
        setup_behavior();
    }

    ~RenderPasswordBox() override = default;

    void set_properties(const std::string& password,
                        const std::string& placeholder,
                        const std::string& header,
                        PasswordRevealMode reveal_mode,
                        size_t max_length,
                        float width,
                        bool disabled,
                        std::function<void(const std::string&)> on_change,
                        std::function<void(const std::string&)> on_submit) {
        if (m_password != password) {
            m_password = password;
            m_edit.clamp_caret(m_password);
            m_edit.clear_selection();
            mark_needs_paint();
        }
        if (m_placeholder != placeholder) { m_placeholder = placeholder; mark_needs_paint(); }
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (m_reveal_mode != reveal_mode) { m_reveal_mode = reveal_mode; mark_needs_paint(); }
        m_max_length = max_length;
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            is_focusable = !disabled;
            cursor_shape = disabled ? CursorShape::Arrow : CursorShape::IBeam;
            if (disabled) { m_focused = false; m_peeking = false; }
            mark_needs_paint();
        }
        m_on_change = std::move(on_change);
        m_on_submit = std::move(on_submit);
    }

    void set_password_value(const std::string& v) {
        if (m_password != v) {
            m_password = v;
            m_edit.clamp_caret(m_password);
            m_edit.clear_selection();
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& password() const noexcept { return m_password; }
    [[nodiscard]] bool is_revealing() const noexcept {
        return m_reveal_mode == PasswordRevealMode::Visible ||
               (m_reveal_mode == PasswordRevealMode::Peek && m_peeking);
    }

    // ==================== ITextInputClient ====================
    [[nodiscard]] bool accepts_text_input() const noexcept override { return !m_disabled; }

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        Vec2 g = global_offset();
        Rect box = box_rect();
        std::string disp = display_string();
        size_t disp_caret = display_byte_index(m_edit.caret());
        float cx = box.min_x + 11.0f + TextMeasure::measure(disp.substr(0, disp_caret), 13.5f).x;
        return Rect::from_xywh(g.x + cx, g.y + box.min_y + 6.0f, 2.0f, 20.0f);
    }

    void on_ime_preedit(const std::string&, size_t) override {}

    void on_ime_commit(const std::string& utf8_text) override {
        if (!m_disabled && m_edit.insert_text(m_password, utf8_text, m_max_length)) {
            notify_changed();
        }
    }

    bool tick_cursor_blink() override {
        if (m_edit.tick_blink(m_focused, m_disabled)) {
            mark_needs_paint();
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept override {
        return m_edit.time_until_next_blink_ms(m_focused, m_disabled);
    }

    void perform_layout() override {
        float w = (m_width > 0.0f) ? m_width : 240.0f;
        float h = (m_header.empty() ? 0.0f : 22.0f) + 32.0f;
        set_size(m_constraints.constrain(Vec2{w, h}));
    }

    void paint(PaintContext& ctx) override {
        if (FluentInputChrome::step_focus_anim(m_focus_anim, m_focused && !m_disabled)) {
            mark_needs_paint();
        }
        const Theme& theme = ThemeContext::current();
        Rect box = box_rect();
        FluentInputChrome::paint_box(ctx, theme, box, m_header, m_disabled, m_hovered, m_focused, m_focus_anim);

        Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f}
                                       : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
        Color ph_col   = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.50f} : Color{0.0f, 0.0f, 0.0f, 0.50f};
        Color accent   = theme.colors.primary;

        // 4. 文本或掩码圆点绘制区
        const bool show_eye = has_reveal_button();
        const float right_reserve = show_eye ? 34.0f : 10.0f;
        Rect text_clip = Rect::from_xywh(box.min_x + 10.0f, box.min_y + 2.0f,
                                         std::max(0.0f, box.width() - 10.0f - right_reserve),
                                         box.height() - 4.0f);
        ctx.push_clip(text_clip, 0.0f);

        std::string disp = display_string();
        if (disp.empty()) {
            if (!m_placeholder.empty()) {
                Vec2 ps = TextMeasure::measure(m_placeholder, 13.5f);
                ctx.draw_text(m_placeholder,
                              PointF{std::round(text_clip.min_x), std::round(box.min_y + (box.height() - ps.y) * 0.5f)},
                              13.5f, ph_col);
            }
        } else {
            Vec2 ts = TextMeasure::measure(disp, 13.5f);
            float ty = std::round(box.min_y + (box.height() - ts.y) * 0.5f);
            float scroll_x = 0.0f;
            float caret_px = TextMeasure::measure(disp.substr(0, display_byte_index(m_edit.caret())), 13.5f).x;
            if (caret_px > text_clip.width() - 4.0f) {
                scroll_x = caret_px - (text_clip.width() - 4.0f);
            }
            if (m_edit.select_all() && m_focused) {
                Color sel_bg = accent;
                sel_bg.a = theme.is_dark ? 0.35f : 0.25f;
                ctx.draw_rect(Rect::from_xywh(text_clip.min_x - scroll_x, box.min_y + 6.0f,
                                             ts.x + 2.0f, box.height() - 12.0f), sel_bg, 2.0f, 0.0f);
            }
            ctx.draw_text(disp, PointF{std::round(text_clip.min_x - scroll_x), ty}, 13.5f, text_col);

            if (m_focused && m_edit.caret_visible() && !m_disabled) {
                float cx = std::round(text_clip.min_x + caret_px - scroll_x);
                ctx.draw_rect(Rect::from_xywh(cx, box.min_y + 7.0f, 1.5f, box.height() - 14.0f),
                              text_col, 0.5f, 0.0f);
            }
        }
        if (disp.empty() && m_focused && m_edit.caret_visible() && !m_disabled) {
            ctx.draw_rect(Rect::from_xywh(std::round(text_clip.min_x), box.min_y + 7.0f, 1.5f, box.height() - 14.0f),
                          text_col, 0.5f, 0.0f);
        }
        ctx.pop_clip();

        // 5. 右侧眼睛按住窥视按钮 (复用 fluent_icons::kEye)
        if (show_eye) {
            Rect er = eye_button_rect();
            if (m_peeking) {
                Color bg_p = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f} : Color{0.0f, 0.0f, 0.0f, 0.06f};
                ctx.draw_rect(er, bg_p, 4.0f, 0.0f);
            } else if (m_eye_hovered) {
                Color bg_h = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
                ctx.draw_rect(er, bg_h, 4.0f, 0.0f);
            }
            Color ic = m_peeking ? accent : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f} : Color{0.0f, 0.0f, 0.0f, 0.68f});
            fluent_icons::draw_centered(ctx, fluent_icons::kEye, er, 15.0f, ic);
        }
    }

private:
    [[nodiscard]] Rect box_rect() const noexcept {
        float y = m_header.empty() ? 0.0f : 22.0f;
        return Rect::from_xywh(0.0f, y, size().x, std::max(28.0f, size().y - y));
    }

    [[nodiscard]] Rect eye_button_rect() const noexcept {
        Rect b = box_rect();
        return Rect::from_xywh(b.max_x - 30.0f, b.min_y + 3.0f, 26.0f, b.height() - 6.0f);
    }

    [[nodiscard]] bool has_reveal_button() const noexcept {
        return !m_disabled && m_reveal_mode == PasswordRevealMode::Peek && !m_password.empty();
    }

    [[nodiscard]] static size_t prev_utf8(const std::string& s, size_t idx) noexcept {
        if (idx == 0) return 0;
        --idx;
        while (idx > 0 && (static_cast<unsigned char>(s[idx]) & 0xC0) == 0x80) --idx;
        return idx;
    }

    [[nodiscard]] static size_t next_utf8(const std::string& s, size_t idx) noexcept {
        if (idx >= s.size()) return s.size();
        ++idx;
        while (idx < s.size() && (static_cast<unsigned char>(s[idx]) & 0xC0) == 0x80) ++idx;
        return idx;
    }

    [[nodiscard]] static size_t count_codepoints(const std::string& s, size_t end_byte) noexcept {
        size_t n = 0;
        size_t lim = std::min(end_byte, s.size());
        for (size_t i = 0; i < lim; ++i) {
            if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) ++n;
        }
        return n;
    }

    [[nodiscard]] std::string display_string() const {
        if (is_revealing()) return m_password;
        size_t cp = count_codepoints(m_password, m_password.size());
        std::string out;
        out.reserve(cp * 3);
        for (size_t i = 0; i < cp; ++i) out += "\xE2\x80\xA2"; // UTF-8 '•'
        return out;
    }

    [[nodiscard]] size_t display_byte_index(size_t raw_byte) const noexcept {
        if (is_revealing()) return std::min(raw_byte, m_password.size());
        return count_codepoints(m_password, raw_byte) * 3;
    }

    void notify_changed() {
        m_edit.reset_blink();
        mark_needs_paint();
        if (m_on_change) m_on_change(m_password);
    }

    void setup_behavior() {
        on_focus = [this](FocusEvent& e) {
            m_focused = e.gained_focus && !m_disabled;
            if (m_focused) {
                m_edit.set_caret(m_password.size(), m_password);
            } else {
                m_edit.clear_selection();
                m_peeking = false;
            }
            mark_needs_paint();
        };

        on_pointer_enter = [this](PointerEvent&) {
            if (!m_disabled) { m_hovered = true; mark_needs_paint(); }
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            m_eye_hovered = false;
            if (m_peeking) m_peeking = false;
            mark_needs_paint();
        };

        on_pointer_move = [this](PointerEvent& e) {
            if (m_disabled) return;
            bool eh = has_reveal_button() && eye_button_rect().contains(e.local_position);
            if (eh != m_eye_hovered) {
                m_eye_hovered = eh;
                cursor_shape = eh ? CursorShape::PointingHand : CursorShape::IBeam;
                mark_needs_paint();
            }
        };

        on_pointer_down = [this](PointerEvent& e) {
            if (m_disabled || e.button != PointerButton::Primary) return;
            if (has_reveal_button() && eye_button_rect().contains(e.local_position)) {
                m_peeking = true;
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            m_edit.set_caret(m_password.size(), m_password);
            mark_needs_paint();
            e.stop_propagation();
        };

        on_pointer_up = [this](PointerEvent& e) {
            if (m_peeking) {
                m_peeking = false;
                mark_needs_paint();
                e.stop_propagation();
            }
        };

        on_key = [this](KeyEvent& e) {
            if (m_disabled || e.type != PlatformEventType::KeyDown) return;
            // 安全策略：仅明文态允许 Ctrl+C / Ctrl+X 复制到剪贴板
            auto r = m_edit.handle_key(m_password, e, is_revealing(), m_max_length);
            if (r.submitted) {
                if (m_on_submit) m_on_submit(m_password);
                e.stop_propagation();
                return;
            }
            if (r.text_changed) notify_changed();
            else if (r.caret_moved) mark_needs_paint();
            if (r.handled) e.stop_propagation();
        };
    }

    std::string m_password;
    std::string m_placeholder;
    std::string m_header;
    PasswordRevealMode m_reveal_mode{PasswordRevealMode::Peek};
    size_t m_max_length{0};
    float m_width{240.0f};
    bool m_disabled{false};
    std::function<void(const std::string&)> m_on_change;
    std::function<void(const std::string&)> m_on_submit;

    SingleLineTextEditCore m_edit;
    bool m_focused{false};
    bool m_hovered{false};
    bool m_eye_hovered{false};
    bool m_peeking{false};
    float m_focus_anim{0.0f};
};

// C++20 指定初始化器声明式 WinUI 3 PasswordBox
struct PasswordBox {
    Bindable<std::string> password{""};
    std::string placeholder{"输入密码..."};
    std::string header{""};
    PasswordRevealMode reveal_mode{PasswordRevealMode::Peek};
    size_t max_length{0};
    float width{240.0f};
    bool disabled{false};
    std::function<void(const std::string&)> on_change{nullptr};
    std::function<void(const std::string&)> on_submit{nullptr};

    operator WidgetPtr() const;
};

class PasswordBoxWidget : public Widget {
public:
    explicit PasswordBoxWidget(const PasswordBox& desc) : m_desc(desc) {}
    ~PasswordBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = std::make_shared<RenderPasswordBox>();
        apply(node);
        bind(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto pb = std::dynamic_pointer_cast<RenderPasswordBox>(node)) {
            apply(pb);
            bind(pb);
        }
    }

private:
    void apply(const std::shared_ptr<RenderPasswordBox>& pb) const {
        auto desc_pass = m_desc.password;
        auto user_cb = m_desc.on_change;
        pb->set_properties(
            m_desc.password.get(), m_desc.placeholder, m_desc.header,
            m_desc.reveal_mode, m_desc.max_length, m_desc.width, m_desc.disabled,
            [desc_pass, user_cb](const std::string& v) mutable {
                desc_pass.set_bound(v);
                if (user_cb) user_cb(v);
            },
            m_desc.on_submit);
    }

    void bind(const std::shared_ptr<RenderPasswordBox>& pb) {
        m_bindings.clear();
        m_bindings.attach(m_desc.password, pb, [](RenderPasswordBox& n, const std::string& v) {
            n.set_password_value(v);
        });
    }

    PasswordBox m_desc;
    BindingGroup m_bindings;
};

inline PasswordBox::operator WidgetPtr() const {
    return std::make_shared<PasswordBoxWidget>(*this);
}

} // namespace ktui

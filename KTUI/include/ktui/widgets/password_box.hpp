#pragma once

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
            m_caret = std::min(m_caret, m_password.size());
            m_select_all = false;
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
            m_caret = std::min(m_caret, m_password.size());
            m_select_all = false;
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
        size_t disp_caret = display_byte_index(m_caret);
        float cx = box.min_x + 11.0f + TextMeasure::measure(disp.substr(0, disp_caret), 13.5f).x;
        return Rect::from_xywh(g.x + cx, g.y + box.min_y + 6.0f, 2.0f, 20.0f);
    }

    void on_ime_preedit(const std::string&, size_t) override {}

    void on_ime_commit(const std::string& utf8_text) override {
        if (m_disabled || utf8_text.empty()) return;
        insert_text(utf8_text);
    }

    bool tick_cursor_blink() override {
        if (!m_focused || m_disabled) return false;
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_blink_time).count();
        if (ms >= 530) {
            m_caret_visible = !m_caret_visible;
            m_blink_time = now;
            mark_needs_paint();
            return true;
        }
        return false;
    }

    [[nodiscard]] uint32_t time_until_next_blink_ms() const noexcept override {
        if (!m_focused || m_disabled) return 1000;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_blink_time).count();
        return (elapsed >= 530) ? 16 : static_cast<uint32_t>(530 - elapsed);
    }

    void perform_layout() override {
        float w = (m_width > 0.0f) ? m_width : 240.0f;
        float h = (m_header.empty() ? 0.0f : 22.0f) + 32.0f;
        set_size(m_constraints.constrain(Vec2{w, h}));
    }

    void paint(PaintContext& ctx) override {
        update_focus_anim();
        const Theme& theme = ThemeContext::current();

        // 1. 可选标题 Header
        if (!m_header.empty()) {
            Color hc = theme.is_dark
                ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.92f}
                : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.88f};
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, hc);
        }

        Rect box = box_rect();
        Color bg, border, bottom_line, text_col, ph_col, accent;
        if (theme.is_dark) {
            bg = m_disabled ? Color{1.0f, 1.0f, 1.0f, 0.03f}
               : (m_focused ? Color{30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 0.92f}
                            : (m_hovered ? Color{1.0f, 1.0f, 1.0f, 0.075f} : Color{1.0f, 1.0f, 1.0f, 0.055f}));
            border = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.05f : 0.085f};
            bottom_line = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.06f : 0.48f};
            text_col = Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f};
            ph_col = Color{1.0f, 1.0f, 1.0f, 0.50f};
            accent = Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f};
        } else {
            bg = m_disabled ? Color{0.96f, 0.96f, 0.96f, 0.55f}
               : (m_focused ? Color{1.0f, 1.0f, 1.0f, 1.0f}
                            : (m_hovered ? Color{249.0f / 255.0f, 249.0f / 255.0f, 249.0f / 255.0f, 0.65f}
                                         : Color{1.0f, 1.0f, 1.0f, 0.78f}));
            border = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.05f : 0.08f};
            bottom_line = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.06f : 0.42f};
            text_col = Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
            ph_col = Color{0.0f, 0.0f, 0.0f, 0.50f};
            accent = Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f};
        }

        // 2. 输入框底板 + 边框 + 1px 底部抬升线
        ctx.draw_rect(box, bg, 4.0f, 0.0f);
        ctx.draw_rect(box, border, 4.0f, 1.0f);
        ctx.draw_rect(Rect::from_xywh(box.min_x + 1.0f, box.max_y - 1.0f, std::max(0.0f, box.width() - 2.0f), 1.0f),
                      bottom_line, 0.0f, 0.0f);

        // 3. 中心展开 2px WinUI 3 强调色聚焦底线
        if (m_focus_anim > 0.01f) {
            float full_w = std::max(0.0f, box.width() - 2.0f);
            float line_w = full_w * m_focus_anim;
            float lx = box.min_x + 1.0f + (full_w - line_w) * 0.5f;
            ctx.draw_rect(Rect::from_xywh(lx, box.max_y - 2.0f, line_w, 2.0f), accent, 1.0f, 0.0f);
        }

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
            float caret_px = TextMeasure::measure(disp.substr(0, display_byte_index(m_caret)), 13.5f).x;
            if (caret_px > text_clip.width() - 4.0f) {
                scroll_x = caret_px - (text_clip.width() - 4.0f);
            }
            if (m_select_all && m_focused) {
                Color sel_bg = accent;
                sel_bg.a = theme.is_dark ? 0.35f : 0.25f;
                ctx.draw_rect(Rect::from_xywh(text_clip.min_x - scroll_x, box.min_y + 6.0f,
                                             ts.x + 2.0f, box.height() - 12.0f), sel_bg, 2.0f, 0.0f);
            }
            ctx.draw_text(disp, PointF{std::round(text_clip.min_x - scroll_x), ty}, 13.5f, text_col);

            if (m_focused && m_caret_visible && !m_disabled) {
                float cx = std::round(text_clip.min_x + caret_px - scroll_x);
                ctx.draw_rect(Rect::from_xywh(cx, box.min_y + 7.0f, 1.5f, box.height() - 14.0f),
                              text_col, 0.5f, 0.0f);
            }
        }
        if (disp.empty() && m_focused && m_caret_visible && !m_disabled) {
            ctx.draw_rect(Rect::from_xywh(std::round(text_clip.min_x), box.min_y + 7.0f, 1.5f, box.height() - 14.0f),
                          text_col, 0.5f, 0.0f);
        }
        ctx.pop_clip();

        // 5. 右侧眼睛按住窥视按钮 (Fluent Eye SVG Icon)
        if (show_eye) {
            Rect er = eye_button_rect();
            if (m_peeking) {
                Color bg_p = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f} : Color{0.0f, 0.0f, 0.0f, 0.06f};
                ctx.draw_rect(er, bg_p, 4.0f, 0.0f);
            } else if (m_eye_hovered) {
                Color bg_h = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
                ctx.draw_rect(er, bg_h, 4.0f, 0.0f);
            }
            static constexpr const char* kEyeSvg =
                "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
                "<path fill='#FFFFFF' d='M12 5C7.24 5 3.17 7.91 1.5 12c1.67 4.09 5.74 7 10.5 7s8.83-2.91 10.5-7C20.83 7.91 16.76 5 12 5zm0 11.5a4.5 4.5 0 1 1 0-9 4.5 4.5 0 0 1 0 9zm0-7a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z'/>"
                "</svg>";
            Color ic = m_peeking ? accent : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f} : Color{0.0f, 0.0f, 0.0f, 0.68f});
            float isz = 15.0f;
            Rect ir = Rect::from_xywh(std::round(er.min_x + (er.width() - isz) * 0.5f),
                                      std::round(er.min_y + (er.height() - isz) * 0.5f), isz, isz);
            SvgEngine::draw_svg_xml(ctx, kEyeSvg, ir, ic);
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

    void reset_blink() noexcept {
        m_caret_visible = true;
        m_blink_time = std::chrono::steady_clock::now();
    }

    void notify_changed() {
        reset_blink();
        mark_needs_paint();
        if (m_on_change) m_on_change(m_password);
    }

    void insert_text(const std::string& raw) {
        std::string filtered;
        for (char c : raw) {
            if (static_cast<unsigned char>(c) >= 0x20 && c != '\r' && c != '\n') filtered.push_back(c);
        }
        if (filtered.empty()) return;
        if (m_select_all) {
            m_password.clear();
            m_caret = 0;
            m_select_all = false;
        }
        if (m_max_length > 0) {
            size_t cur_cp = count_codepoints(m_password, m_password.size());
            size_t add_cp = count_codepoints(filtered, filtered.size());
            if (cur_cp + add_cp > m_max_length) return;
        }
        m_password.insert(m_caret, filtered);
        m_caret += filtered.size();
        notify_changed();
    }

    void update_focus_anim() {
        auto now = std::chrono::steady_clock::now();
        if (m_anim_time.time_since_epoch().count() == 0) m_anim_time = now;
        float dt = std::clamp(std::chrono::duration<float>(now - m_anim_time).count(), 0.0f, 0.05f);
        m_anim_time = now;
        float target = (m_focused && !m_disabled) ? 1.0f : 0.0f;
        if (std::abs(m_focus_anim - target) > 0.005f) {
            m_focus_anim += (target - m_focus_anim) * std::min(1.0f, dt * 22.0f);
            if (std::abs(m_focus_anim - target) <= 0.005f) m_focus_anim = target;
            mark_needs_paint();
        }
    }

    void setup_behavior() {
        on_focus = [this](FocusEvent& e) {
            m_focused = e.gained_focus && !m_disabled;
            if (m_focused) {
                m_caret = m_password.size();
                reset_blink();
            } else {
                m_select_all = false;
                m_peeking = false;
            }
            m_anim_time = std::chrono::steady_clock::now();
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
            m_select_all = false;
            m_caret = m_password.size();
            reset_blink();
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
            if (e.modifiers.ctrl) {
                if (e.key_code == 'A' || e.key_code == 'a') {
                    m_select_all = !m_password.empty();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                }
                if (e.key_code == 'V' || e.key_code == 'v') {
                    insert_text(Clipboard::get_text());
                    e.stop_propagation();
                    return;
                }
                // 安全策略：掩码态禁止 Ctrl+C / Ctrl+X 复制密码
                if (e.key_code == 'C' || e.key_code == 'c' || e.key_code == 'X' || e.key_code == 'x') {
                    if (is_revealing() && !m_password.empty()) {
                        Clipboard::set_text(m_password);
                        if (e.key_code == 'X' || e.key_code == 'x') {
                            m_password.clear();
                            m_caret = 0;
                            m_select_all = false;
                            notify_changed();
                        }
                    }
                    e.stop_propagation();
                    return;
                }
            }

            switch (e.key_code) {
                case 8: // Backspace
                    if (m_select_all) {
                        m_password.clear();
                        m_caret = 0;
                        m_select_all = false;
                        notify_changed();
                    } else if (m_caret > 0) {
                        size_t p = prev_utf8(m_password, m_caret);
                        m_password.erase(p, m_caret - p);
                        m_caret = p;
                        notify_changed();
                    }
                    e.stop_propagation();
                    return;
                case 46: // Delete
                    if (m_select_all) {
                        m_password.clear();
                        m_caret = 0;
                        m_select_all = false;
                        notify_changed();
                    } else if (m_caret < m_password.size()) {
                        size_t n = next_utf8(m_password, m_caret);
                        m_password.erase(m_caret, n - m_caret);
                        notify_changed();
                    }
                    e.stop_propagation();
                    return;
                case 37: // Left
                    m_select_all = false;
                    m_caret = prev_utf8(m_password, m_caret);
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 39: // Right
                    m_select_all = false;
                    m_caret = next_utf8(m_password, m_caret);
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 36: // Home
                    m_select_all = false;
                    m_caret = 0;
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 35: // End
                    m_select_all = false;
                    m_caret = m_password.size();
                    reset_blink();
                    mark_needs_paint();
                    e.stop_propagation();
                    return;
                case 13: // Enter
                    if (m_on_submit) m_on_submit(m_password);
                    e.stop_propagation();
                    return;
                default:
                    if (!e.text.empty()) {
                        insert_text(e.text);
                        e.stop_propagation();
                    }
                    break;
            }
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

    size_t m_caret{0};
    bool m_select_all{false};
    bool m_focused{false};
    bool m_hovered{false};
    bool m_eye_hovered{false};
    bool m_peeking{false};
    bool m_caret_visible{true};
    float m_focus_anim{0.0f};
    std::chrono::steady_clock::time_point m_blink_time{std::chrono::steady_clock::now()};
    std::chrono::steady_clock::time_point m_anim_time{};
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

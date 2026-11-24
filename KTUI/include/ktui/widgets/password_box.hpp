#pragma once

#include "common/fluent_icons.hpp"
#include "textbox/textbox_logic.hpp"
#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>

#include <algorithm>
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

// WinUI 3 PasswordBox 渲染盒
// 直接组合复用真实 RenderTextBox 子节点，完整继承选区、撤销/重做、WinUI 3 文本选中样式与右键菜单，
// 并保持按住窥视眼睛按钮、安全禁复制与最大长度限制
class RenderPasswordBox : public RenderBox {
public:
    RenderPasswordBox() noexcept {
        m_input = make_render_node<RenderTextBox>();
        m_input->set_is_password(true);
        m_input->set_show_reveal_button(false); // 由 PasswordBox 叠加绘制 Fluent 按住窥视按钮
        append_child(m_input.get());
        setup_input_bridge();
        sync_input_properties();
    }

    ~RenderPasswordBox() override {
        if (m_input && m_input->parent() == this) {
            remove_child(m_input.get());
        }
    }

    void set_properties(const std::string& password,
                        const std::string& placeholder,
                        const std::string& header,
                        PasswordRevealMode reveal_mode,
                        size_t max_length,
                        float width,
                        bool disabled,
                        Fn<void(const std::string&)> on_change,
                        Fn<void(const std::string&)> on_submit) {
        m_max_length = max_length;
        std::string clamped = clamp_length(password);
        if (m_password != clamped) m_password = std::move(clamped);
        if (m_placeholder != placeholder) m_placeholder = placeholder;
        if (m_header != header) { m_header = header; mark_needs_layout(); }
        if (m_reveal_mode != reveal_mode) m_reveal_mode = reveal_mode;
        if (std::abs(m_width - width) > 0.001f) { m_width = width; mark_needs_layout(); }
        if (m_disabled != disabled) {
            m_disabled = disabled;
            if (disabled) m_peeking = false;
        }
        m_on_change = std::move(on_change);
        m_on_submit = std::move(on_submit);

        sync_input_properties();
        mark_needs_paint();
    }

    void set_password_value(const std::string& v) {
        std::string clamped = clamp_length(v);
        if (m_password != clamped) {
            m_password = std::move(clamped);
            if (m_input) m_input->set_text(m_password);
            update_input_padding();
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& password() const noexcept { return m_password; }
    [[nodiscard]] const std::shared_ptr<RenderTextBox>& input_box() const noexcept { return m_input; }
    void on_ime_commit(const std::string& utf8_text) {
        if (!m_disabled && m_input) m_input->on_ime_commit(utf8_text);
    }
    [[nodiscard]] bool is_revealing() const noexcept {
        return m_reveal_mode == PasswordRevealMode::Visible ||
               (m_reveal_mode == PasswordRevealMode::Peek && m_peeking);
    }

    void perform_layout() override {
        float w = (m_width > 0.0f) ? m_width : 240.0f;
        float header_h = m_header.empty() ? 0.0f : 22.0f;
        set_size(m_constraints.constrain(Vec2{w, header_h + 32.0f}));
        if (m_input) {
            float box_h = std::max(28.0f, size().y - header_h);
            m_input->layout(BoxConstraints::tight(Vec2{size().x, box_h}), false);
            m_input->set_offset(Vec2{0.0f, header_h});
        }
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();

        if (!m_header.empty()) {
            Color text_col = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, m_disabled ? 0.36f : 0.96f}
                                           : Color{0.0f, 0.0f, 0.0f, m_disabled ? 0.36f : 0.90f};
            ctx.draw_text(m_header, PointF{0.0f, 2.0f}, 13.0f, text_col);
        }

        if (m_input) {
            PaintContext::OffsetScope scope(ctx, m_input->offset());
            m_input->paint(ctx);
        }

        // 右侧眼睛按住窥视按钮 (复用 fluent_icons::kEye)
        if (has_reveal_button()) {
            Rect er = eye_button_rect();
            if (m_peeking) {
                Color bg_p = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.09f} : Color{0.0f, 0.0f, 0.0f, 0.06f};
                ctx.draw_rect(er, bg_p, 4.0f, 0.0f);
            } else if (m_eye_hovered) {
                Color bg_h = theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.04f};
                ctx.draw_rect(er, bg_h, 4.0f, 0.0f);
            }
            Color ic = m_peeking
                ? theme.colors.primary
                : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.78f} : Color{0.0f, 0.0f, 0.0f, 0.68f});
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

    [[nodiscard]] Rect input_local_eye_rect() const noexcept {
        Vec2 s = m_input ? m_input->size() : Vec2{size().x, 32.0f};
        return Rect::from_xywh(s.x - 30.0f, 3.0f, 26.0f, std::max(0.0f, s.y - 6.0f));
    }

    [[nodiscard]] bool has_reveal_button() const noexcept {
        return !m_disabled && m_reveal_mode == PasswordRevealMode::Peek && !m_password.empty();
    }

    [[nodiscard]] std::string clamp_length(const std::string& raw) const {
        if (m_max_length == 0) return raw;
        size_t count = 0;
        size_t i = 0;
        while (i < raw.size()) {
            if ((static_cast<unsigned char>(raw[i]) & 0xC0) != 0x80) {
                if (count >= m_max_length) return raw.substr(0, i);
                ++count;
            }
            ++i;
        }
        return raw;
    }

    void update_input_padding() {
        if (!m_input) return;
        float right_pad = has_reveal_button() ? 36.0f : 10.0f;
        m_input->apply_custom_overrides(
            std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
            32.0f, 4.0f, EdgeInsets{6.0f, right_pad, 6.0f, 10.0f}, 13.5f);
    }

    void sync_input_properties() {
        if (!m_input) return;
        m_input->set_properties(
            m_password, m_placeholder,
            32.0f, 4.0f,
            EdgeInsets{6.0f, has_reveal_button() ? 36.0f : 10.0f, 6.0f, 10.0f},
            Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1},
            Color{-1, -1, -1, -1}, Color{-1, -1, -1, -1}, 13.5f,
            [this](const std::string& updated) {
                std::string clamped = clamp_length(updated);
                if (clamped != updated) {
                    m_input->set_text(clamped);
                }
                m_password = std::move(clamped);
                update_input_padding();
                mark_needs_paint();
                if (m_on_change) m_on_change(m_password);
            },
            [this](const std::string& submitted) {
                if (m_on_submit) m_on_submit(submitted);
            });
        m_input->set_is_password(true);
        m_input->set_password_revealed(is_revealing());
        m_input->set_show_reveal_button(false);
        m_input->set_disabled(m_disabled);
    }

    void setup_input_bridge() {
        auto orig_move = m_input->on_pointer_move;
        auto orig_exit = m_input->on_pointer_exit;
        auto orig_down = m_input->on_pointer_down;
        auto orig_up = m_input->on_pointer_up;
        auto orig_focus = m_input->on_focus;

        m_input->on_pointer_move = [this, orig_move](PointerEvent& e) {
            if (!m_disabled) {
                bool eh = has_reveal_button() && input_local_eye_rect().contains(e.local_position);
                if (eh != m_eye_hovered) {
                    m_eye_hovered = eh;
                    m_input->cursor_shape = eh ? CursorShape::PointingHand : CursorShape::IBeam;
                    mark_needs_paint();
                }
            }
            if (orig_move) orig_move(e);
        };

        m_input->on_pointer_exit = [this, orig_exit](PointerEvent& e) {
            if (m_eye_hovered || m_peeking) {
                m_eye_hovered = false;
                m_peeking = false;
                m_input->set_password_revealed(is_revealing());
                mark_needs_paint();
            }
            if (orig_exit) orig_exit(e);
        };

        m_input->on_pointer_down = [this, orig_down](PointerEvent& e) {
            if (m_disabled) return;
            if (e.button == PointerButton::Primary && has_reveal_button() &&
                input_local_eye_rect().contains(e.local_position)) {
                m_peeking = true;
                m_input->set_password_revealed(true);
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (orig_down) orig_down(e);
        };

        m_input->on_pointer_up = [this, orig_up](PointerEvent& e) {
            if (m_peeking) {
                m_peeking = false;
                m_input->set_password_revealed(is_revealing());
                mark_needs_paint();
                e.stop_propagation();
                return;
            }
            if (orig_up) orig_up(e);
        };

        m_input->on_focus = [this, orig_focus](FocusEvent& e) {
            if (orig_focus) orig_focus(e);
            if (!e.gained_focus && m_peeking) {
                m_peeking = false;
                m_input->set_password_revealed(is_revealing());
            }
            mark_needs_paint();
        };
    }

    std::shared_ptr<RenderTextBox> m_input;
    std::string m_password;
    std::string m_placeholder;
    std::string m_header;
    PasswordRevealMode m_reveal_mode{PasswordRevealMode::Peek};
    size_t m_max_length{0};
    float m_width{240.0f};
    bool m_disabled{false};
    bool m_eye_hovered{false};
    bool m_peeking{false};
    Fn<void(const std::string&)> m_on_change;
    Fn<void(const std::string&)> m_on_submit;

public:
    void wrap_on_change(Fn<void(const std::string&)> extra) {
        auto prev = std::move(m_on_change);
        m_on_change = [prev = std::move(prev), extra = std::move(extra)](const std::string& v) {
            if (extra) extra(v);
            if (prev) prev(v);
        };
    }
};

// C++20 指定初始化器声明式 WinUI 3 PasswordBox
struct PasswordBox : BindableMixin<PasswordBox, RenderPasswordBox> {
    std::string password{""};
    std::string placeholder{"输入密码..."};
    std::string header{""};
    PasswordRevealMode reveal_mode{PasswordRevealMode::Peek};
    size_t max_length{0};
    float width{240.0f};
    bool disabled{false};
    Fn<void(const std::string&)> on_change{nullptr};
    Fn<void(const std::string&)> on_submit{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderPasswordBox>& node, Property<std::string>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_password_value(prop.get());
    std::weak_ptr<RenderPasswordBox> weak = node;
    bg.subscribe(prop, [weak, guard](const std::string& v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_password_value(v);
        }
    });
    Property<std::string>* p = &prop;
    node->wrap_on_change([p, guard](const std::string& v) {
        *guard = true;
        p->set(v);
        *guard = false;
    });
}

class PasswordBoxWidget : public Widget {
public:
    explicit PasswordBoxWidget(const PasswordBox& desc) : m_desc(desc) {}
    explicit PasswordBoxWidget(PasswordBox&& desc) noexcept : m_desc(std::move(desc)) {}
    ~PasswordBoxWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderPasswordBox>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto pb = std::static_pointer_cast<RenderPasswordBox>(node)) {
            apply(pb);
        }
    }

private:
    void apply(const std::shared_ptr<RenderPasswordBox>& pb) const {
        pb->set_properties(
            m_desc.password, m_desc.placeholder, m_desc.header,
            m_desc.reveal_mode, m_desc.max_length, m_desc.width, m_desc.disabled,
            m_desc.on_change, m_desc.on_submit);
    }

    PasswordBox m_desc;
};

inline PasswordBox::operator WidgetPtr() const& {
    return make_widget<PasswordBoxWidget>(*this);
}
inline PasswordBox::operator WidgetPtr() && {
    return make_widget<PasswordBoxWidget>(std::move(*this));
}

} // namespace ktui

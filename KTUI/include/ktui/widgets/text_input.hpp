#pragma once

#include "widget.hpp"
#include <ktui/pal/platform_types.hpp>
#include <ktui/binding/binding.hpp>
#include <ktui/event/text_input_client.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <optional>
#include <string>
#include <algorithm>

namespace ktui {

// UTF-8 码点边界安全工具
namespace utf8_util {
    inline size_t prev_char_pos(const std::string& s, size_t byte_pos) noexcept {
        if (byte_pos == 0 || s.empty()) return 0;
        size_t pos = std::min(byte_pos, s.size()) - 1;
        while (pos > 0 && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) {
            --pos;
        }
        return pos;
    }

    inline size_t next_char_pos(const std::string& s, size_t byte_pos) noexcept {
        if (byte_pos >= s.size()) return s.size();
        size_t pos = byte_pos + 1;
        while (pos < s.size() && (static_cast<uint8_t>(s[pos]) & 0xC0) == 0x80) {
            ++pos;
        }
        return pos;
    }

    inline float estimate_width(std::string_view s, float font_size) noexcept {
        float w = 0.0f;
        size_t i = 0;
        while (i < s.size()) {
            uint8_t b = static_cast<uint8_t>(s[i]);
            if (b < 0x80) {
                w += font_size * 0.52f;
                i += 1;
            } else {
                w += font_size * 1.0f; // CJK 全角估算
                if ((b & 0xE0) == 0xC0) i += 2;
                else if ((b & 0xF0) == 0xE0) i += 3;
                else if ((b & 0xF8) == 0xF0) i += 4;
                else i += 1;
            }
        }
        return w;
    }
}

// 桌面紧凑单行文本输入框渲染盒 (RenderTextInput)
class RenderTextInput : public RenderBox, public ITextInputClient {
public:
    RenderTextInput() noexcept {
        is_focusable = true;
        setup_behavior();
    }

    ~RenderTextInput() override = default;

    void set_properties(std::string text, std::string placeholder,
                        float h, float radius, EdgeInsets pad,
                        Color bg, Color border_normal, Color border_focus,
                        Color txt_col, Color ph_col, float font_sz,
                        std::function<void(const std::string&)> on_ch,
                        std::function<void(const std::string&)> on_sub) {
        if (m_text != text) {
            m_text = std::move(text);
            m_cursor_index = std::min(m_cursor_index, m_text.length());
        }
        m_placeholder = std::move(placeholder);
        m_height = h;
        m_corner_radius = radius;
        m_padding = pad;
        m_bg_color = bg;
        m_border_normal = border_normal;
        m_border_focus = border_focus;
        m_text_color = txt_col;
        m_placeholder_color = ph_col;
        m_font_size = font_sz;
        m_on_change = std::move(on_ch);
        m_on_submit = std::move(on_sub);

        mark_needs_layout();
    }

    void set_text(std::string text) {
        if (m_text != text) {
            m_text = std::move(text);
            m_cursor_index = std::min(m_cursor_index, m_text.length());
            mark_needs_paint();
        }
    }

    [[nodiscard]] const std::string& text() const noexcept { return m_text; }
    [[nodiscard]] size_t cursor_index() const noexcept { return m_cursor_index; }
    void set_cursor_index(size_t idx) noexcept {
        m_cursor_index = std::min(idx, m_text.length());
        mark_needs_paint();
    }

    [[nodiscard]] bool is_input_focused() const noexcept { return m_is_focused; }
    void set_input_focused(bool focused) noexcept {
        if (m_is_focused != focused) {
            m_is_focused = focused;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float text_w = static_cast<float>(std::max(m_text.length(), m_placeholder.length())) * m_font_size * 0.54f;
        float desired_w = text_w + m_padding.horizontal() + 20.0f;
        if (m_constraints.max_width < BoxConstraints::kInfinity) {
            desired_w = std::max(desired_w, m_constraints.max_width);
        }
        float desired_h = m_height;

        m_size = m_constraints.constrain(Vec2{desired_w, desired_h});
    }

    void paint(PaintContext& context) override {
        // 1. 绘制硬朗微圆角实心背景与 1px 细边框 (聚焦时高亮蓝色)
        Color border_col = m_is_focused ? m_border_focus : (is_hovered ? Color{0.38f, 0.38f, 0.44f, 1.0f} : m_border_normal);
        context.draw_rect(bounds(), m_bg_color, m_corner_radius, 0.0f);
        context.draw_rect(bounds(), border_col, m_corner_radius, 1.0f);

        // 2. 绘制内文（含 IME 预编辑组合态）或占位提示
        float text_x = m_padding.left;
        float text_y = std::max(0.0f, (m_size.y - m_font_size * 1.30f) * 0.5f);

        std::string display_text = m_text;
        float preedit_start_x = text_x + utf8_util::estimate_width(std::string_view(m_text).substr(0, m_cursor_index), m_font_size);
        float preedit_w = 0.0f;
        if (!m_preedit_text.empty()) {
            display_text.insert(m_cursor_index, m_preedit_text);
            preedit_w = utf8_util::estimate_width(m_preedit_text, m_font_size);
        }

        if (display_text.empty() && !m_placeholder.empty()) {
            context.draw_text(m_placeholder, PointF{text_x, text_y}, m_font_size, m_placeholder_color);
        } else if (!display_text.empty()) {
            context.draw_text(display_text, PointF{text_x, text_y}, m_font_size, m_text_color);
        }

        // 2.5 若存在 IME 预编辑文本，在其下方绘制强调下划线
        if (!m_preedit_text.empty() && preedit_w > 0.0f) {
            float underline_y = text_y + m_font_size * 1.15f;
            context.draw_rect(Rect::from_xywh(preedit_start_x, underline_y, preedit_w, 1.5f),
                              m_border_focus, 0.0f, 0.0f);
        }

        // 3. 聚焦时绘制紧凑光标竖线
        if (m_is_focused) {
            float cursor_x = preedit_start_x + preedit_w;
            float cursor_h = m_font_size * 1.15f;
            float cursor_y = std::max(0.0f, (m_size.y - cursor_h) * 0.5f);

            context.draw_rect(Rect::from_xywh(cursor_x, cursor_y, 1.5f, cursor_h),
                              m_text_color, 0.0f, 0.0f);
        }
    }

    // ==================== ITextInputClient 框架级 IME 协议实现 ====================
    [[nodiscard]] bool accepts_text_input() const noexcept override { return true; }

    [[nodiscard]] Rect get_ime_cursor_rect() const override {
        Vec2 abs = global_offset();
        float text_x = m_padding.left;
        float prefix_w = utf8_util::estimate_width(std::string_view(m_text).substr(0, m_cursor_index), m_font_size);
        float preedit_w = utf8_util::estimate_width(m_preedit_text, m_font_size);
        float cursor_h = m_font_size * 1.2f;
        float cursor_y = std::max(0.0f, (m_size.y - cursor_h) * 0.5f);
        return Rect::from_xywh(abs.x + text_x + prefix_w + preedit_w, abs.y + cursor_y, 2.0f, cursor_h);
    }

    void on_ime_preedit(const std::string& text, size_t /*cursor_byte_pos*/) override {
        m_preedit_text = text;
        mark_needs_paint();
    }

    void on_ime_commit(const std::string& utf8_text) override {
        m_preedit_text.clear();
        if (!utf8_text.empty()) {
            append_chars(utf8_text);
        } else {
            mark_needs_paint();
        }
    }

    // 外部或测试注入按键处理
    void handle_text_key(const KeyEvent& ev) {
        if (!m_is_focused) return;

        if (ev.type == PlatformEventType::KeyDown) {
            constexpr uint32_t kKeyBackspace = 8;
            constexpr uint32_t kKeyEnter = 13;
            constexpr uint32_t kKeyDelete = 46;
            constexpr uint32_t kKeyLeft = 37;
            constexpr uint32_t kKeyRight = 39;
            constexpr uint32_t kKeyHome = 36;
            constexpr uint32_t kKeyEnd = 35;

            if (ev.key_code == kKeyBackspace) {
                if (m_cursor_index > 0 && !m_text.empty()) {
                    size_t prev = utf8_util::prev_char_pos(m_text, m_cursor_index);
                    m_text.erase(prev, m_cursor_index - prev);
                    m_cursor_index = prev;
                    notify_changed();
                }
            } else if (ev.key_code == kKeyDelete) {
                if (m_cursor_index < m_text.length()) {
                    size_t next = utf8_util::next_char_pos(m_text, m_cursor_index);
                    m_text.erase(m_cursor_index, next - m_cursor_index);
                    notify_changed();
                }
            } else if (ev.key_code == kKeyLeft) {
                if (m_cursor_index > 0) {
                    m_cursor_index = utf8_util::prev_char_pos(m_text, m_cursor_index);
                    mark_needs_paint();
                }
            } else if (ev.key_code == kKeyRight) {
                if (m_cursor_index < m_text.length()) {
                    m_cursor_index = utf8_util::next_char_pos(m_text, m_cursor_index);
                    mark_needs_paint();
                }
            } else if (ev.key_code == kKeyHome) {
                m_cursor_index = 0;
                mark_needs_paint();
            } else if (ev.key_code == kKeyEnd) {
                m_cursor_index = m_text.length();
                mark_needs_paint();
            } else if (ev.key_code == kKeyEnter) {
                if (m_on_submit) {
                    m_on_submit(m_text);
                }
            }
        }
        
        if (ev.type == PlatformEventType::ImeCommit || !ev.text.empty()) {
            if (!ev.text.empty()) {
                append_chars(ev.text);
            }
        }
    }

    // 直接键入 UTF-8 字符序列 (便捷测试与自动化输入)
    void append_chars(const std::string& chars) {
        if (chars.empty()) return;
        m_text.insert(m_cursor_index, chars);
        m_cursor_index += chars.size();
        notify_changed();
    }

private:
    void notify_changed() {
        mark_needs_layout();
        if (m_on_change) {
            m_on_change(m_text);
        }
    }

    void setup_behavior() {
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                set_input_focused(true);
                // 根据点击的局部 x 坐标按 UTF-8 码点边界推测光标位置
                float click_x = (ev.local_position.x > 0.0f || ev.local_position.y > 0.0f) ? ev.local_position.x : ev.position.x;
                float relative_x = click_x - m_padding.left;
                if (relative_x <= 0.0f) {
                    m_cursor_index = 0;
                } else {
                    size_t pos = 0;
                    float acc_w = 0.0f;
                    while (pos < m_text.size()) {
                        size_t next = utf8_util::next_char_pos(m_text, pos);
                        float cw = utf8_util::estimate_width(std::string_view(m_text).substr(pos, next - pos), m_font_size);
                        if (acc_w + cw * 0.5f >= relative_x) break;
                        acc_w += cw;
                        pos = next;
                    }
                    m_cursor_index = pos;
                }
                mark_needs_paint();
            }
        };

        on_focus = [this](FocusEvent& ev) {
            set_input_focused(ev.gained_focus);
            if (!ev.gained_focus) {
                m_preedit_text.clear();
            }
        };

        on_key = [this](KeyEvent& ev) {
            handle_text_key(ev);
        };
    }

    std::string m_text;
    std::string m_preedit_text;
    std::string m_placeholder;
    size_t m_cursor_index{0};
    bool m_is_focused{false};

    float m_height{26.0f};          // 桌面紧凑输入框标准 26px
    float m_corner_radius{3.0f};    // 桌面精致硬朗微圆角 3px
    EdgeInsets m_padding{EdgeInsets(4.0f, 8.0f)};
    Color m_bg_color{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color m_border_normal{Color{0.25f, 0.25f, 0.28f, 1.0f}};
    Color m_border_focus{Color{0.20f, 0.55f, 0.95f, 1.0f}}; // 桌面焦点蓝环
    Color m_text_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    Color m_placeholder_color{Color{0.45f, 0.45f, 0.48f, 1.0f}};
    float m_font_size{13.0f};

    std::function<void(const std::string&)> m_on_change{nullptr};
    std::function<void(const std::string&)> m_on_submit{nullptr};
};

class TextInputWidget;

// C++20 指定初始化器单行文本输入框 (TextInput)
// 桌面专业标准：高度 26px，硬朗微圆角 3px，双向绑定响应式同步
struct TextInput {
    std::string text{""};
    std::string placeholder{""};
    BindRef<std::string> bind_text{nullptr};
    float height{26.0f};          // 桌面高信息密度 26px
    float corner_radius{3.0f};    // 硬朗微圆角 3px
    EdgeInsets padding{EdgeInsets(4.0f, 8.0f)};
    Color background_color{Color{0.14f, 0.14f, 0.16f, 1.0f}};
    Color border_color{Color{0.25f, 0.25f, 0.28f, 1.0f}};
    Color focused_border_color{Color{0.20f, 0.55f, 0.95f, 1.0f}};
    Color text_color{Color{0.92f, 0.92f, 0.95f, 1.0f}};
    Color placeholder_color{Color{0.45f, 0.45f, 0.48f, 1.0f}};
    float font_size{13.0f};
    std::function<void(const std::string&)> on_change{nullptr};
    std::function<void(const std::string&)> on_submit{nullptr};

    operator WidgetPtr() const;
};

// TextInputWidget 包装实现
class TextInputWidget : public Widget {
public:
    explicit TextInputWidget(const TextInput& desc) : m_desc(desc) {}

    ~TextInputWidget() override {
        unsubscribe();
    }

    std::shared_ptr<RenderBox> create_render_node() override {
        std::string init_text = m_desc.bind_text ? m_desc.bind_text->get() : m_desc.text;
        auto node = std::make_shared<RenderTextInput>();

        // 双向同步回调
        auto on_ch = [this, node_weak = std::weak_ptr<RenderTextInput>(node)](const std::string& val) {
            m_is_internal_update = true;
            if (m_desc.bind_text) {
                m_desc.bind_text->set(val);
            }
            if (m_desc.on_change) {
                m_desc.on_change(val);
            }
            m_is_internal_update = false;
        };

        node->set_properties(init_text, m_desc.placeholder, m_desc.height, m_desc.corner_radius,
                             m_desc.padding, m_desc.background_color, m_desc.border_color,
                             m_desc.focused_border_color, m_desc.text_color,
                             m_desc.placeholder_color, m_desc.font_size, on_ch, m_desc.on_submit);

        setup_binding(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto input_box = std::dynamic_pointer_cast<RenderTextInput>(node)) {
            std::string current_val = m_desc.bind_text ? m_desc.bind_text->get() : m_desc.text;
            auto on_ch = [this](const std::string& val) {
                m_is_internal_update = true;
                if (m_desc.bind_text) {
                    m_desc.bind_text->set(val);
                }
                if (m_desc.on_change) {
                    m_desc.on_change(val);
                }
                m_is_internal_update = false;
            };

            input_box->set_properties(current_val, m_desc.placeholder, m_desc.height, m_desc.corner_radius,
                                      m_desc.padding, m_desc.background_color, m_desc.border_color,
                                      m_desc.focused_border_color, m_desc.text_color,
                                      m_desc.placeholder_color, m_desc.font_size, on_ch, m_desc.on_submit);

            setup_binding(input_box);
        }
    }

    [[nodiscard]] const TextInput& desc() const noexcept { return m_desc; }

private:
    void unsubscribe() {
        if (m_binding_token != 0 && m_desc.bind_text) {
            m_desc.bind_text->unsubscribe(m_binding_token);
            m_binding_token = 0;
        }
    }

    void setup_binding(std::shared_ptr<RenderTextInput> input_box) {
        unsubscribe();
        if (m_desc.bind_text) {
            std::weak_ptr<RenderTextInput> weak_box = input_box;
            m_binding_token = m_desc.bind_text->subscribe([this, weak_box](const std::string& new_val) {
                if (m_is_internal_update) return; // 避免双向更新死循环
                if (auto box = weak_box.lock()) {
                    box->set_text(new_val);
                }
            });
        }
    }

    TextInput m_desc;
    uint64_t m_binding_token{0};
    bool m_is_internal_update{false};
};

inline TextInput::operator WidgetPtr() const {
    return std::make_shared<TextInputWidget>(*this);
}

} // namespace ktui

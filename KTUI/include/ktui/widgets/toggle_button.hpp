#pragma once

#include "button/button_logic.hpp"
#include "widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/tree/render_box.hpp>

#include <functional>
#include <memory>
#include <string>

namespace ktui {

// WinUI 3 ToggleButton 切换按钮渲染盒
// 直接组合复用真实 RenderButton 子节点，100% 继承 WinUI 3 样式、立体描边、键盘交互与点击涟漪
class RenderToggleButton : public RenderBox {
public:
    RenderToggleButton() noexcept {
        m_btn = make_render_node<RenderButton>();
        m_btn->set_click_handler([this]() { toggle(); });
        append_child(m_btn.get());
    }

    ~RenderToggleButton() override {
        if (m_btn && m_btn->parent() == this) {
            remove_child(m_btn.get());
        }
    }

    void set_properties(bool checked,
                        std::string text,
                        std::string svg_path,
                        bool disabled,
                        Fn<void(bool)> on_change) {
        m_checked = checked;
        m_disabled = disabled;
        m_on_change = std::move(on_change);
        if (m_btn) {
            m_btn->set_text(std::move(text));
            m_btn->set_svg_path(std::move(svg_path));
            m_btn->set_disabled(disabled);
            m_btn->set_variant(m_checked ? ButtonVariant::Accent : ButtonVariant::Standard);
        }
        mark_needs_layout();
    }

    void set_checked(bool v) {
        if (m_checked != v) {
            m_checked = v;
            if (m_btn) m_btn->set_variant(m_checked ? ButtonVariant::Accent : ButtonVariant::Standard);
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool checked() const noexcept { return m_checked; }
    [[nodiscard]] const std::shared_ptr<RenderButton>& button() const noexcept { return m_btn; }

    void toggle() {
        if (m_disabled) return;
        m_checked = !m_checked;
        if (m_btn) m_btn->set_variant(m_checked ? ButtonVariant::Accent : ButtonVariant::Standard);
        mark_needs_paint();
        if (m_on_change) m_on_change(m_checked);
    }

    void perform_layout() override {
        if (m_btn) {
            m_btn->layout(m_constraints, true);
            m_btn->set_offset(Vec2{0.0f, 0.0f});
            set_size(m_btn->size());
        } else {
            set_size(m_constraints.constrain(Vec2{36.0f, 32.0f}));
        }
    }

    void paint(PaintContext& ctx) override {
        if (m_btn) {
            PaintContext::OffsetScope scope(ctx, m_btn->offset());
            m_btn->paint(ctx);
        }
    }

private:
    std::shared_ptr<RenderButton> m_btn;
    bool m_checked{false};
    bool m_disabled{false};
    Fn<void(bool)> m_on_change;

public:
    void wrap_on_change(Fn<void(bool)> extra) {
        auto prev = std::move(m_on_change);
        m_on_change = [prev = std::move(prev), extra = std::move(extra)](bool v) {
            if (extra) extra(v);
            if (prev) prev(v);
        };
    }
};

struct ToggleButton : BindableMixin<ToggleButton, RenderToggleButton> {
    bool checked{false};
    std::string text;
    std::string svg_path;
    bool disabled{false};
    Fn<void(bool)> on_change{nullptr};

    operator WidgetPtr() const&;

    operator WidgetPtr() &&;
};

inline void ktui_bind_two_way(const std::shared_ptr<RenderToggleButton>& node, Property<bool>& prop, BindingGroup& bg) {
    auto guard = std::make_shared<bool>(false);
    node->set_checked(prop.get());
    std::weak_ptr<RenderToggleButton> weak = node;
    bg.subscribe(prop, [weak, guard](bool v) {
        if (!*guard) {
            if (auto n = weak.lock()) n->set_checked(v);
        }
    });
    Property<bool>* p = &prop;
    node->wrap_on_change([p, guard](bool v) {
        *guard = true;
        p->set(v);
        *guard = false;
    });
}

class ToggleButtonWidget : public Widget {
public:
    explicit ToggleButtonWidget(const ToggleButton& desc) : m_desc(desc) {}
    explicit ToggleButtonWidget(ToggleButton&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ToggleButtonWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderToggleButton>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderToggleButton>(node)) {
            apply(n);
        }
    }

private:
    void apply(const std::shared_ptr<RenderToggleButton>& node) {
        node->set_properties(m_desc.checked, m_desc.text, m_desc.svg_path, m_desc.disabled, m_desc.on_change);
    }

    ToggleButton m_desc;
};

inline ToggleButton::operator WidgetPtr() const& {
    return make_widget<ToggleButtonWidget>(*this);
}
inline ToggleButton::operator WidgetPtr() && {
    return make_widget<ToggleButtonWidget>(std::move(*this));
}

} // namespace ktui

#pragma once

#include "layout_primitives_data.hpp"
#include "layout_primitives_style.hpp"
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <algorithm>

namespace ktui {


// ==================== 1. Flexible / Expanded (Flex 子项弹性包装) ====================
class RenderFlexItem : public RenderBox {
public:
    explicit RenderFlexItem(FlexItemData data = {}) noexcept {
        set_flex_data(data);
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    void perform_layout() override {
        if (!m_child) {
            m_size = m_constraints.constrain(Vec2{0.0f, 0.0f});
            return;
        }
        m_child->layout(m_constraints, true);
        m_child->set_offset(Vec2{0.0f, 0.0f});
        m_size = m_constraints.constrain(m_child->size());
    }

    [[nodiscard]] Vec2 compute_intrinsic_size(const BoxConstraints& constraints) const override {
        return m_child ? m_child->compute_intrinsic_size(constraints) : constraints.constrain(Vec2{0.0f, 0.0f});
    }

private:
    std::shared_ptr<RenderBox> m_child;
};

struct Flexible : BindableMixin<Flexible, RenderFlexItem> {
    float flex_grow{0.0f};
    float flex_shrink{1.0f};
    FlexFit fit{FlexFit::Loose};
    std::optional<AlignItems> align_self{std::nullopt};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class FlexibleWidget : public Widget {
public:
    explicit FlexibleWidget(const Flexible& desc) : m_desc(desc) {}
    explicit FlexibleWidget(Flexible&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        FlexItemData fd{m_desc.flex_grow, m_desc.flex_shrink, m_desc.fit, m_desc.align_self};
        auto node = make_render_node<RenderFlexItem>(fd);
        m_child_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_box) m_desc.child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto item = std::static_pointer_cast<RenderFlexItem>(node)) {
            item->set_flex_data(FlexItemData{m_desc.flex_grow, m_desc.flex_shrink, m_desc.fit, m_desc.align_self});
            if (m_desc.child) {
                if (!m_child_box) {
                    m_child_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_box);
                    item->set_child_box(m_child_box);
                } else {
                    m_desc.child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                item->set_child_box(nullptr);
            }
        }
    }

private:
    Flexible m_desc;
    std::shared_ptr<RenderBox> m_child_box;
};

inline Flexible::operator WidgetPtr() const& { return make_widget<FlexibleWidget>(*this); }
inline Flexible::operator WidgetPtr() && { return make_widget<FlexibleWidget>(std::move(*this)); }

struct Expanded {
    float flex{1.0f};
    std::optional<AlignItems> align_self{std::nullopt};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const& {
        return Flexible{
            .flex_grow = flex,
            .flex_shrink = 1.0f,
            .fit = FlexFit::Tight,
            .align_self = align_self,
            .child = child
        };
    }
    operator WidgetPtr() && {
        return Flexible{
            .flex_grow = flex,
            .flex_shrink = 1.0f,
            .fit = FlexFit::Tight,
            .align_self = align_self,
            .child = std::move(child)
        };
    }
};

// ==================== 2. Spacer (弹性占位间隔) ====================
class RenderSpacer : public RenderBox {
public:
    RenderSpacer(float flex = 1.0f, float min_length = 0.0f) noexcept
        : m_min_length(min_length) {
        set_flex_data(FlexItemData{flex, 1.0f, FlexFit::Tight, std::nullopt});
    }

    void set_params(float flex, float min_len) noexcept {
        set_flex_data(FlexItemData{flex, 1.0f, FlexFit::Tight, std::nullopt});
        if (m_min_length != min_len) {
            m_min_length = min_len;
            mark_needs_layout();
        }
    }

    void perform_layout() override {
        m_size = m_constraints.constrain(Vec2{m_min_length, m_min_length});
    }

    [[nodiscard]] bool hit_test(HitTestResult&, PointF) override { return false; }
    [[nodiscard]] bool hit_test_touch(HitTestResult&, PointF) override { return false; }

private:
    float m_min_length{0.0f};
};

struct Spacer {
    float flex{1.0f};
    float min_length{0.0f};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class SpacerWidget : public Widget {
public:
    explicit SpacerWidget(const Spacer& desc) : m_desc(desc) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderSpacer>(m_desc.flex, m_desc.min_length);
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto sp = std::static_pointer_cast<RenderSpacer>(node)) {
            sp->set_params(m_desc.flex, m_desc.min_length);
        }
    }

private:
    Spacer m_desc;
};

inline Spacer::operator WidgetPtr() const& { return make_widget<SpacerWidget>(*this); }
inline Spacer::operator WidgetPtr() && { return make_widget<SpacerWidget>(*this); }

// ==================== 3. Align & Center (对齐容器) ====================
class RenderAlign : public RenderBox {
public:
    RenderAlign(Alignment alignment = Alignment::center(),
                float width_factor = -1.0f,
                float height_factor = -1.0f) noexcept
        : m_alignment(alignment), m_width_factor(width_factor), m_height_factor(height_factor) {}

    void set_alignment(Alignment a) noexcept {
        if (!(m_alignment == a)) {
            m_alignment = a;
            mark_needs_layout();
        }
    }

    void set_factors(float wf, float hf) noexcept {
        if (m_width_factor != wf || m_height_factor != hf) {
            m_width_factor = wf;
            m_height_factor = hf;
            mark_needs_layout();
        }
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    void perform_layout() override {
        // 仅当未指定 factor 且约束在对应轴为紧约束 (min == max) 时才撑满；
        // 在 VStack/HStack 传入的松散有界约束 (min == 0, max == 视口高/宽) 下，
        // 必须按子项尺寸收缩，避免单个 Align 撑满整页把后续控件挤出视口。
        const bool tight_w = m_constraints.has_bounded_width() &&
                             (std::abs(m_constraints.max_width - m_constraints.min_width) < 0.01f);
        const bool tight_h = m_constraints.has_bounded_height() &&
                             (std::abs(m_constraints.max_height - m_constraints.min_height) < 0.01f);
        const bool shrink_w = (m_width_factor >= 0.0f) || !tight_w;
        const bool shrink_h = (m_height_factor >= 0.0f) || !tight_h;

        if (!m_child) {
            m_size = m_constraints.constrain(Vec2{
                shrink_w ? 0.0f : m_constraints.max_width,
                shrink_h ? 0.0f : m_constraints.max_height
            });
            return;
        }

        m_child->layout(m_constraints.loosen(), true);
        Vec2 csz = m_child->size();
        float w = shrink_w ? csz.x * (m_width_factor >= 0.0f ? m_width_factor : 1.0f) : m_constraints.max_width;
        float h = shrink_h ? csz.y * (m_height_factor >= 0.0f ? m_height_factor : 1.0f) : m_constraints.max_height;
        m_size = m_constraints.constrain(Vec2{w, h});
        m_child->set_offset(m_alignment.compute_offset(m_size, csz));
    }

private:
    Alignment m_alignment{Alignment::center()};
    float m_width_factor{-1.0f};
    float m_height_factor{-1.0f};
    std::shared_ptr<RenderBox> m_child;
};

struct Align : BindableMixin<Align, RenderAlign> {
    Alignment alignment{Alignment::center()};
    float width_factor{-1.0f};
    float height_factor{-1.0f};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class AlignWidget : public Widget {
public:
    explicit AlignWidget(const Align& desc) : m_desc(desc) {}
    explicit AlignWidget(Align&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderAlign>(m_desc.alignment, m_desc.width_factor, m_desc.height_factor);
        m_child_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_box) m_desc.child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto a = std::static_pointer_cast<RenderAlign>(node)) {
            a->set_alignment(m_desc.alignment);
            a->set_factors(m_desc.width_factor, m_desc.height_factor);
            if (m_desc.child) {
                if (!m_child_box) {
                    m_child_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_box);
                    a->set_child_box(m_child_box);
                } else {
                    m_desc.child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                a->set_child_box(nullptr);
            }
        }
    }

private:
    Align m_desc;
    std::shared_ptr<RenderBox> m_child_box;
};

inline Align::operator WidgetPtr() const& { return make_widget<AlignWidget>(*this); }
inline Align::operator WidgetPtr() && { return make_widget<AlignWidget>(std::move(*this)); }

struct Center {
    float width_factor{-1.0f};
    float height_factor{-1.0f};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const& {
        return Align{
            .alignment = Alignment::center(),
            .width_factor = width_factor,
            .height_factor = height_factor,
            .child = child
        };
    }
    operator WidgetPtr() && {
        return Align{
            .alignment = Alignment::center(),
            .width_factor = width_factor,
            .height_factor = height_factor,
            .child = std::move(child)
        };
    }
};

// ==================== 4. Padding (轻量内边距包装) ====================
class RenderPadding : public RenderBox {
public:
    explicit RenderPadding(EdgeInsets insets = EdgeInsets::zero()) noexcept : m_insets(insets) {}

    void set_insets(EdgeInsets insets) noexcept {
        if (!(m_insets == insets)) {
            m_insets = insets;
            mark_needs_layout();
        }
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    void perform_layout() override {
        Vec2 pad{m_insets.horizontal(), m_insets.vertical()};
        if (!m_child) {
            m_size = m_constraints.constrain(pad);
            return;
        }
        m_child->layout(m_constraints.deflate(m_insets), true);
        m_child->set_offset(Vec2{m_insets.left, m_insets.top});
        m_size = m_constraints.constrain(m_child->size() + pad);
    }

private:
    EdgeInsets m_insets{EdgeInsets::zero()};
    std::shared_ptr<RenderBox> m_child;
};

struct Padding : BindableMixin<Padding, RenderPadding> {
    EdgeInsets insets{EdgeInsets::zero()};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class PaddingWidget : public Widget {
public:
    explicit PaddingWidget(const Padding& desc) : m_desc(desc) {}
    explicit PaddingWidget(Padding&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderPadding>(m_desc.insets);
        m_child_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_box) m_desc.child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto p = std::static_pointer_cast<RenderPadding>(node)) {
            p->set_insets(m_desc.insets);
            if (m_desc.child) {
                if (!m_child_box) {
                    m_child_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_box);
                    p->set_child_box(m_child_box);
                } else {
                    m_desc.child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                p->set_child_box(nullptr);
            }
        }
    }

private:
    Padding m_desc;
    std::shared_ptr<RenderBox> m_child_box;
};

inline Padding::operator WidgetPtr() const& { return make_widget<PaddingWidget>(*this); }
inline Padding::operator WidgetPtr() && { return make_widget<PaddingWidget>(std::move(*this)); }

// ==================== 5. AspectRatio (固定宽高比容器) ====================
class RenderAspectRatio : public RenderBox {
public:
    explicit RenderAspectRatio(float ratio = 16.0f / 9.0f) noexcept
        : m_ratio(ratio > 0.001f ? ratio : 1.0f) {}

    void set_ratio(float r) noexcept {
        float clamped = (r > 0.001f) ? r : 1.0f;
        if (m_ratio != clamped) {
            m_ratio = clamped;
            mark_needs_layout();
        }
    }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    void perform_layout() override {
        float w = 0.0f;
        float h = 0.0f;
        if (m_constraints.is_tight()) {
            w = m_constraints.max_width;
            h = m_constraints.max_height;
        } else if (m_constraints.has_bounded_width()) {
            w = m_constraints.max_width;
            h = w / m_ratio;
            if (m_constraints.has_bounded_height() && h > m_constraints.max_height) {
                h = m_constraints.max_height;
                w = h * m_ratio;
            }
        } else if (m_constraints.has_bounded_height()) {
            h = m_constraints.max_height;
            w = h * m_ratio;
        } else {
            w = std::max(m_constraints.min_width, 100.0f);
            h = w / m_ratio;
        }
        m_size = m_constraints.constrain(Vec2{w, h});
        if (m_child) {
            m_child->layout(BoxConstraints::tight(m_size), true);
            m_child->set_offset(Vec2{0.0f, 0.0f});
        }
    }

private:
    float m_ratio{16.0f / 9.0f};
    std::shared_ptr<RenderBox> m_child;
};

struct AspectRatio : BindableMixin<AspectRatio, RenderAspectRatio> {
    float ratio{16.0f / 9.0f};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class AspectRatioWidget : public Widget {
public:
    explicit AspectRatioWidget(const AspectRatio& desc) : m_desc(desc) {}
    explicit AspectRatioWidget(AspectRatio&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderAspectRatio>(m_desc.ratio);
        m_child_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_box) m_desc.child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto ar = std::static_pointer_cast<RenderAspectRatio>(node)) {
            ar->set_ratio(m_desc.ratio);
            if (m_desc.child) {
                if (!m_child_box) {
                    m_child_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_box);
                    ar->set_child_box(m_child_box);
                } else {
                    m_desc.child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                ar->set_child_box(nullptr);
            }
        }
    }

private:
    AspectRatio m_desc;
    std::shared_ptr<RenderBox> m_child_box;
};

inline AspectRatio::operator WidgetPtr() const& { return make_widget<AspectRatioWidget>(*this); }
inline AspectRatio::operator WidgetPtr() && { return make_widget<AspectRatioWidget>(std::move(*this)); }

// ==================== 6. Divider (主题自适应分隔线) ====================
class RenderDivider : public RenderBox {
public:
    RenderDivider(bool vertical = false, float thickness = 1.0f, float length = -1.0f,
                  float indent = 0.0f, float end_indent = 0.0f,
                  std::optional<Color> color = std::nullopt) noexcept
        : m_vertical(vertical), m_thickness(thickness), m_length(length),
          m_indent(indent), m_end_indent(end_indent), m_color(color) {}

    void set_props(bool vertical, float thickness, float length, float indent, float end_indent,
                   std::optional<Color> color) noexcept {
        m_vertical = vertical;
        m_thickness = thickness;
        m_length = length;
        m_indent = indent;
        m_end_indent = end_indent;
        m_color = color;
        mark_needs_layout();
    }

    void perform_layout() override {
        if (m_vertical) {
            float h = (m_length >= 0.0f) ? m_length
                : (m_constraints.has_bounded_height() ? m_constraints.max_height : 20.0f);
            m_size = m_constraints.constrain(Vec2{m_thickness, h});
        } else {
            float w = (m_length >= 0.0f) ? m_length
                : (m_constraints.has_bounded_width() ? m_constraints.max_width : 120.0f);
            m_size = m_constraints.constrain(Vec2{w, m_thickness});
        }
    }

    void paint(PaintContext& context) override {
        const Theme& t = ThemeContext::current();
        Color c = m_color.value_or(t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.085f}
                                             : Color{0.0f, 0.0f, 0.0f, 0.085f});
        if (m_vertical) {
            float y0 = m_indent;
            float h = std::max(0.0f, m_size.y - m_indent - m_end_indent);
            if (h > 0.0f) context.draw_rect(Rect::from_xywh(0.0f, y0, m_thickness, h), c, 0.0f, 0.0f);
        } else {
            float x0 = m_indent;
            float w = std::max(0.0f, m_size.x - m_indent - m_end_indent);
            if (w > 0.0f) context.draw_rect(Rect::from_xywh(x0, 0.0f, w, m_thickness), c, 0.0f, 0.0f);
        }
    }

    [[nodiscard]] bool hit_test(HitTestResult&, PointF) override { return false; }
    [[nodiscard]] bool hit_test_touch(HitTestResult&, PointF) override { return false; }

private:
    bool m_vertical{false};
    float m_thickness{1.0f};
    float m_length{-1.0f};
    float m_indent{0.0f};
    float m_end_indent{0.0f};
    std::optional<Color> m_color{std::nullopt};
};

struct Divider {
    bool vertical{false};
    float thickness{1.0f};
    float length{-1.0f};
    float indent{0.0f};
    float end_indent{0.0f};
    std::optional<Color> color{std::nullopt};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class DividerWidget : public Widget {
public:
    explicit DividerWidget(const Divider& desc) : m_desc(desc) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderDivider>(
            m_desc.vertical, m_desc.thickness, m_desc.length,
            m_desc.indent, m_desc.end_indent, m_desc.color);
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto d = std::static_pointer_cast<RenderDivider>(node)) {
            d->set_props(m_desc.vertical, m_desc.thickness, m_desc.length,
                         m_desc.indent, m_desc.end_indent, m_desc.color);
        }
    }

private:
    Divider m_desc;
};

inline Divider::operator WidgetPtr() const& { return make_widget<DividerWidget>(*this); }
inline Divider::operator WidgetPtr() && { return make_widget<DividerWidget>(*this); }

// ==================== 7. Visibility (响应式显隐容器: Collapse / Hidden) ====================
class RenderVisibility : public RenderBox {
public:
    RenderVisibility(bool visible = true, bool maintain_size = false) noexcept
        : m_visible(visible), m_maintain_size(maintain_size) {}

    void set_visible(bool v) noexcept {
        if (m_visible != v) {
            m_visible = v;
            mark_needs_layout();
            mark_needs_paint();
        }
    }
    [[nodiscard]] bool is_visible() const noexcept { return m_visible; }

    void set_maintain_size(bool m) noexcept {
        if (m_maintain_size != m) {
            m_maintain_size = m;
            mark_needs_layout();
        }
    }
    [[nodiscard]] bool maintain_size() const noexcept { return m_maintain_size; }

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    void perform_layout() override {
        if (!m_child || (!m_visible && !m_maintain_size)) {
            m_size = m_constraints.constrain(Vec2{0.0f, 0.0f});
            return;
        }
        m_child->layout(m_constraints, true);
        m_child->set_offset(Vec2{0.0f, 0.0f});
        m_size = m_constraints.constrain(m_child->size());
    }

    void paint(PaintContext& context) override {
        if (!m_visible) return;
        RenderBox::paint(context);
    }

    bool hit_test(HitTestResult& result, PointF position) override {
        if (!m_visible) return false;
        return RenderBox::hit_test(result, position);
    }

    bool hit_test_touch(HitTestResult& result, PointF position) override {
        if (!m_visible) return false;
        return RenderBox::hit_test_touch(result, position);
    }

private:
    bool m_visible{true};
    bool m_maintain_size{false};
    std::shared_ptr<RenderBox> m_child;
};

struct Visibility : BindableMixin<Visibility, RenderVisibility> {
    bool visible{true};
    bool maintain_size{false};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class VisibilityWidget : public Widget {
public:
    explicit VisibilityWidget(const Visibility& desc) : m_desc(desc) {}
    explicit VisibilityWidget(Visibility&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderVisibility>(m_desc.visible, m_desc.maintain_size);
        m_child_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_box) m_desc.child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto v = std::static_pointer_cast<RenderVisibility>(node)) {
            v->set_visible(m_desc.visible);
            v->set_maintain_size(m_desc.maintain_size);
            if (m_desc.child) {
                if (!m_child_box) {
                    m_child_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_box);
                    v->set_child_box(m_child_box);
                } else {
                    m_desc.child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                v->set_child_box(nullptr);
            }
        }
    }

private:
    Visibility m_desc;
    std::shared_ptr<RenderBox> m_child_box;
};

inline Visibility::operator WidgetPtr() const& { return make_widget<VisibilityWidget>(*this); }
inline Visibility::operator WidgetPtr() && { return make_widget<VisibilityWidget>(std::move(*this)); }
} // namespace ktui
 // namespace ktui

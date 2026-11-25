#pragma once

#include "widget.hpp"
#include <ktui/layout/flex_types.hpp>
#include <ktui/binding/binding.hpp>
#include <algorithm>
#include <memory>
#include <vector>

namespace ktui {

// 流式自动换行布局渲染盒 (RenderWrap)
// 沿主轴顺序排布子项，超出主轴上限时自动折行/折列，支持行内主轴对齐、行内交叉轴对齐与行列间距
class RenderWrap : public RenderBox {
public:
    RenderWrap(FlexDirection direction = FlexDirection::Row,
               float spacing = 6.0f,
               float run_spacing = 6.0f,
               JustifyContent alignment = JustifyContent::FlexStart,
               AlignItems cross_alignment = AlignItems::Center,
               EdgeInsets padding = EdgeInsets::zero()) noexcept
        : m_direction(direction),
          m_spacing(spacing),
          m_run_spacing(run_spacing),
          m_alignment(alignment),
          m_cross_alignment(cross_alignment),
          m_padding(padding) {}

    void set_direction(FlexDirection dir) noexcept {
        if (m_direction != dir) { m_direction = dir; mark_needs_layout(); }
    }
    void set_spacing(float s) noexcept {
        if (m_spacing != s) { m_spacing = s; mark_needs_layout(); }
    }
    void set_run_spacing(float rs) noexcept {
        if (m_run_spacing != rs) { m_run_spacing = rs; mark_needs_layout(); }
    }
    void set_alignment(JustifyContent j) noexcept {
        if (m_alignment != j) { m_alignment = j; mark_needs_layout(); }
    }
    void set_cross_alignment(AlignItems a) noexcept {
        if (m_cross_alignment != a) { m_cross_alignment = a; mark_needs_layout(); }
    }
    void set_padding(EdgeInsets p) noexcept {
        if (!(m_padding == p)) { m_padding = p; mark_needs_layout(); }
    }

    void set_child_boxes(std::vector<std::shared_ptr<RenderBox>> boxes) {
        while (first_child()) remove_child(first_child());
        m_child_boxes = std::move(boxes);
        for (auto& c : m_child_boxes) {
            if (c) append_child(c.get());
        }
        mark_needs_layout();
    }

    void perform_layout() override {
        BoxConstraints inner = m_constraints.deflate(m_padding);
        const bool is_horizontal = (m_direction == FlexDirection::Row || m_direction == FlexDirection::RowReverse);
        const float max_main = is_horizontal ? inner.max_width : inner.max_height;
        const bool main_bounded = (max_main < BoxConstraints::kInfinity);

        auto get_main = [is_horizontal](Vec2 v) noexcept { return is_horizontal ? v.x : v.y; };
        auto get_cross = [is_horizontal](Vec2 v) noexcept { return is_horizontal ? v.y : v.x; };
        auto make_vec = [is_horizontal](float m, float c) noexcept {
            return is_horizontal ? Vec2{m, c} : Vec2{c, m};
        };

        struct RunInfo {
            size_t start_idx{0};
            size_t count{0};
            float main_extent{0.0f};
            float cross_extent{0.0f};
        };

        std::vector<RenderBox*> children;
        children.reserve(m_child_boxes.size());
        for (RenderNode* n = first_child(); n != nullptr; n = n->next_sibling()) {
            if (auto* b = n->as_render_box()) children.push_back(b);
        }

        if (children.empty()) {
            m_size = m_constraints.constrain(Vec2{m_padding.horizontal(), m_padding.vertical()});
            return;
        }

        BoxConstraints child_constraints = inner.loosen();
        std::vector<RunInfo> runs;
        RunInfo cur_run{0, 0, 0.0f, 0.0f};

        // Pass 1: 测量子项并切分行 (Run)
        for (size_t i = 0; i < children.size(); ++i) {
            RenderBox* child = children[i];
            child->layout(child_constraints, true);
            const float cm = get_main(child->size());
            const float cc = get_cross(child->size());

            const float needed = (cur_run.count == 0) ? cm : (cur_run.main_extent + m_spacing + cm);
            if (cur_run.count > 0 && main_bounded && needed > max_main + 0.01f) {
                runs.push_back(cur_run);
                cur_run = RunInfo{i, 1, cm, cc};
            } else {
                cur_run.count++;
                cur_run.main_extent = needed;
                cur_run.cross_extent = std::max(cur_run.cross_extent, cc);
            }
        }
        if (cur_run.count > 0) {
            runs.push_back(cur_run);
        }

        float max_run_main = 0.0f;
        float total_cross = 0.0f;
        for (size_t r = 0; r < runs.size(); ++r) {
            max_run_main = std::max(max_run_main, runs[r].main_extent);
            total_cross += runs[r].cross_extent;
            if (r + 1 < runs.size()) total_cross += m_run_spacing;
        }

        const float inner_main = (main_bounded && m_alignment != JustifyContent::FlexStart)
            ? max_main
            : std::clamp(max_run_main, is_horizontal ? inner.min_width : inner.min_height, max_main);
        const float inner_cross = std::clamp(
            total_cross,
            is_horizontal ? inner.min_height : inner.min_width,
            is_horizontal ? inner.max_height : inner.max_width);

        Vec2 pad_sz{m_padding.horizontal(), m_padding.vertical()};
        m_size = m_constraints.constrain(make_vec(inner_main, inner_cross) + pad_sz);

        const float final_inner_main = std::max(0.0f, get_main(m_size) - (is_horizontal ? pad_sz.x : pad_sz.y));

        // Pass 2: 逐行定位子项
        float run_cross_offset = 0.0f;
        for (const auto& run : runs) {
            float free_main = std::max(0.0f, final_inner_main - run.main_extent);
            float cursor_main = 0.0f;
            float gap = m_spacing;

            switch (m_alignment) {
                case JustifyContent::FlexStart:
                    cursor_main = 0.0f;
                    break;
                case JustifyContent::FlexEnd:
                    cursor_main = free_main;
                    break;
                case JustifyContent::Center:
                    cursor_main = free_main * 0.5f;
                    break;
                case JustifyContent::SpaceBetween:
                    cursor_main = 0.0f;
                    if (run.count > 1) gap = m_spacing + free_main / static_cast<float>(run.count - 1);
                    break;
                case JustifyContent::SpaceAround: {
                    float unit = free_main / (2.0f * static_cast<float>(run.count));
                    cursor_main = unit;
                    gap = m_spacing + unit * 2.0f;
                    break;
                }
                case JustifyContent::SpaceEvenly: {
                    float unit = free_main / static_cast<float>(run.count + 1);
                    cursor_main = unit;
                    gap = m_spacing + unit;
                    break;
                }
            }

            for (size_t k = 0; k < run.count; ++k) {
                RenderBox* child = children[run.start_idx + k];
                float cc = get_cross(child->size());
                float local_cross = 0.0f;
                switch (m_cross_alignment) {
                    case AlignItems::FlexStart:
                    case AlignItems::Stretch:
                        local_cross = 0.0f;
                        break;
                    case AlignItems::Center:
                        local_cross = (run.cross_extent - cc) * 0.5f;
                        break;
                    case AlignItems::FlexEnd:
                        local_cross = run.cross_extent - cc;
                        break;
                }
                Vec2 pos = make_vec(cursor_main, run_cross_offset + local_cross);
                child->set_offset(Vec2{m_padding.left + pos.x, m_padding.top + pos.y});
                cursor_main += get_main(child->size()) + gap;
            }
            run_cross_offset += run.cross_extent + m_run_spacing;
        }
    }

private:
    FlexDirection m_direction{FlexDirection::Row};
    float m_spacing{6.0f};
    float m_run_spacing{6.0f};
    JustifyContent m_alignment{JustifyContent::FlexStart};
    AlignItems m_cross_alignment{AlignItems::Center};
    EdgeInsets m_padding{EdgeInsets::zero()};
    std::vector<std::shared_ptr<RenderBox>> m_child_boxes;
};

struct Wrap : BindableMixin<Wrap, RenderWrap> {
    FlexDirection direction{FlexDirection::Row};
    float spacing{6.0f};
    float run_spacing{6.0f};
    JustifyContent alignment{JustifyContent::FlexStart};
    AlignItems cross_alignment{AlignItems::Center};
    EdgeInsets padding{EdgeInsets::zero()};
    std::vector<WidgetPtr> content{};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class WrapWidget : public Widget {
public:
    explicit WrapWidget(const Wrap& desc) : m_desc(desc) {}
    explicit WrapWidget(Wrap&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderWrap>(
            m_desc.direction, m_desc.spacing, m_desc.run_spacing,
            m_desc.alignment, m_desc.cross_alignment, m_desc.padding);
        m_child_boxes.clear();
        m_child_boxes.reserve(m_desc.content.size());
        for (auto& w : m_desc.content) {
            if (w) {
                auto cb = w->create_render_node();
                w->set_render_node(cb);
                m_child_boxes.push_back(cb);
            }
        }
        node->set_child_boxes(m_child_boxes);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto wrap = std::static_pointer_cast<RenderWrap>(node)) {
            wrap->set_direction(m_desc.direction);
            wrap->set_spacing(m_desc.spacing);
            wrap->set_run_spacing(m_desc.run_spacing);
            wrap->set_alignment(m_desc.alignment);
            wrap->set_cross_alignment(m_desc.cross_alignment);
            wrap->set_padding(m_desc.padding);
            m_child_boxes.clear();
            m_child_boxes.reserve(m_desc.content.size());
            for (auto& w : m_desc.content) {
                if (w) {
                    auto existing = w->render_node();
                    if (!existing) {
                        existing = w->create_render_node();
                        w->set_render_node(existing);
                    } else {
                        w->update_render_node(existing);
                    }
                    m_child_boxes.push_back(existing);
                }
            }
            wrap->set_child_boxes(m_child_boxes);
        }
    }

private:
    Wrap m_desc;
    std::vector<std::shared_ptr<RenderBox>> m_child_boxes;
};

inline Wrap::operator WidgetPtr() const& { return make_widget<WrapWidget>(*this); }
inline Wrap::operator WidgetPtr() && { return make_widget<WrapWidget>(std::move(*this)); }

} // namespace ktui

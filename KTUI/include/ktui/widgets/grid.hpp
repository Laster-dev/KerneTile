#pragma once

#include "widget.hpp"
#include <ktui/layout/flex_types.hpp>
#include <ktui/binding/binding.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

namespace ktui {

// 网格跨列子项包装 (GridItem)
// 通过 flex_data().flex_grow 编码跨列数，零 RTTI 即可被 RenderGrid 识别
class RenderGridItem : public RenderBox {
public:
    explicit RenderGridItem(uint32_t col_span = 1) noexcept
        : m_col_span(std::max<uint32_t>(1, col_span)) {
        // 复用 FlexItemData 携带跨列提示 (RenderGrid 内部通过 grid_column_span() 读取)
        set_flex_data(FlexItemData{static_cast<float>(m_col_span), 0.0f, FlexFit::Tight, std::nullopt});
    }

    void set_column_span(uint32_t span) noexcept {
        uint32_t s = std::max<uint32_t>(1, span);
        if (m_col_span != s) {
            m_col_span = s;
            set_flex_data(FlexItemData{static_cast<float>(m_col_span), 0.0f, FlexFit::Tight, std::nullopt});
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
        if (!m_child) {
            m_size = m_constraints.constrain(Vec2{0.0f, 0.0f});
            return;
        }
        m_child->layout(m_constraints, true);
        m_child->set_offset(Vec2{0.0f, 0.0f});
        m_size = m_constraints.constrain(m_child->size());
    }

private:
    uint32_t m_col_span{1};
    std::shared_ptr<RenderBox> m_child;
};

struct GridItem {
    uint32_t column_span{1};
    WidgetPtr child{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class GridItemWidget : public Widget {
public:
    explicit GridItemWidget(const GridItem& desc) : m_desc(desc) {}
    explicit GridItemWidget(GridItem&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderGridItem>(m_desc.column_span);
        m_child_box = m_desc.child ? m_desc.child->create_render_node() : nullptr;
        if (m_desc.child && m_child_box) m_desc.child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto gi = std::static_pointer_cast<RenderGridItem>(node)) {
            gi->set_column_span(m_desc.column_span);
            if (m_desc.child) {
                if (!m_child_box) {
                    m_child_box = m_desc.child->create_render_node();
                    m_desc.child->set_render_node(m_child_box);
                    gi->set_child_box(m_child_box);
                } else {
                    m_desc.child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                gi->set_child_box(nullptr);
            }
        }
    }

private:
    GridItem m_desc;
    std::shared_ptr<RenderBox> m_child_box;
};

inline GridItem::operator WidgetPtr() const& { return make_widget<GridItemWidget>(*this); }
inline GridItem::operator WidgetPtr() && { return make_widget<GridItemWidget>(std::move(*this)); }

// 二维网格布局渲染盒 (RenderGrid)
// 支持：
// 1. 固定列数 (columns)
// 2. 响应式自动列数 (min_item_width > 0，等价 CSS repeat(auto-fit, minmax(min_item_width, 1fr)))
// 3. 比例权重轨道 (column_weights，等价 CSS 1fr 2fr 1fr)
// 4. 固定行高或自动行高 (cell_height <= 0 时每行自动对齐至该行最高子项)
// 5. 子项跨列 (GridItem { .column_span = 2, ... })
class RenderGrid : public RenderBox {
public:
    RenderGrid(uint32_t columns = 2,
               float min_item_width = 0.0f,
               std::vector<float> column_weights = {},
               float column_spacing = 8.0f,
               float row_spacing = 8.0f,
               float cell_height = -1.0f,
               EdgeInsets padding = EdgeInsets::zero()) noexcept
        : m_columns(std::max<uint32_t>(1, columns)),
          m_min_item_width(min_item_width),
          m_column_weights(std::move(column_weights)),
          m_column_spacing(column_spacing),
          m_row_spacing(row_spacing),
          m_cell_height(cell_height),
          m_padding(padding) {}

    void set_columns(uint32_t cols) noexcept {
        uint32_t c = std::max<uint32_t>(1, cols);
        if (m_columns != c) { m_columns = c; mark_needs_layout(); }
    }
    void set_min_item_width(float w) noexcept {
        if (m_min_item_width != w) { m_min_item_width = w; mark_needs_layout(); }
    }
    void set_column_weights(std::vector<float> weights) {
        if (m_column_weights != weights) { m_column_weights = std::move(weights); mark_needs_layout(); }
    }
    void set_column_spacing(float s) noexcept {
        if (m_column_spacing != s) { m_column_spacing = s; mark_needs_layout(); }
    }
    void set_row_spacing(float s) noexcept {
        if (m_row_spacing != s) { m_row_spacing = s; mark_needs_layout(); }
    }
    void set_cell_height(float h) noexcept {
        if (m_cell_height != h) { m_cell_height = h; mark_needs_layout(); }
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

    [[nodiscard]] uint32_t resolved_columns() const noexcept { return m_resolved_columns; }

    void perform_layout() override {
        BoxConstraints inner = m_constraints.deflate(m_padding);
        const float avail_w = inner.has_bounded_width() ? inner.max_width : std::max(inner.min_width, 400.0f);

        // 1. 解析列数 (响应式 min_item_width 优先，否则按 column_weights 或 columns)
        uint32_t cols = m_columns;
        if (m_min_item_width > 1.0f) {
            float raw = std::floor((avail_w + m_column_spacing) / (m_min_item_width + m_column_spacing));
            cols = static_cast<uint32_t>(std::max(1.0f, raw));
        } else if (!m_column_weights.empty()) {
            cols = static_cast<uint32_t>(m_column_weights.size());
        }
        cols = std::max<uint32_t>(1, cols);
        m_resolved_columns = cols;

        // 2. 计算每列宽度与起始 X 坐标
        const float total_col_gap = (cols > 1) ? m_column_spacing * static_cast<float>(cols - 1) : 0.0f;
        const float track_avail_w = std::max(0.0f, avail_w - total_col_gap);

        std::vector<float> col_widths(cols, track_avail_w / static_cast<float>(cols));
        if (m_min_item_width <= 1.0f && m_column_weights.size() == cols) {
            float weight_sum = 0.0f;
            for (float w : m_column_weights) weight_sum += std::max(0.01f, w);
            for (uint32_t c = 0; c < cols; ++c) {
                col_widths[c] = track_avail_w * (std::max(0.01f, m_column_weights[c]) / weight_sum);
            }
        }

        std::vector<float> col_x(cols, 0.0f);
        for (uint32_t c = 1; c < cols; ++c) {
            col_x[c] = col_x[c - 1] + col_widths[c - 1] + m_column_spacing;
        }

        // 3. 收集子节点并按行排布 (支持 GridItem 跨列)
        std::vector<RenderBox*> children;
        children.reserve(m_child_boxes.size());
        for (RenderNode* n = first_child(); n != nullptr; n = n->next_sibling()) {
            if (auto* b = n->as_render_box()) children.push_back(b);
        }

        if (children.empty()) {
            m_size = m_constraints.constrain(Vec2{avail_w + m_padding.horizontal(), m_padding.vertical()});
            return;
        }

        struct PlacedCell {
            RenderBox* box;
            uint32_t col;
            uint32_t span;
            float width;
        };

        std::vector<PlacedCell> row_cells;
        row_cells.reserve(cols);
        float cursor_y = 0.0f;
        uint32_t cur_col = 0;

        auto flush_row = [&]() {
            if (row_cells.empty()) return;
            float row_h = (m_cell_height > 0.0f) ? m_cell_height : 0.0f;
            if (m_cell_height <= 0.0f) {
                for (const auto& cell : row_cells) {
                    row_h = std::max(row_h, cell.box->size().y);
                }
                // 自动行高模式下，将同行的矮子项拉伸至行高，保持整齐卡片视觉
                for (const auto& cell : row_cells) {
                    if (cell.box->size().y + 0.5f < row_h) {
                        cell.box->layout(BoxConstraints::tight(Vec2{cell.width, row_h}), true);
                    }
                }
            }
            for (const auto& cell : row_cells) {
                cell.box->set_offset(Vec2{m_padding.left + col_x[cell.col], m_padding.top + cursor_y});
            }
            cursor_y += row_h + m_row_spacing;
            row_cells.clear();
            cur_col = 0;
        };

        for (RenderBox* child : children) {
            uint32_t span = 1;
            const auto& fd = child->flex_data();
            if (fd.flex_grow >= 1.0f && fd.flex_shrink == 0.0f) {
                span = static_cast<uint32_t>(std::lround(fd.flex_grow));
            }
            span = std::clamp<uint32_t>(span, 1, cols);

            if (cur_col + span > cols) {
                flush_row();
            }

            float cell_w = 0.0f;
            for (uint32_t c = cur_col; c < cur_col + span; ++c) {
                cell_w += col_widths[c];
                if (c > cur_col) cell_w += m_column_spacing;
            }

            BoxConstraints cell_constraints = (m_cell_height > 0.0f)
                ? BoxConstraints::tight(Vec2{cell_w, m_cell_height})
                : BoxConstraints(cell_w, cell_w, 0.0f, inner.max_height);
            child->layout(cell_constraints, true);

            row_cells.push_back(PlacedCell{child, cur_col, span, cell_w});
            cur_col += span;
            if (cur_col >= cols) {
                flush_row();
            }
        }
        flush_row();

        float total_h = (cursor_y > m_row_spacing) ? (cursor_y - m_row_spacing) : 0.0f;
        Vec2 pad_sz{m_padding.horizontal(), m_padding.vertical()};
        m_size = m_constraints.constrain(Vec2{avail_w + pad_sz.x, total_h + pad_sz.y});
    }

private:
    uint32_t m_columns{2};
    uint32_t m_resolved_columns{2};
    float m_min_item_width{0.0f};
    std::vector<float> m_column_weights;
    float m_column_spacing{8.0f};
    float m_row_spacing{8.0f};
    float m_cell_height{-1.0f};
    EdgeInsets m_padding{EdgeInsets::zero()};
    std::vector<std::shared_ptr<RenderBox>> m_child_boxes;
};

struct Grid : BindableMixin<Grid, RenderGrid> {
    uint32_t columns{2};
    float min_item_width{0.0f};
    std::vector<float> column_weights{};
    float column_spacing{8.0f};
    float row_spacing{8.0f};
    float cell_height{-1.0f};
    EdgeInsets padding{EdgeInsets::zero()};
    std::vector<WidgetPtr> content{};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class GridWidget : public Widget {
public:
    explicit GridWidget(const Grid& desc) : m_desc(desc) {}
    explicit GridWidget(Grid&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderGrid>(
            m_desc.columns, m_desc.min_item_width, m_desc.column_weights,
            m_desc.column_spacing, m_desc.row_spacing, m_desc.cell_height, m_desc.padding);
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
        if (auto grid = std::static_pointer_cast<RenderGrid>(node)) {
            grid->set_columns(m_desc.columns);
            grid->set_min_item_width(m_desc.min_item_width);
            grid->set_column_weights(m_desc.column_weights);
            grid->set_column_spacing(m_desc.column_spacing);
            grid->set_row_spacing(m_desc.row_spacing);
            grid->set_cell_height(m_desc.cell_height);
            grid->set_padding(m_desc.padding);
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
            grid->set_child_boxes(m_child_boxes);
        }
    }

private:
    Grid m_desc;
    std::vector<std::shared_ptr<RenderBox>> m_child_boxes;
};

inline Grid::operator WidgetPtr() const& { return make_widget<GridWidget>(*this); }
inline Grid::operator WidgetPtr() && { return make_widget<GridWidget>(std::move(*this)); }

} // namespace ktui

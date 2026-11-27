#include "../gallery_pages.hpp"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

// 轻量辅助：捕获任意子控件创建后的 RenderBox 弱引用，供 SpotlightTour 实时定位真实目标控件
class RefCaptureWidget : public Widget {
public:
    RefCaptureWidget(WidgetPtr inner, std::shared_ptr<std::weak_ptr<RenderBox>> slot)
        : m_inner(std::move(inner)), m_slot(std::move(slot)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = m_inner ? m_inner->create_render_node() : nullptr;
        if (m_inner && node) m_inner->set_render_node(node);
        if (m_slot) *m_slot = node;
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (m_slot) *m_slot = node;
        if (m_inner) m_inner->update_render_node(std::move(node));
    }

private:
    WidgetPtr m_inner;
    std::shared_ptr<std::weak_ptr<RenderBox>> m_slot;
};

WidgetPtr with_ref(WidgetPtr child, std::shared_ptr<std::weak_ptr<RenderBox>> slot) {
    return make_widget<RefCaptureWidget>(std::move(child), std::move(slot));
}

} // namespace

// ============================================================================
// 1. 结构化查询构建器 (QueryFilterBuilder)
// ============================================================================
WidgetPtr build_query_filter_page(GalleryApp& app) {
    auto expr_text = std::make_shared<Property<std::string>>("当前查询表达式: status:open priority:high");
    auto ref_slot = std::make_shared<std::weak_ptr<RenderBox>>();

    std::vector<QueryFilterField> schema = {
        QueryFilterField{ "status",   "工单状态", { "open", "in_progress", "review", "closed" }, Color{0.10f, 0.65f, 0.95f, 1.0f} },
        QueryFilterField{ "priority", "优先级",   { "urgent", "high", "medium", "low" },          Color{0.94f, 0.45f, 0.14f, 1.0f} },
        QueryFilterField{ "owner",    "负责人",   { "alice", "bob", "charlie", "me" },            Color{0.55f, 0.42f, 0.96f, 1.0f} },
        QueryFilterField{ "env",      "部署环境", { "prod", "staging", "dev" },                   Color{0.12f, 0.72f, 0.45f, 1.0f} }
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("QueryFilterBuilder (结构化查询构建器)", 24.0f),
            app.make_muted("Linear / Jira / GitHub Issues 风格单行分词筛选框：输入 status: 自动弹出枚举下拉，选中化为胶囊，支持方向键/退格逐个删除与纯文本表达式双向切换。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    with_ref(QueryFilterBuilder {
                        .schema = schema,
                        .initial_tokens = {
                            QueryFilterToken{ "status", "open" },
                            QueryFilterToken{ "priority", "high" }
                        },
                        .placeholder = "输入 status: / priority: / owner: 或点击展开字段建议...",
                        .on_changed = [expr_text](const std::vector<QueryFilterToken>& tokens, const std::string& expr) {
                            expr_text->set("已匹配 " + std::to_string(tokens.size()) + " 个条件  |  表达式: " + (expr.empty() ? "(空)" : expr));
                        }
                    }, ref_slot),

                    app.make_accent(*expr_text, 12.5f, FontWeight::Medium),

                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "+ env:prod",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->add_token("env", "prod");
                                }
                            },
                            Button {
                                .text = "+ owner:alice",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->add_token("owner", "alice");
                                }
                            },
                            Button {
                                .text = "切换 胶囊 / 表达式",
                                .variant = ButtonVariant::Accent,
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->toggle_raw_mode();
                                }
                            },
                            Button {
                                .text = "清空筛选",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderQueryFilterBuilder*>(b.get())->clear_all();
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

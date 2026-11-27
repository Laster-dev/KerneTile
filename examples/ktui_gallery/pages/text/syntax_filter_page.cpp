#include "../gallery_pages.hpp"
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

class GeekRefCaptureWidget : public Widget {
public:
    GeekRefCaptureWidget(WidgetPtr inner, std::shared_ptr<std::weak_ptr<RenderBox>> slot)
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

WidgetPtr capture_box(WidgetPtr w, std::shared_ptr<std::weak_ptr<RenderBox>> slot) {
    return make_widget<GeekRefCaptureWidget>(std::move(w), std::move(slot));
}

} // namespace

// ============================================================================
// 4. 语法感知过滤输入框 (SyntaxFilterBar)
// ============================================================================
WidgetPtr build_syntax_filter_page(GalleryApp& app) {
    auto diag_text = std::make_shared<Property<std::string>>("✓ 语法合法，表达式可直接应用");
    auto ref = std::make_shared<std::weak_ptr<RenderBox>>();

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("SyntaxFilterBar (语法感知过滤输入框)", 24.0f),
            app.make_muted("直接继承单一 RenderTextBox：Wireshark 风格底色状态灯（合法浅绿 / 警告浅黄 / 语法错误浅红），支持字段名 Tab 自动补全。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    capture_box(SyntaxFilterBar {
                        .text = "ip == 10.0.0.1 && port > 1024",
                        .on_evaluated = [diag_text](const std::string&, SyntaxFilterState, const std::string& diag) {
                            diag_text->set(diag);
                        }
                    }, ref),
                    app.make_accent(*diag_text, 12.5f, FontWeight::Medium),
                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "合法示例 (绿)",
                                .on_click = [ref]() {
                                    if (auto b = ref->lock()) static_cast<RenderSyntaxFilterBar*>(b.get())->set_expression("ip == 10.0.0.1 && port >= 443");
                                }
                            },
                            Button {
                                .text = "警告示例 (黄)",
                                .on_click = [ref]() {
                                    if (auto b = ref->lock()) static_cast<RenderSyntaxFilterBar*>(b.get())->set_expression("custom_tag == prod && port > 80");
                                }
                            },
                            Button {
                                .text = "错误示例 (红)",
                                .on_click = [ref]() {
                                    if (auto b = ref->lock()) static_cast<RenderSyntaxFilterBar*>(b.get())->set_expression("(ip == 10.0.0.1 &&");
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

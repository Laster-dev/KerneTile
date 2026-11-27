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
// 4. 增量式幽灵文本补全 (GhostTextBox)
// ============================================================================
WidgetPtr build_ghost_text_box_page(GalleryApp& app) {
    auto status_text = std::make_shared<Property<std::string>>("提示: 尝试输入 \"SELECT \"、\"git \" 或 \"kubectl \"，然后按 Tab 键一键上屏（按 Esc 忽略）");
    auto ref_slot = std::make_shared<std::weak_ptr<RenderBox>>();

    std::vector<std::string> suggestions = {
        "SELECT id, status, latency_ms FROM requests WHERE latency_ms > 200 ORDER BY id DESC;",
        "git checkout -b feat/fluent-dock-panel",
        "git commit -m \"feat: add ghost text autocomplete\"",
        "kubectl get pods -n production --watch",
        "docker compose up -d --build"
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("GhostTextBox (增量式幽灵文本补全)", 24.0f),
            app.make_muted("GitHub Copilot 风格行内半透明补全提示：每次按 Tab 仅补全到下一个空格（逐词补全），方便随时改写后续参数，按 Esc 忽略。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    with_ref(GhostTextBox {
                        .text = "SELECT ",
                        .placeholder = "输入 SELECT / git / kubectl / docker 体验行内幽灵补全...",
                        .suggestions = suggestions,
                        .on_changed = [status_text](const std::string& val) {
                            status_text->set("当前输入: " + (val.empty() ? "(空)" : val));
                        },
                        .on_accepted = [status_text](const std::string& val) {
                            status_text->set("✓ 已按 Tab 接受幽灵补全: " + val);
                        }
                    }, ref_slot),

                    app.make_accent(*status_text, 12.5f, FontWeight::Medium),

                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "试试 \"SELECT \"",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->set_text("SELECT ", true);
                                }
                            },
                            Button {
                                .text = "试试 \"git ch\"",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->set_text("git ch", true);
                                }
                            },
                            Button {
                                .text = "试试 \"kubectl \"",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->set_text("kubectl ", true);
                                }
                            },
                            Button {
                                .text = "Tab 逐词补全",
                                .variant = ButtonVariant::Accent,
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderGhostTextBox*>(b.get())->accept_next_word();
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

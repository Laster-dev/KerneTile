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
// 5. 结构化分段式状态栏 (StatusBar)
// ============================================================================
WidgetPtr build_status_bar_page(GalleryApp& app) {
    auto status = std::make_shared<Property<std::string>>("点击状态栏右侧的 UTF-8 / LF / 1000ms 槽位，直接弹出 MenuFlyout 切换配置！");
    auto ref = std::make_shared<std::weak_ptr<RenderBox>>();
    auto sel_cnt = std::make_shared<int>(3);

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("StatusBar (结构化分段式状态栏)", 24.0f),
            app.make_muted("经典 24~26px 工程底部状态栏：左侧常驻选中统计与偏移信息，右侧放编码、换行符、刷新频率等可点击切换槽位（复用 MenuFlyout）。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 14.0f,
                .content = {
                    capture_box(StatusBar {
                        .slots = {
                            StatusBarSlot{"conn",   "已连接 D3D12",         false, Color{0.10f, 0.72f, 0.45f, 1.0f}},
                            StatusBarSlot{"stats",  "已选 3 项 / 共 14,208 项", false},
                            StatusBarSlot{"offset", "偏移 0x004F",          false},
                            StatusBarSlot{"enc",    "UTF-8",                true,  {}, {"UTF-8", "UTF-16 LE", "GBK", "ASCII"},
                                [status](const std::string& v) { status->set("已切换编码格式: " + v); }},
                            StatusBarSlot{"eol",    "LF",                   true,  {}, {"LF", "CRLF", "CR"},
                                [status](const std::string& v) { status->set("已切换换行符: " + v); }},
                            StatusBarSlot{"rate",   "1000ms",               true,  {}, {"100ms", "500ms", "1000ms", "5000ms"},
                                [status](const std::string& v) { status->set("已切换刷新频率: " + v); }}
                        }
                    }, ref),
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            app.make_accent(*status, 12.5f, FontWeight::Medium),
                            Button {
                                .text = "模拟选中 +1",
                                .on_click = [ref, sel_cnt]() {
                                    ++(*sel_cnt);
                                    if (auto b = ref->lock()) {
                                        static_cast<RenderStatusBar*>(b.get())->set_slot_text(
                                            "stats", "已选 " + std::to_string(*sel_cnt) + " 项 / 共 14,208 项");
                                    }
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

#include "../gallery_pages.hpp"
#include <cmath>
#include <cstdio>

using namespace ktui;

namespace gallery::pages {

namespace {

WidgetPtr page_header(GalleryApp& app, const std::string& title, const std::string& subtitle) {
    return VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 4.0f,
        .content = {
            app.make_page_title(title, 22.0f),
            Text { .text = subtitle, .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
        }
    };
}

} // namespace

// ==================== RadioButton ====================
WidgetPtr build_radio_button_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "RadioButton", "经典 WinUI 3 圆形单选按钮组，像素对齐同心圆点与禁用项支持。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "渲染策略单选组", .font_size = 14.0f },
                    RadioGroup {
                        .options = {
                            RadioOption { .label = "选项 A — 推荐默认配置", .value = 0 },
                            RadioOption { .label = "选项 B — 高刷新率优先 (120 FPS)", .value = 1 },
                            RadioOption { .label = "选项 C — 极简省电模式", .value = 2 },
                            RadioOption { .label = "选项 D — 已禁用项", .value = 3, .disabled = true }
                        },
                        .spacing = 6.0f
                    }.bind_two_way(app.radio_btn_val)
                }
            })
        }
    });
}

} // namespace gallery::pages

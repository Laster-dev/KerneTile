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

// ==================== ToggleSwitch ====================
WidgetPtr build_toggle_switch_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ToggleSwitch", "WinUI 3 拨动开关，支持物理弹簧滑动胶囊与按压形变。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "状态开关示例", .font_size = 14.0f },
                    ToggleSwitch {
                        .label = "锁定计数器按钮 (联动 Button 页面)",
                        .on_change = [&app](bool v) {
                            app.locked.set(v);
                            app.refresh_count();
                        }
                    }.bind_two_way(app.locked),
                    ToggleSwitch {
                        .checked = true,
                        .label = "开启且禁用",
                        .disabled = true
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

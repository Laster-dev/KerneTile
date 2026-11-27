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

// ==================== CheckBox ====================
WidgetPtr build_checkbox_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "CheckBox", "WinUI 3 复选框，支持对勾平滑生长动画、悬停态与禁用态。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "基础复选框", .font_size = 14.0f },
                    Checkbox {
                        .label = "每次递增步长设为 +10 (联动 Button 页面计数器)",
                        .on_change = [&app](bool v) {
                            app.step_ten.set(v);
                            app.refresh_count();
                        }
                    }.bind_two_way(app.step_ten),
                    Checkbox {
                        .checked = true,
                        .label = "已选中且禁用",
                        .disabled = true
                    },
                    Checkbox {
                        .checked = false,
                        .label = "未选中且禁用",
                        .disabled = true
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

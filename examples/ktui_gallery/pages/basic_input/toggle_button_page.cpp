#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_toggle_button_page(GalleryApp& app) {
    auto refresh_fmt = [&app]() {
        std::string s;
        if (app.tb_bold.get()) s += "粗体 ";
        if (app.tb_italic.get()) s += "斜体 ";
        if (app.tb_underline.get()) s += "下划线 ";
        app.tb_status.set("当前格式: " + (s.empty() ? "无" : s));
    };
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("ToggleButton", 22.0f),
            app.make_muted("WinUI 3 切换按钮，可保持选中主题色高亮态，适用于富文本格式或工具栏模式开关。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "文本格式切换组", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            ToggleButton { .text = "B 粗体", .on_change = [refresh_fmt](bool) { refresh_fmt(); } }.bind_two_way(app.tb_bold),
                            ToggleButton { .text = "I 斜体", .on_change = [refresh_fmt](bool) { refresh_fmt(); } }.bind_two_way(app.tb_italic),
                            ToggleButton { .text = "U 下划线", .on_change = [refresh_fmt](bool) { refresh_fmt(); } }.bind_two_way(app.tb_underline),
                            ToggleButton { .checked = false, .text = "已禁用", .disabled = true }
                        }
                    },
                    app.make_accent(app.tb_status, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

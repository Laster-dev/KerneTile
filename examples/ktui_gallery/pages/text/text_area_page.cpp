#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_text_area_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TextArea", 22.0f),
            app.make_muted("WinUI 3 多行文本编辑器，支持回车换行、自动折行、多行光标上下左右定位、剪贴板、中文 IME 与内置垂直滚动条。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    TextArea {
                        .placeholder = "在此输入多行内容...",
                        .header = "文档备注",
                        .height = 140.0f,
                        .on_change = [&app](const std::string& v) {
                            app.textarea_status.set("当前字符数: " + std::to_string(v.size()) + " 字节");
                        }
                    }.bind_two_way(app.textarea_val),
                    app.make_accent(app.textarea_status, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

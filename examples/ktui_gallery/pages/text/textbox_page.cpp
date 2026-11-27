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

// ==================== TextBox / TextInput ====================
WidgetPtr build_textbox_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "TextBox", "单行文本输入框，支持完整中文 IME 组合输入、选区高亮、剪贴板与撤销/重做。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "响应式双向绑定文本输入", .font_size = 14.0f },
                    TextInput {
                        .placeholder = "键入中文或英文...",
                        .on_change = [&app](const std::string& val) {
                            app.input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                        }
                    }.bind_two_way(app.input_text),
                    Text {}.bind(&RenderText::set_text, app.input_preview).bind(&RenderText::set_color, app.accent_text)
                }
            })
        }
    });
}

} // namespace gallery::pages

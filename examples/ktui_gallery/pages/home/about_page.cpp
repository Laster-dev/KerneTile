#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {

static constexpr const char* kIconWindow =
    "M4.5 4h15A1.5 1.5 0 0 1 21 5.5v13a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5v-13A1.5 1.5 0 0 1 4.5 4Zm0 4v10.5h15V8h-15Z";

static constexpr const char* kIconInfo =
    "M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20Zm0 5a1.25 1.25 0 1 1 0 2.5A1.25 1.25 0 0 1 12 7Zm-1 4.5h2v6h-2v-6Z";

} // namespace

WidgetPtr build_about_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 6.0f,
        .content = {
            Text { .text = "关于 KerneTile KTUI Gallery", .font_size = 24.0f, .font_weight = FontWeight::SemiBold },
            Box { .height = 6.0f },
            SettingsCard {
                .header = "KerneTile 引擎与 KTUI 框架",
                .description = "2D 瓦片计算渲染引擎 + C++20 声明式桌面 UI 框架，零异常零 RTTI 极致体积",
                .header_icon_svg = kIconInfo,
                .content = Text { .text = "v1.2.0", .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
            },
            SettingsCard {
                .header = "开源仓库",
                .description = "https://github.com/Laster-dev/KerneTile",
                .header_icon_svg = kIconWindow,
                .is_click_enabled = true,
                .on_click = []() {
                    Clipboard::set_text("https://github.com/Laster-dev/KerneTile");
                    Toast::show(ToastDesc{ .title = "已复制仓库链接", .message = "https://github.com/Laster-dev/KerneTile", .severity = ToastSeverity::Success });
                }
            }
        }
    });
}

} // namespace gallery::pages

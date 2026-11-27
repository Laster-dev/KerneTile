#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_pips_pager_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("PipsPager", 22.0f),
            app.make_muted("WinUI 3 圆点分页指示器，切换时当前圆点平滑放大并显示页码数字，1 秒后数字淡出、圆点平滑缩回。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "12 页滑动窗口演示 (MaxVisiblePips = 7)", .font_size = 14.0f },
                    PipsPager {
                        .number_of_pages = 12,
                        .max_visible_pips = 7,
                        .show_nav_buttons = true
                    }.bind_two_way(app.pips_page)
                }
            })
        }
    });
}

} // namespace gallery::pages

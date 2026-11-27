#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_person_picture_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("PersonPicture", 22.0f),
            app.make_muted("WinUI 3 用户头像组件，支持中英文姓名首字母自动提取、确定性 Fluent 配色、多尺寸规范与在线状态指示圆点。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "多尺寸头像与在线状态指示点", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 18.0f,
                        .content = {
                            PersonPicture { .display_name = "Jane Doe", .diameter = 64.0f, .presence = PersonPresence::Available },
                            PersonPicture { .display_name = "林思远", .diameter = 52.0f, .presence = PersonPresence::Busy },
                            PersonPicture { .display_name = "Alex Chen", .diameter = 44.0f, .presence = PersonPresence::Away },
                            PersonPicture { .display_name = "KerneTile", .diameter = 36.0f, .presence = PersonPresence::Offline }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

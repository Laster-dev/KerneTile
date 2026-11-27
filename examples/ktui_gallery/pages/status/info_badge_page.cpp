#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_info_badge_page(GalleryApp& app) {
    static constexpr const char* kStarSvg =
        "M12 2.5l2.84 5.75 6.35.92-4.6 4.48 1.09 6.32L12 16.98l-5.68 2.99 1.09-6.32-4.6-4.48 6.35-.92L12 2.5z";
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("InfoBadge", 22.0f),
            app.make_muted("WinUI 3 状态角标，提供数字（支持 99+ 溢出）、圆点与矢量图标三种形态，覆盖 5 种 Fluent 2 语义色。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "动态数字角标与 5 种语义形态", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            InfoBadge { .severity = InfoBadgeSeverity::Attention }.bind(&RenderInfoBadge::set_value, app.badge_count),
                            InfoBadge { .value = 128, .severity = InfoBadgeSeverity::Critical },
                            InfoBadge { .value = -1, .severity = InfoBadgeSeverity::Success },
                            InfoBadge { .value = -1, .severity = InfoBadgeSeverity::Caution, .svg_path = kStarSvg },
                            InfoBadge { .value = 7, .severity = InfoBadgeSeverity::Informational }
                        }
                    },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "未读数 +1",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() { app.badge_count.set(app.badge_count.get() + 1); }
                            },
                            Button {
                                .text = "设为 105 (触发 99+)",
                                .on_click = [&app]() { app.badge_count.set(105); }
                            },
                            Button {
                                .text = "清零",
                                .on_click = [&app]() { app.badge_count.set(0); }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

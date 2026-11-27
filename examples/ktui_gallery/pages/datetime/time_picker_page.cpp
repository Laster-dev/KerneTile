#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_time_picker_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TimePicker", 22.0f),
            app.make_muted("WinUI 3 时间选择器，点击弹出固定尺寸滚轮选时浮层，支持 24h / 12h AM·PM 模式、滚轮滚动步进与分钟步长配置。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "24 小时制与 12 小时制对比", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::FlexEnd,
                        .spacing = 16.0f,
                        .content = {
                            TimePicker {
                                .header = "会议开始时间 (24h)",
                                .use_24h = true,
                                .minute_increment = 5,
                                .width = 200.0f,
                                .on_time_changed = [&app](const TimeOfDay& t) {
                                    app.time_status.set("当前选择: " + t.format(true) + " (24h) / " + app.picked_time_12.get().format(false) + " (12h)");
                                }
                            }.bind_two_way(app.picked_time_24),
                            TimePicker {
                                .header = "提醒时间 (12h AM/PM)",
                                .use_24h = false,
                                .minute_increment = 15,
                                .width = 210.0f,
                                .on_time_changed = [&app](const TimeOfDay& t) {
                                    app.time_status.set("当前选择: " + app.picked_time_24.get().format(true) + " (24h) / " + t.format(false) + " (12h)");
                                }
                            }.bind_two_way(app.picked_time_12)
                        }
                    },
                    app.make_accent(app.time_status, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

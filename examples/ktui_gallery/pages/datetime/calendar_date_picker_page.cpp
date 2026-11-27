#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_calendar_date_picker_page(GalleryApp& app) {
    CalendarDate today = CalendarDate::today();
    int end_day = std::min(today.day + 5, CalendarDate::days_in_month(today.year, today.month));
    CalendarDate init_end{today.year, today.month, end_day};
    auto range_status = std::make_shared<Property<std::string>>(
        "已选区间: " + today.format_iso() + " ~ " + init_end.format_iso());

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("CalendarDatePicker", 22.0f),
            app.make_muted("WinUI 3 日历日期选择器：带滑入弹出动画、滚轮/翻页平滑切月、单日与日期区间选择（含悬停实时预览连带）。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "双向绑定单日选择", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::FlexEnd,
                        .spacing = 12.0f,
                        .content = {
                            CalendarDatePicker {
                                .placeholder = "请选择日期",
                                .header = "发布日期",
                                .width = 240.0f,
                                .on_date_changed = [&app](const CalendarDate& d) {
                                    app.date_status.set("当前选中日期: " + d.format_iso());
                                }
                            }.bind_two_way(app.picked_date),
                            Button {
                                .text = "回到今天",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app, today]() {
                                    app.picked_date.set(today);
                                    app.date_status.set("当前选中日期: " + today.format_iso());
                                }
                            }
                        }
                    },
                    app.make_accent(app.date_status, 12.5f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "日期区间选择 (Date Range)", .font_size = 14.0f },
                    CalendarDatePicker {
                        .placeholder = "请选择起止日期",
                        .header = "迭代冲刺周期 (点击起止两日，支持滚轮切月)",
                        .width = 280.0f,
                        .range_mode = true,
                        .start_date = today,
                        .end_date = init_end,
                        .on_range_changed = [range_status](const CalendarDate& s, const CalendarDate& e) {
                            range_status->set("已选区间: " + s.format_iso() + " ~ " + e.format_iso());
                        }
                    },
                    Text { .font_size = 12.5f }.bind(&RenderText::set_text, *range_status).bind(&RenderText::set_color, app.accent_text)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "限定可选范围 (仅限本月内)", .font_size = 14.0f },
                    CalendarDatePicker {
                        .date = today,
                        .header = "本月排期窗口",
                        .min_date = CalendarDate{today.year, today.month, 1},
                        .max_date = CalendarDate{today.year, today.month, CalendarDate::days_in_month(today.year, today.month)},
                        .width = 240.0f
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

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

// ==================== NumberBox ====================
WidgetPtr build_number_box_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "NumberBox", "WinUI 3 数值输入框，支持内嵌/紧凑步进按钮、方向键/滚轮微调与算术表达式自动求值。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "Inline 步进与表达式求值", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            NumberBox {
                                .minimum = 0.0,
                                .maximum = 500.0,
                                .small_change = 1.0,
                                .large_change = 10.0,
                                .precision = 0,
                                .accepts_expression = true,
                                .spin_button_placement = NumberBoxSpinButtonPlacementMode::Inline,
                                .header = "整数 [0..500] (可输入 12+8*2)",
                                .width = 220.0f,
                                .on_value_changed = [&app](double v) {
                                    char buf[128];
                                    std::snprintf(buf, sizeof(buf), "当前数值: %.0f / %.2f (支持方向键、PageUp/Down、滚轮与表达式)",
                                                  v, app.number_float_val.get());
                                    app.number_status.set(buf);
                                }
                            }.bind_two_way(app.number_int_val),
                            NumberBox {
                                .minimum = -100.0,
                                .maximum = 100.0,
                                .small_change = 0.25,
                                .large_change = 2.0,
                                .precision = 2,
                                .accepts_expression = true,
                                .spin_button_placement = NumberBoxSpinButtonPlacementMode::Compact,
                                .header = "浮点步进 (Compact 悬停显示)",
                                .width = 220.0f,
                                .on_value_changed = [&app](double v) {
                                    char buf[128];
                                    std::snprintf(buf, sizeof(buf), "当前数值: %.0f / %.2f (支持方向键、PageUp/Down、滚轮与表达式)",
                                                  app.number_int_val.get(), v);
                                    app.number_status.set(buf);
                                }
                            }.bind_two_way(app.number_float_val)
                        }
                    },
                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.number_status).bind(&RenderText::set_color, app.muted_text)
                }
            })
        }
    });
}

} // namespace gallery::pages

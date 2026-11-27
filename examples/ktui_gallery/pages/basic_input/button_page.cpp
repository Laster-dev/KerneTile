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

// ==================== Button ====================
WidgetPtr build_button_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Button", "标准、强调色、危险操作及禁用态按钮，支持弹簧按压微动效与焦点键盘激活。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "交互式计数器与按钮变体", .font_size = 14.0f },
                    Text { .font_size = 15.0f }.bind(&RenderText::set_text, app.count_text),
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Tooltip {
                                .text = "按步长递增计数 (支持 Tab 焦点与 Space/Enter 激活)",
                                .child = Button {
                                    .text = "递增计数 (+)",
                                    .variant = ButtonVariant::Accent,
                                    .on_click = [&app]() {
                                        app.count += app.step_ten.get() ? 10 : 1;
                                        app.refresh_count();
                                    }
                                }.bind(&RenderButton::set_disabled, app.locked)
                            },
                            Tooltip {
                                .text = "将当前计数清零重置",
                                .child = Button {
                                    .text = "归零重置",
                                    .variant = ButtonVariant::Danger,
                                    .on_click = [&app]() {
                                        app.count = 0;
                                        app.refresh_count();
                                    }
                                }.bind(&RenderButton::set_disabled, app.locked)
                            },
                            Button {
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    app.app->set_theme(app.app->current_theme().is_dark ? Theme::light() : Theme::dark());
                                }
                            }.bind(&RenderButton::set_text, app.theme_btn_text),
                            Button {
                                .text = "已禁用按钮",
                                .variant = ButtonVariant::Standard,
                                .disabled = true
                            }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = {
                    Text { .text = "多窗口派生示例", .font_size = 14.0f },
                    app.make_muted("主窗口与子窗口共享响应式状态与主题切换。", 12.0f),
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "新建独立工具窗口",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() { app.spawn_child_window(); }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

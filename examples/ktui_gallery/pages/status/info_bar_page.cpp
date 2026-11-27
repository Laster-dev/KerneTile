#include "../gallery_pages.hpp"

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

// ==================== InfoBar ====================
WidgetPtr build_info_bar_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "InfoBar", "WinUI 3 内联状态通知条，支持 4 种语义级别、操作按钮与平滑可关闭交互。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Text { .text = "可交互 InfoBar (支持关闭与切换语义):", .font_size = 13.5f },
                            Button {
                                .text = "切换级别 / 重新显示",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    int next = (static_cast<int>(app.infobar_severity.get()) + 1) % 4;
                                    app.infobar_severity.set(static_cast<InfoBarSeverity>(next));
                                    app.infobar_open.set(true);
                                }
                            }
                        }
                    },
                    InfoBar {
                        .title = "系统通知",
                        .message = "此 InfoBar 支持关闭按钮与自定义操作按钮，点击上方按钮可切换 4 种语义级别。",
                        .is_closable = true,
                        .action_text = "查看详情",
                        .on_action = [&app]() { app.dialog_status.set("InfoBar 操作按钮已点击"); },
                        .on_close = [&app]() { app.infobar_open.set(false); }
                    }.bind(&RenderInfoBar::set_severity, app.infobar_severity)
                     .bind_two_way(app.infobar_open),
                    InfoBar {
                        .title = "成功",
                        .message = "NanoSVG 矢量图集、Chromium 平滑滚动与跨平台触控手势已就绪。",
                        .severity = InfoBarSeverity::Success,
                        .is_closable = false
                    },
                    InfoBar {
                        .title = "警告",
                        .message = "检测到高刷新率显示器，已自动启用 120 FPS 高精度定时器。",
                        .severity = InfoBarSeverity::Warning,
                        .is_closable = false
                    },
                    InfoBar {
                        .title = "错误",
                        .message = "连接远程渲染节点超时，已自动回退至本地 GPU 计算后端。",
                        .severity = InfoBarSeverity::Error,
                        .is_closable = false
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

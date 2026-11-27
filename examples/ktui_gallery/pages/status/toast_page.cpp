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

// ==================== Toast ====================
WidgetPtr build_toast_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Toast", "Windows 11 悬浮通知吐司，支持高斯软阴影、滑入/滑出动效、底部倒计时进度条（悬停自动暂停）与多条自动堆叠。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "触发不同语义与方位的 Toast 通知", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "信息通知 (右下角)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "系统消息",
                                        .message = "KerneTile 云母与触控手势已启用。",
                                        .severity = ToastSeverity::Informational,
                                        .placement = ToastPlacement::BottomRight,
                                        .duration_sec = 4.0f,
                                        .action_text = "查看",
                                        .on_action = [&app]() { app.toast_status.set("已点击 Toast 操作按钮: 查看"); }
                                    });
                                    app.toast_status.set("已弹出 Informational Toast (右下角)");
                                }
                            },
                            Button {
                                .text = "成功通知",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "保存成功",
                                        .message = "KTUI Gallery 配置已写入本地缓存。",
                                        .severity = ToastSeverity::Success,
                                        .placement = ToastPlacement::BottomRight,
                                        .duration_sec = 3.5f
                                    });
                                    app.toast_status.set("已弹出 Success Toast");
                                }
                            },
                            Button {
                                .text = "警告通知 (右上角)",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "性能提示",
                                        .message = "当前窗口正在以 120 FPS 高刷运行。",
                                        .severity = ToastSeverity::Warning,
                                        .placement = ToastPlacement::TopRight,
                                        .duration_sec = 4.0f
                                    });
                                    app.toast_status.set("已弹出 Warning Toast (右上角)");
                                }
                            },
                            Button {
                                .text = "错误通知 (底部居中)",
                                .variant = ButtonVariant::Danger,
                                .on_click = [&app]() {
                                    Toast::show(ToastDesc{
                                        .title = "连接超时",
                                        .message = "远端同步服务未响应，请稍后重试。",
                                        .severity = ToastSeverity::Error,
                                        .placement = ToastPlacement::BottomCenter,
                                        .duration_sec = 4.5f,
                                        .action_text = "重试",
                                        .on_action = [&app]() { app.toast_status.set("已点击 Toast 操作按钮: 重试"); }
                                    });
                                    app.toast_status.set("已弹出 Error Toast (底部居中)");
                                }
                            }
                        }
                    },
                    app.make_accent(app.toast_status, 12.0f)
                }
            })
        }
    });
}

} // namespace gallery::pages

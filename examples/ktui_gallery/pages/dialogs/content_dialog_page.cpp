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

// ==================== ContentDialog ====================
WidgetPtr build_content_dialog_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ContentDialog", "WinUI 3 模态对话框，具备 1.05x->1.00x 缩放弹出动效、64dp 高斯软阴影与平滑 Smoke 遮罩层。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "模态对话框演示", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Button {
                                .text = "弹出保存确认框 (3 按钮)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    ContentDialog::show(ContentDialogDesc{
                                        .title = "保存更改?",
                                        .content = "您对当前 KTUI Gallery 配置进行了修改，是否在离开前保存更改？按 Esc 或点击取消可关闭弹窗。",
                                        .primary_button_text = "保存",
                                        .secondary_button_text = "不保存",
                                        .close_button_text = "取消",
                                        .width = 430.0f,
                                        .on_result = [&app](ContentDialogResult res) {
                                            switch (res) {
                                                case ContentDialogResult::Primary:   app.dialog_status.set("对话框返回: Primary (已保存)"); break;
                                                case ContentDialogResult::Secondary: app.dialog_status.set("对话框返回: Secondary (放弃更改)"); break;
                                                case ContentDialogResult::Close:     app.dialog_status.set("对话框返回: Close (取消)"); break;
                                                default:                             app.dialog_status.set("对话框返回: Esc 取消"); break;
                                            }
                                        }
                                    });
                                }
                            },
                            Button {
                                .text = "弹出信息提示框 (单按钮)",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    ContentDialog::show(ContentDialogDesc{
                                        .title = "渲染引擎就绪",
                                        .content = "KerneTile GPU 计算着色器、NanoSVG 矢量管线与跨平台触控手势已全部启用。",
                                        .primary_button_text = "知道了",
                                        .width = 390.0f,
                                        .on_result = [&app](ContentDialogResult) {
                                            app.dialog_status.set("信息对话框已确认关闭");
                                        }
                                    });
                                }
                            }
                        }
                    },
                    app.make_accent(app.dialog_status, 12.0f)
                }
            })
        }
    });
}

} // namespace gallery::pages

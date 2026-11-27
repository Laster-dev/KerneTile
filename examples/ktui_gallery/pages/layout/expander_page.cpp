#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {

constexpr const char* kCopySvg =
    "M5.5 4.625V16.5a2.5 2.5 0 0 0 2.5 2.5h7.375a2.25 2.25 0 0 1-2.125 1.5H8a4 4 0 0 1-4-4V6.75A2.25 2.25 0 0 1 5.5 4.625ZM9.25 2A2.25 2.25 0 0 0 7 4.25v11.5A2.25 2.25 0 0 0 9.25 18h7.5A2.25 2.25 0 0 0 19 15.75V4.25A2.25 2.25 0 0 0 16.75 2h-7.5ZM8.5 4.25a.75.75 0 0 1 .75-.75h7.5a.75.75 0 0 1 .75.75v11.5a.75.75 0 0 1-.75.75h-7.5a.75.75 0 0 1-.75-.75V4.25Z";

constexpr const char* kSaveSvg =
    "M5.25 3A2.25 2.25 0 0 0 3 5.25v13.5A2.25 2.25 0 0 0 5.25 21h13.5A2.25 2.25 0 0 0 21 18.75V7.96a2.25 2.25 0 0 0-.659-1.591l-2.71-2.71A2.25 2.25 0 0 0 16.04 3H5.25ZM4.5 5.25a.75.75 0 0 1 .75-.75H7v4.25A1.75 1.75 0 0 0 8.75 10.5h5.5A1.75 1.75 0 0 0 16 8.75V4.5h.04a.75.75 0 0 1 .53.22l2.71 2.71a.75.75 0 0 1 .22.53v10.79a.75.75 0 0 1-.75.75H17v-5.25A1.75 1.75 0 0 0 15.25 12.5h-6.5A1.75 1.75 0 0 0 7 14.25v5.25H5.25a.75.75 0 0 1-.75-.75V5.25Zm4 9a.25.25 0 0 1 .25-.25h6.5a.25.25 0 0 1 .25.25v5.25h-7v-5.25ZM8.5 4.5h6v4.25a.25.25 0 0 1-.25.25h-5.5a.25.25 0 0 1-.25-.25V4.5Z";

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

// ==================== Expander ====================
WidgetPtr build_expander_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Expander", "WinUI 3 折叠展开卡片：标题栏带旋转 180° 箭头与平滑高度过渡动效，适合设置项与高级选项分组。"),
            Expander {
                .header = "渲染管线与帧率策略",
                .subtitle = "配置 GPU 计算着色器、高刷定时器与脏瓦片合批选项",
                .svg_path = kSaveSvg,
                .content = VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 10.0f,
                    .content = {
                        app.make_muted("展开后支持内嵌任意交互控件（开关、按钮、输入框等）：", 12.5f),
                        ToggleSwitch { .label = "启用 120 FPS 高精度等待定时器" }.bind_two_way(app.step_ten),
                        HStack {
                            .align_items = AlignItems::Center,
                            .spacing = 8.0f,
                            .content = {
                                Label { .text = "D3D12 Compute", .variant = LabelVariant::Accent, .pill = true },
                                Label { .text = "16×16 Tile Binning", .variant = LabelVariant::Success, .pill = true }
                            }
                        }
                    }
                }
            }.bind_two_way(app.expander_open_1),
            Expander {
                .header = "高级开发者诊断选项",
                .subtitle = "默认折叠状态，点击整行标题栏即可平滑展开",
                .svg_path = kCopySvg,
                .content = VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 8.0f,
                    .content = {
                        app.make_muted("这里可以放置低频诊断参数、日志级别或实验性功能开关。", 12.5f),
                        Button {
                            .text = "触发一条诊断 Toast",
                            .variant = ButtonVariant::Standard,
                            .on_click = []() {
                                Toast::show(ToastDesc{
                                    .title = "诊断就绪",
                                    .message = "Expander 内部按钮交互响应正常。",
                                    .severity = ToastSeverity::Success,
                                    .duration_sec = 3.0f
                                });
                            }
                        }
                    }
                }
            }.bind_two_way(app.expander_open_2)
        }
    });
}

} // namespace gallery::pages

#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_tooltip_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Tooltip", 22.0f),
            app.make_muted("WinUI 3 悬浮提示浮层，悬停后经可配置延迟平滑淡入，自动上下防溢出翻转，且点击穿透不阻断交互。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "基础按钮与不同延迟配置 (将鼠标悬停于控件上方)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Tooltip {
                                .text = "标准 WinUI 3 ToolTip (默认 400ms 延迟)",
                                .child = Button { .text = "标准提示 (400ms)", .variant = ButtonVariant::Accent }
                            },
                            Tooltip {
                                .text = "极速触发提示 (120ms 延迟)",
                                .delay_sec = 0.12f,
                                .child = Button { .text = "快速提示 (120ms)", .variant = ButtonVariant::Standard }
                            },
                            Tooltip {
                                .text = "慢速长悬停提示 (800ms 延迟)",
                                .delay_sec = 0.80f,
                                .child = Button { .text = "慢速提示 (800ms)", .variant = ButtonVariant::Standard }
                            }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "为标签、切换按钮与表单项挂载语义提示", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 12.0f,
                        .content = {
                            Tooltip {
                                .text = "当前 GPU 渲染管线已开启 16×16 瓦片脏区剔除",
                                .delay_sec = 0.25f,
                                .child = Label { .text = "渲染状态: 正常", .variant = LabelVariant::Success, .pill = true }
                            },
                            Tooltip {
                                .text = "快捷键 Ctrl+B 切换粗体格式",
                                .delay_sec = 0.25f,
                                .child = ToggleButton { .text = "B 粗体" }.bind_two_way(app.tb_bold)
                            },
                            Tooltip {
                                .text = "提示文本支持响应式 Bind(...) 动态更新",
                                .delay_sec = 0.25f,
                                .child = Button { .text = "动态绑定提示", .variant = ButtonVariant::Subtle }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

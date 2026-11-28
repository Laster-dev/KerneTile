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
            Text { .text = subtitle, .font_size = 12.5f, .overflow = TextOverflow::Wrap }.bind(&RenderText::set_color, app.muted_text)
        }
    };
}

} // namespace

// ==================== Label ====================
WidgetPtr build_label_page(GalleryApp& app) {
    static constexpr const char* kTagSvg =
        "M12 2.25c.3 0 .59.07.86.2l6.5 3.1c.54.26.89.8.89 1.4v4.73c0 4.65-3.07 8.79-7.53 10.12a2.5 2.5 0 0 1-1.44 0C6.82 20.47 3.75 16.33 3.75 11.68V6.95c0-.6.35-1.14.89-1.4l6.5-3.1c.27-.13.56-.2.86-.2Z";

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "Label", "WinUI 3 / Fluent 2 语义标签与富文本排版组件，支持多级字重、斜体、下划线、删除线、必填星号 (*)、前置 SVG 图标与语义胶囊。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "字体字重、斜体与文本装饰 (Bold / Italic / Underline / Strikethrough)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
                    Wrap {
                        .spacing = 16.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            Label { .text = "细体 Light (300)", .font_weight = FontWeight::Light },
                            Label { .text = "常规 Regular (400)", .font_weight = FontWeight::Regular },
                            Label { .text = "半粗 SemiBold (600)", .font_weight = FontWeight::SemiBold },
                            Label { .text = "粗体 Bold (700)", .bold = true }
                        }
                    },
                    Wrap {
                        .spacing = 16.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            Label { .text = "斜体 Italic 文本", .italic = true },
                            Label { .text = "粗斜体 Bold + Italic", .bold = true, .italic = true },
                            Label { .text = "下划线 Underline", .variant = LabelVariant::Accent, .underline = true },
                            Label { .text = "删除线 Strikethrough", .variant = LabelVariant::Caption, .strikethrough = true }
                        }
                    },
                    Wrap {
                        .spacing = 16.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            Label { .text = "原价 ¥299.00", .variant = LabelVariant::Caption, .strikethrough = true },
                            Label { .text = "限时特惠 ¥149.00", .variant = LabelVariant::Error, .bold = true },
                            Label { .text = "组合装饰 (粗体+斜体+下划线)", .variant = LabelVariant::Success, .bold = true, .italic = true, .underline = true },
                            Label { .text = "下划线+删除线", .underline = true, .strikethrough = true }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "WinUI 3 语义字阶与表单必填标签", .font_size = 14.0f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
                    Label { .text = "Display 展示大标题 (26px Bold)", .variant = LabelVariant::Title, .font_size = 26.0f, .bold = true },
                    Label { .text = "Title 标题标签 (20px SemiBold)", .variant = LabelVariant::Title },
                    Label { .text = "Subtitle 副标题标签 (16px SemiBold)", .variant = LabelVariant::Subtitle },
                    Label { .text = "Body 正文标签 (13.5px Regular)", .variant = LabelVariant::Body },
                    Label { .text = "Caption 辅助说明标签 (12px)", .variant = LabelVariant::Caption },
                    Wrap {
                        .spacing = 14.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            Label { .text = "用户名", .variant = LabelVariant::Body, .required = true },
                            Label { .text = "安全校验已通过", .variant = LabelVariant::Success, .svg_path = kTagSvg },
                            Label { .text = "只读禁用标签", .variant = LabelVariant::Body, .disabled = true }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "Fluent 2 语义胶囊标签 (pill = true)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
                    Wrap {
                        .spacing = 8.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            Label { .text = "默认标签", .variant = LabelVariant::Body, .pill = true },
                            Label { .text = "强调 Accent", .variant = LabelVariant::Accent, .pill = true, .svg_path = kTagSvg },
                            Label { .text = "已就绪 Success", .variant = LabelVariant::Success, .pill = true },
                            Label { .text = "高刷新 Warning", .variant = LabelVariant::Warning, .pill = true },
                            Label { .text = "异常 Error", .variant = LabelVariant::Error, .pill = true }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

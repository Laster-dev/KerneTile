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

// ==================== RichTextBlock ====================
WidgetPtr build_rich_text_block_page(GalleryApp& app) {
    auto link_status = std::make_shared<Property<std::string>>("点击下方富文本中的蓝色超链接，或直接拖动鼠标跨段划选并按 Ctrl+C 复制。");

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "RichTextBlock", "WinUI 3 富文本排版控件：支持多段落混排、多字号基线对齐、粗体/斜体/下划线/删除线/荧光高亮、内联代码胶囊、可点击超链接与拖拽选区复制。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "混排文档、内联代码与交互式超链接", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    RichTextBlock {
                        .paragraphs = {
                            RichTextParagraph{
                                .runs = {
                                    RichTextRun{ .text = "KerneTile ", .font_size = 18.0f, .bold = true },
                                    RichTextRun{ .text = "是一套基于 " },
                                    RichTextRun{ .text = "16×16 Tile Binning", .is_code = true },
                                    RichTextRun{ .text = " 与 " },
                                    RichTextRun{ .text = "GPU Compute Shader", .bold = true, .color = Color{0.38f, 0.80f, 1.0f, 1.0f} },
                                    RichTextRun{ .text = " 的极简跨平台 2D UI 引擎，支持 " },
                                    RichTextRun{ .text = "Direct3D 12", .link_url = "d3d12", .on_click = [link_status]() {
                                        link_status->set("已点击超链接: Direct3D 12 Compute 后端 (Windows 11 默认)");
                                    }},
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "Vulkan 1.3", .link_url = "vulkan", .on_click = [link_status]() {
                                        link_status->set("已点击超链接: Vulkan 1.3 Compute 后端 (Linux / Android)");
                                    }},
                                    RichTextRun{ .text = " 与 " },
                                    RichTextRun{ .text = "WebGPU / WASM", .link_url = "wgpu", .on_click = [link_status]() {
                                        link_status->set("已点击超链接: WebGPU / WASM 单文件浏览器后端");
                                    }},
                                    RichTextRun{ .text = "。" }
                                },
                                .spacing_after = 10.0f
                            },
                            RichTextParagraph{
                                .runs = {
                                    RichTextRun{ .text = "支持丰富的行内修饰：" },
                                    RichTextRun{ .text = "粗体强调", .bold = true },
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "斜体引述", .italic = true },
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "下划线标注", .underline = true },
                                    RichTextRun{ .text = "、" },
                                    RichTextRun{ .text = "已废弃接口", .strikethrough = true, .color = Color{0.62f, 0.62f, 0.66f, 1.0f} },
                                    RichTextRun{ .text = "、以及 " },
                                    RichTextRun{ .text = "荧光重点高亮", .bold = true, .color = Color{0.10f, 0.10f, 0.12f, 1.0f}, .highlight = Color{1.0f, 0.86f, 0.30f, 0.92f} },
                                    RichTextRun{ .text = " 和内联代码 " },
                                    RichTextRun{ .text = "RichTextBlock { .paragraphs = ... }", .is_code = true },
                                    RichTextRun{ .text = "。" }
                                },
                                .spacing_after = 6.0f
                            }
                        },
                        .font_size = 14.0f,
                        .line_height = 1.55f,
                        .is_text_selection_enabled = true
                    },
                    Text { .font_size = 12.5f }.bind(&RenderText::set_text, *link_status).bind(&RenderText::set_color, app.accent_text)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "单段快捷写法 (.runs = {...}) 与只读不可选模式", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    RichTextBlock {
                        .runs = {
                            RichTextRun{ .text = "版本状态：", .font_weight = FontWeight::SemiBold },
                            RichTextRun{ .text = "v0.0.2 已发布 ", .bold = true, .color = Color{0.25f, 0.82f, 0.45f, 1.0f} },
                            RichTextRun{ .text = "— 完整 40+ WinUI 3 控件库编译为单文件 " },
                            RichTextRun{ .text = "ktui_gallery.exe", .is_code = true },
                            RichTextRun{ .text = " 体积仅约 " },
                            RichTextRun{ .text = "1.0 MB", .bold = true, .underline = true },
                            RichTextRun{ .text = "（零外部运行库依赖）。" }
                        },
                        .font_size = 13.5f,
                        .is_text_selection_enabled = false
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

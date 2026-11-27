#include "../gallery_pages.hpp"
#include <cmath>

using namespace ktui;

namespace gallery::pages {

namespace {

WidgetPtr make_demo_tile(const std::string& label, Color bg, float w = -1.0f, float h = 42.0f) {
    return Box {
        .width = w,
        .height = h,
        .background_color = bg,
        .border_color = Color{1.0f, 1.0f, 1.0f, 0.16f},
        .border_width = 1.0f,
        .corner_radius = 6.0f,
        .padding = EdgeInsets(6.0f, 12.0f),
        .child = Text {
            .text = label,
            .font_size = 12.5f,
            .color = Color::white(),
            .font_weight = FontWeight::Medium
        },
        .alignment = Alignment::center()
    };
}

const Color kBlue   {0.00f, 0.47f, 0.84f, 0.88f};

const Color kTeal   {0.06f, 0.60f, 0.56f, 0.88f};

const Color kPurple {0.48f, 0.36f, 0.86f, 0.88f};

} // namespace

// ==================== 6. SplitView 可拖拽分隔独立展示页 ====================
WidgetPtr build_layout_split_view_page(GalleryApp& app) {
    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("SplitView 可拖拽分隔面板", 22.0f),
            app.make_muted("支持水平与垂直双栏自由嵌套、悬停高亮 Fluent 把手、ResizeEW/ResizeNS 光标自动切换、最小尺寸保护与双击重置。", 12.5f),
            Expanded {
                .flex = 1.0f,
                .child = Box {
                    .background_color = Color{0.0f, 0.0f, 0.0f, 0.14f},
                    .border_color = Color{1.0f, 1.0f, 1.0f, 0.10f},
                    .border_width = 1.0f,
                    .corner_radius = 6.0f,
                    .padding = EdgeInsets(6.0f, 6.0f),
                    .child = SplitView {
                        .vertical = false,
                        .ratio = app.layout_split_ratio.get(),
                        .min_first = 110.0f,
                        .min_second = 180.0f,
                        .first = make_demo_tile("左侧导航 / 文件树", kPurple, -1.0f, -1.0f),
                        .second = SplitView {
                            .vertical = true,
                            .ratio = 0.62f,
                            .min_first = 72.0f,
                            .min_second = 64.0f,
                            .first = make_demo_tile("主视图 / 代码编辑器", kBlue, -1.0f, -1.0f),
                            .second = make_demo_tile("底部控制台 / 调试输出", kTeal, -1.0f, -1.0f)
                        },
                        .on_ratio_changed = [&app](float r) {
                            int p1 = static_cast<int>(std::lround(r * 100.0f));
                            app.layout_split_label.set("当前分割比例: " + std::to_string(p1) + "% / " + std::to_string(100 - p1) + "% (拖拽中间分隔条调整，双击重置)");
                        }
                    }.bind_two_way(app.layout_split_ratio)
                }
            },
            app.make_accent(app.layout_split_label, 12.0f)
        }
    });
}

} // namespace gallery::pages

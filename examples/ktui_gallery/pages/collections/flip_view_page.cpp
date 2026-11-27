#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_flip_view_page(GalleryApp& app) {
    auto make_slide = [](const std::string& title, const std::string& desc, Color bg) -> WidgetPtr {
        return Box {
            .background_color = bg,
            .padding = EdgeInsets(24.0f, 36.0f),
            .child = VStack {
                .align_items = AlignItems::Center,
                .spacing = 8.0f,
                .content = {
                    Text { .text = title, .font_size = 20.0f, .color = Color{1.0f, 1.0f, 1.0f, 0.96f} },
                    Text { .text = desc, .font_size = 13.0f, .color = Color{1.0f, 1.0f, 1.0f, 0.80f} }
                }
            }
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("FlipView", 22.0f),
            app.make_muted("WinUI 3 轮播视图，支持悬停左右翻页按钮、触控滑动切页与平滑位移过渡，下方与 PipsPager 双向绑定联动。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    FlipView {
                        .items = {
                            make_slide("01 · KerneTile 16×16 瓦片渲染", "纯 C++20 零依赖 2D 计算着色器与 AVX2 软光栅双引擎", Color{0.08f, 0.34f, 0.62f, 0.85f}),
                            make_slide("02 · WinUI 3 Fluent 2 设计体系", "40 个独立控件展厅 · 真高斯阴影 · 云母动态聚焦 · 主题涟漪", Color{0.10f, 0.45f, 0.38f, 0.85f}),
                            make_slide("03 · 全平台原生与 WebOS Desktop", "Windows / Linux / Android / Emscripten WASM 桌面多开", Color{0.38f, 0.22f, 0.58f, 0.85f}),
                            make_slide("04 · 100,000 行虚拟化 120 FPS", "VirtualList & DataGrid 恒定 O(1) 视口开销", Color{0.62f, 0.24f, 0.32f, 0.85f})
                        },
                        .width = 520.0f,
                        .height = 170.0f
                    }.bind_two_way(app.flip_page),
                    PipsPager {
                        .number_of_pages = 4
                    }.bind_two_way(app.flip_page)
                }
            })
        }
    });
}

} // namespace gallery::pages

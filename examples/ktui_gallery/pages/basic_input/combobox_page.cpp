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

// ==================== ComboBox ====================
WidgetPtr build_combobox_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "ComboBox", "WinUI 3 下拉选择框，内置超长列表裁剪、自动向上翻转、高斯软阴影与平滑/触控滚动。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "图形渲染后端选择 (10 项可滚动列表)", .font_size = 14.0f },
                    ComboBox {
                        .items = {
                            ComboBoxItem("Direct3D 12 Compute"),
                            ComboBoxItem("Vulkan 1.3 Compute"),
                            ComboBoxItem("WebGPU (WASM)"),
                            ComboBoxItem("CPU SIMD Tile Rasterizer"),
                            ComboBoxItem("Metal 3 Compute"),
                            ComboBoxItem("OpenGL ES 3.2 (Legacy)", true),
                            ComboBoxItem("Software Fallback (AVX2)"),
                            ComboBoxItem("Headless Test Renderer"),
                            ComboBoxItem("Direct2D Interop"),
                            ComboBoxItem("Remote Tile Stream")
                        },
                        .placeholder = "选择渲染后端...",
                        .width = 240.0f,
                        .max_visible_items = 6,
                        .max_dropdown_height = 200.0f,
                        .on_change = [&app](int, const std::string& label) {
                            app.combo_preview.set("已选后端: " + label);
                        }
                    }.bind_two_way(app.combo_idx),
                    app.make_accent(app.combo_preview, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

#include "../gallery_pages.hpp"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

// ============================================================================
// 2. 微调 Scrubbing 数字输入框 (ScrubNumberBox)
// ============================================================================
WidgetPtr build_scrub_number_box_page(GalleryApp& app) {
    auto summary = std::make_shared<Property<std::string>>("当前参数: X = 120.0 px,  Rotation = 15.0 °,  Opacity = 85 %,  Blur = 12.0 px");
    auto state = std::make_shared<std::array<double, 4>>(std::array<double, 4>{120.0, 15.0, 85.0, 12.0});

    auto refresh = [summary, state]() {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "当前参数: X = %.1f px,  Rotation = %.1f °,  Opacity = %.0f %%,  Blur = %.1f px",
                      (*state)[0], (*state)[1], (*state)[2], (*state)[3]);
        summary->set(buf);
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("ScrubNumberBox (微调 Scrubbing 数字框)", 24.0f),
            app.make_muted("Blender / Figma 风格参数输入框：按住左右拖动无级调节数值，按住 Shift 10× 加速，Alt 0.1× 精细微调；单击进入键盘编辑，双击恢复默认值。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Grid {
                        .columns = 2,
                        .column_spacing = 12.0f,
                        .row_spacing = 10.0f,
                        .content = {
                            ScrubNumberBox {
                                .label = "Position X", .value = 120.0, .min_value = -500.0, .max_value = 500.0, .step = 1.0, .precision = 1, .unit = "px",
                                .on_changed = [state, refresh](double v) { (*state)[0] = v; refresh(); }
                            },
                            ScrubNumberBox {
                                .label = "Rotation", .value = 15.0, .min_value = -180.0, .max_value = 180.0, .step = 0.5, .precision = 1, .unit = "°",
                                .on_changed = [state, refresh](double v) { (*state)[1] = v; refresh(); }
                            },
                            ScrubNumberBox {
                                .label = "Opacity", .value = 85.0, .min_value = 0.0, .max_value = 100.0, .step = 1.0, .precision = 0, .unit = "%",
                                .on_changed = [state, refresh](double v) { (*state)[2] = v; refresh(); }
                            },
                            ScrubNumberBox {
                                .label = "Blur Radius", .value = 12.0, .min_value = 0.0, .max_value = 64.0, .step = 0.5, .precision = 1, .unit = "px",
                                .on_changed = [state, refresh](double v) { (*state)[3] = v; refresh(); }
                            }
                        }
                    },
                    app.make_accent(*summary, 12.5f, FontWeight::Medium)
                }
            })
        }
    });
}

} // namespace gallery::pages

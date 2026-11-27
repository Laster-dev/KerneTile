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

// ==================== RadioCard ====================
WidgetPtr build_radio_card_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "RadioCard", "图标化大卡片单选组件，适合电源模式、主题或布局模板选择。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "性能配置单选卡片组", .font_size = 14.0f },
                    RadioCardGroup {
                        .options = {
                            RadioCardOption {
                                .label = "节能",
                                .value = 0,
                                .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                                    float cx = std::round(r.min_x + r.width() * 0.5f);
                                    float cy = std::round(r.min_y + r.height() * 0.5f);
                                    ctx.draw_rect(Rect::from_xywh(cx - 7.0f, cy - 7.0f, 14.0f, 14.0f), col, 7.0f, 1.6f);
                                    ctx.draw_line(PointF{cx, cy - 3.5f}, PointF{cx, cy + 3.5f}, col, 1.6f);
                                }
                            },
                            RadioCardOption {
                                .label = "均衡",
                                .value = 1,
                                .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                                    float cx = std::round(r.min_x + r.width() * 0.5f);
                                    float cy = std::round(r.min_y + r.height() * 0.5f);
                                    ctx.draw_rect(Rect::from_xywh(cx - 8.0f, cy - 5.0f, 16.0f, 10.0f), col, 3.0f, 1.6f);
                                    ctx.draw_rect(Rect::from_xywh(cx - 3.0f, cy - 2.0f, 6.0f, 4.0f), col, 1.0f, 0.0f);
                                }
                            },
                            RadioCardOption {
                                .label = "性能",
                                .value = 2,
                                .icon_painter = [](PaintContext& ctx, Rect r, Color col) {
                                    float cx = std::round(r.min_x + r.width() * 0.5f);
                                    float cy = std::round(r.min_y + r.height() * 0.5f);
                                    ctx.draw_line(PointF{cx + 1.5f, cy - 8.0f}, PointF{cx - 4.5f, cy + 0.5f}, col, 1.8f);
                                    ctx.draw_line(PointF{cx - 4.5f, cy + 0.5f}, PointF{cx + 1.0f, cy + 0.5f}, col, 1.8f);
                                    ctx.draw_line(PointF{cx + 1.0f, cy + 0.5f}, PointF{cx - 1.5f, cy + 8.0f}, col, 1.8f);
                                }
                            }
                        },
                        .tile_width = 96.0f,
                        .tile_height = 74.0f
                    }.bind_two_way(app.radio_card_val)
                }
            })
        }
    });
}

} // namespace gallery::pages

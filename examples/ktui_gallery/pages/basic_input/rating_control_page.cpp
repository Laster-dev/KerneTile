#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_rating_control_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("RatingControl", 22.0f),
            app.make_muted("WinUI 3 星级评分控件，支持 Hover 实时预览高亮、整星/半星精度、再次点击清零与只读展示模式。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "交互式整星评分 (再次点击当前星级可清零)", .font_size = 14.0f },
                    RatingControl {
                        .max_rating = 5,
                        .is_clear_enabled = true,
                        .allow_half_stars = false,
                        .star_size = 24.0f,
                        .on_value_changed = [&app](float v) {
                            char buf[32];
                            std::snprintf(buf, sizeof(buf), "%.1f / 5.0", v);
                            app.rating_caption.set(buf);
                        }
                    }.bind_two_way(app.rating_val)
                     .bind(&RenderRatingControl::set_caption, app.rating_caption)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "半星精度评分 (AllowHalfStars = true)", .font_size = 14.0f },
                    RatingControl {
                        .max_rating = 5,
                        .allow_half_stars = true,
                        .star_size = 24.0f,
                        .on_value_changed = [&app](float v) {
                            char buf[48];
                            std::snprintf(buf, sizeof(buf), "%.1f / 5.0 (半星模式)", v);
                            app.rating_half_caption.set(buf);
                        }
                    }.bind_two_way(app.rating_half_val)
                     .bind(&RenderRatingControl::set_caption, app.rating_half_caption)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "只读展示 (ReadOnly = true)", .font_size = 14.0f },
                    RatingControl {
                        .value = 4.7f,
                        .max_rating = 5,
                        .is_read_only = true,
                        .star_size = 20.0f,
                        .caption = "4.7 (1,284 条评价)"
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

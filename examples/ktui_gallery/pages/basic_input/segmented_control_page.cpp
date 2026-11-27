#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_segmented_control_page(GalleryApp& app) {
    static const char* kModes[] = {"列表视图", "网格视图", "紧凑详情"};
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("SegmentedControl", 22.0f),
            app.make_muted("Fluent 2 分段选择器，内置 FrameClock 驱动的平滑滑动激活胶囊底板与底部主题色微指示条。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "视图切换分段栏", .font_size = 14.0f },
                    SegmentedControl {
                        .items = {
                            SegmentedItem{ .label = "列表" },
                            SegmentedItem{ .label = "网格" },
                            SegmentedItem{ .label = "详情" }
                        },
                        .width = 280.0f,
                        .on_selected_index_changed = [&app](int idx) {
                            if (idx >= 0 && idx < 3) {
                                app.seg_status.set(std::string("当前视图模式: ") + kModes[idx]);
                            }
                        }
                    }.bind_two_way(app.seg_idx),
                    app.make_accent(app.seg_status, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

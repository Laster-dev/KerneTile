#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_virtual_list_page(GalleryApp& app) {
    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("VirtualList", 22.0f),
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 10.0f,
                .content = {
                    app.make_muted("视口虚拟化高性能列表：100,000 条记录仍保持 O(1) 内存与 120 FPS 丝滑滚动。", 12.5f),
                    Label { .text = "O(1) 视口裁剪", .variant = LabelVariant::Accent, .pill = true }
                }
            },
            Expanded {
                .flex = 1.0f,
                .child = VirtualList {
                    .item_count = 100000,
                    .item_height = 38.0f,
                    .height = 0.0f,
                    .overscan = 3,
                    .item_builder = [&app](size_t idx) -> WidgetPtr {
                        bool even = (idx % 2 == 0);
                        return Box {
                            .background_color = even ? Color::transparent() : Color{1.0f, 1.0f, 1.0f, 0.02f},
                            .padding = EdgeInsets(0.0f, 12.0f),
                            .child = HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 12.0f,
                                .content = {
                                    Label {
                                        .text = "#" + std::to_string(idx + 1),
                                        .variant = (idx % 5 == 0) ? LabelVariant::Accent : LabelVariant::Caption,
                                        .pill = true
                                    },
                                    Text {
                                        .text = "KerneTile 虚拟化数据条目 #" + std::to_string(idx + 1) + "  ·  Tile Binning 16×16",
                                        .font_size = 13.0f
                                    },
                                    Text {
                                        .text = (idx % 3 == 0) ? "GPU Ready" : "Active",
                                        .font_size = 11.5f
                                    }.bind(&RenderText::set_color, app.muted_text)
                                }
                            }
                        };
                    }
                }
            }
        }
    });
}

} // namespace gallery::pages

#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

namespace {

constexpr const char* kSaveSvg =
    "M5.25 3A2.25 2.25 0 0 0 3 5.25v13.5A2.25 2.25 0 0 0 5.25 21h13.5A2.25 2.25 0 0 0 21 18.75V7.96a2.25 2.25 0 0 0-.659-1.591l-2.71-2.71A2.25 2.25 0 0 0 16.04 3H5.25ZM4.5 5.25a.75.75 0 0 1 .75-.75H7v4.25A1.75 1.75 0 0 0 8.75 10.5h5.5A1.75 1.75 0 0 0 16 8.75V4.5h.04a.75.75 0 0 1 .53.22l2.71 2.71a.75.75 0 0 1 .22.53v10.79a.75.75 0 0 1-.75.75H17v-5.25A1.75 1.75 0 0 0 15.25 12.5h-6.5A1.75 1.75 0 0 0 7 14.25v5.25H5.25a.75.75 0 0 1-.75-.75V5.25Zm4 9a.25.25 0 0 1 .25-.25h6.5a.25.25 0 0 1 .25.25v5.25h-7v-5.25ZM8.5 4.5h6v4.25a.25.25 0 0 1-.25.25h-5.5a.25.25 0 0 1-.25-.25V4.5Z";

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

// ==================== TabView ====================
WidgetPtr build_tab_view_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "TabView", "WinUI 3 多标签页控件 (Windows Terminal / Edge 风格)：选中标签无缝融底、单标签关闭 (×) 与动态新建 (+)。"),
            TabView {
                .is_add_tab_button_visible = true,
                .height = 280.0f,
                .content_builder = [&app](const std::string& tab_id) -> WidgetPtr {
                    return Box {
                        .padding = EdgeInsets(18.0f, 20.0f),
                        .child = VStack {
                            .align_items = AlignItems::Stretch,
                            .spacing = 10.0f,
                            .content = {
                                Label { .text = "当前标签页: " + tab_id, .variant = LabelVariant::Subtitle },
                                app.make_muted("点击顶部「+」可动态新建标签页，点击标签右侧「×」可关闭可关闭标签页；首标签「欢迎页」设置为不可关闭 (is_closable = false)。", 13.0f),
                                HStack {
                                    .align_items = AlignItems::Center,
                                    .spacing = 8.0f,
                                    .content = {
                                        Label { .text = "Tab ID: " + tab_id, .variant = LabelVariant::Accent, .pill = true },
                                        Label { .text = "GPU 硬件合成", .variant = LabelVariant::Success, .pill = true }
                                    }
                                }
                            }
                        }
                    };
                },
                .on_selection_changed = [&app](int idx) {
                    app.tab_selected.set(idx);
                },
                .on_tab_close = [&app](int idx) {
                    auto list = app.tab_items.get();
                    if (idx >= 0 && idx < static_cast<int>(list.size())) {
                        list.erase(list.begin() + idx);
                        int next_sel = std::min(app.tab_selected.get(), static_cast<int>(list.size()) - 1);
                        app.tab_items.set(std::move(list));
                        app.tab_selected.set(std::max(0, next_sel));
                    }
                },
                .on_add_tab = [&app]() {
                    auto list = app.tab_items.get();
                    int n = app.tab_seq++;
                    list.push_back(TabViewItem{
                        .id = "tab_" + std::to_string(n),
                        .header = "文档 #" + std::to_string(n),
                        .svg_path = kSaveSvg,
                        .is_closable = true
                    });
                    int new_idx = static_cast<int>(list.size()) - 1;
                    app.tab_items.set(std::move(list));
                    app.tab_selected.set(new_idx);
                }
            }.bind(&RenderTabView::set_tabs_value, app.tab_items)
             .bind_two_way(app.tab_selected)
        }
    });
}

} // namespace gallery::pages

#include "../gallery_pages.hpp"

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

// ==================== BreadcrumbBar ====================
WidgetPtr build_breadcrumb_bar_page(GalleryApp& app) {
    auto sibling_path = std::make_shared<Property<std::string>>("当前路径: root / var / log / app（点击段名回退，点击段右侧 › / ▾ 弹出同级兄弟节点横跳）");
    auto sibling_items = std::make_shared<Property<std::vector<BreadcrumbItem>>>(std::vector<BreadcrumbItem>{
        BreadcrumbItem{ .id = "root", .label = "root", .siblings = {"root", "mnt", "proc"} },
        BreadcrumbItem{ .id = "var",  .label = "var",  .siblings = {"var", "etc", "usr", "opt"} },
        BreadcrumbItem{ .id = "log",  .label = "log",  .siblings = {"log", "cache", "lib", "run"} },
        BreadcrumbItem{ .id = "app",  .label = "app",  .siblings = {"app", "nginx", "systemd", "kernel"} }
    });
    auto format_path = [](const std::vector<BreadcrumbItem>& v) {
        std::string out;
        for (size_t i = 0; i < v.size(); ++i) { if (i > 0) out += " / "; out += v[i].label; }
        return out;
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "BreadcrumbBar", "WinUI 3 面包屑路径导航条：支持点击前段路径回溯、窄宽度自动折叠「...」浮层菜单，以及路径段 › / ▾ 同级兄弟节点下拉横跳。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "交互式文件系统路径导航", .font_size = 14.0f },
                    BreadcrumbBar {
                        .font_size = 14.5f,
                        .on_item_clicked = [&app](size_t idx, const BreadcrumbItem& item) {
                            auto list = app.breadcrumb_items.get();
                            if (idx + 1 < list.size()) {
                                list.resize(idx + 1);
                                app.breadcrumb_items.set(std::move(list));
                            }
                            app.breadcrumb_status.set("已跳转至目录: " + item.label);
                        }
                    }.bind(&RenderBreadcrumbBar::set_items_value, app.breadcrumb_items),
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "进入子目录 (+)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    auto list = app.breadcrumb_items.get();
                                    std::string name = "folder_" + std::to_string(list.size());
                                    list.push_back(BreadcrumbItem{ .id = name, .label = name });
                                    app.breadcrumb_items.set(std::move(list));
                                    app.breadcrumb_status.set("已进入子目录: " + name);
                                }
                            },
                            Button {
                                .text = "重置完整路径",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    app.breadcrumb_items.set({
                                        BreadcrumbItem{ .id = "root", .label = "此电脑" },
                                        BreadcrumbItem{ .id = "d", .label = "D:\\" },
                                        BreadcrumbItem{ .id = "kt", .label = "KerneTile" },
                                        BreadcrumbItem{ .id = "ktui", .label = "KTUI" },
                                        BreadcrumbItem{ .id = "widgets", .label = "widgets" }
                                    });
                                    app.breadcrumb_status.set("已恢复默认 5 级路径");
                                }
                            }
                        }
                    },
                    app.make_accent(app.breadcrumb_status, 12.0f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "路径段同级兄弟下拉横跳 (Segment Sibling Jump)", .font_size = 14.0f },
                    BreadcrumbBar {
                        .font_size = 13.5f,
                        .on_item_clicked = [sibling_items, sibling_path, format_path](size_t idx, const BreadcrumbItem&) {
                            auto list = sibling_items->get();
                            if (idx + 1 < list.size()) {
                                list.resize(idx + 1);
                                sibling_items->set(list);
                            }
                            sibling_path->set("已回退至路径: " + format_path(list));
                        },
                        .on_sibling_selected = [sibling_items, sibling_path, format_path](size_t idx, const std::string& chosen) {
                            auto list = sibling_items->get();
                            if (idx < list.size()) {
                                list[idx].id = chosen;
                                list[idx].label = chosen;
                                list.resize(idx + 1);
                                sibling_items->set(list);
                            }
                            sibling_path->set("已横跳至兄弟路径: " + format_path(list));
                        }
                    }.bind(&RenderBreadcrumbBar::set_items_value, *sibling_items),
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            app.make_accent(*sibling_path, 12.0f),
                            Button {
                                .text = "重置路径",
                                .on_click = [sibling_items, sibling_path]() {
                                    sibling_items->set({
                                        BreadcrumbItem{ .id = "root", .label = "root", .siblings = {"root", "mnt", "proc"} },
                                        BreadcrumbItem{ .id = "var",  .label = "var",  .siblings = {"var", "etc", "usr", "opt"} },
                                        BreadcrumbItem{ .id = "log",  .label = "log",  .siblings = {"log", "cache", "lib", "run"} },
                                        BreadcrumbItem{ .id = "app",  .label = "app",  .siblings = {"app", "nginx", "systemd", "kernel"} }
                                    });
                                    sibling_path->set("当前路径: root / var / log / app");
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

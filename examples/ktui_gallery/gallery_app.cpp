#include "gallery_app.hpp"
#include "pages/gallery_pages.hpp"

using namespace ktui;

namespace gallery {

void GalleryApp::refresh_count() {
    int step = step_ten.get() ? 10 : 1;
    count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" +
                   (locked.get() ? "  [已锁定]" : ""));
}

WidgetPtr GalleryApp::make_card(WidgetPtr child) {
    return Box {
        .border_width = 1.0f,
        .corner_radius = 6.0f,
        .padding = EdgeInsets(12.0f, 14.0f),
        .theme_card = true,
        .child = std::move(child)
    };
}

WidgetPtr GalleryApp::make_scroll_page(WidgetPtr body) {
    return ScrollView {
        .child = Box {
            .background_color = Color::transparent(),
            .padding = EdgeInsets(16.0f, 22.0f),
            .child = std::move(body)
        }
    };
}

void GalleryApp::spawn_child_window() {
    int idx = child_window_seq++;
    WindowDesc child_desc;
    child_desc.title = "工具窗口 #" + std::to_string(idx);
    child_desc.size = Vec2{360.0f, 240.0f};
    child_desc.min_size = Vec2{260.0f, 180.0f};
    child_desc.resizable = true;

    auto child_win = app->create_window(child_desc);
    if (!child_win) return;

    auto note = std::make_shared<Property<std::string>>("独立渲染与事件循环窗口 #" + std::to_string(idx));
    child_notes.push_back(note);
    auto raw_child = child_win.get();

    child_win->set_content(Box {
        .background_color = Bind(window_bg),
        .padding = EdgeInsets(14.0f, 16.0f),
        .child = VStack {
            .align_items = AlignItems::Stretch,
            .spacing = 10.0f,
            .content = {
                make_card(VStack {
                    .spacing = 8.0f,
                    .content = {
                        Text { .text = Bind(*note), .font_size = 13.5f },
                        Text { .text = "主窗口与子窗口独立合成、共享主题与响应式状态。", .font_size = 12.0f, .color = Bind(muted_text) },
                        HStack {
                            .align_items = AlignItems::Center,
                            .spacing = 8.0f,
                            .content = {
                                Button {
                                    .text = "主计数 +1",
                                    .variant = ButtonVariant::Accent,
                                    .on_click = [this]() {
                                        if (!locked.get()) {
                                            count += 1;
                                            refresh_count();
                                        }
                                    }
                                },
                                Button {
                                    .text = "关闭此窗口",
                                    .variant = ButtonVariant::Danger,
                                    .on_click = [raw_child]() {
                                        if (raw_child) raw_child->close();
                                    }
                                }
                            }
                        }
                    }
                })
            }
        }
    });
    child_win->show();
}

void GalleryApp::setup(Application& application, std::shared_ptr<Window> win) {
    app = &application;
    window = std::move(win);

    app->theme.subscribe([this](const Theme& t) {
        if (t.is_dark) {
#if defined(__ANDROID__)
            window_bg.set(Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f});
            card_bg.set(Color{0x27 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f});
            card_border.set(Color{1.0f, 1.0f, 1.0f, 0.06f});
#else
            window_bg.set(Color::transparent());
            card_bg.set(Color{1.0f, 1.0f, 1.0f, 0.045f});
            card_border.set(Color{1.0f, 1.0f, 1.0f, 0.07f});
#endif
            muted_text.set(Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f});
            accent_text.set(Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f});
            theme_btn_text.set("切换浅色主题");
        } else {
#if defined(__ANDROID__)
            window_bg.set(Color{0xEA / 255.0f, 0xED / 255.0f, 0xF1 / 255.0f, 1.0f});
            card_bg.set(Color{0xFA / 255.0f, 0xFB / 255.0f, 0xFD / 255.0f, 1.0f});
            card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
#else
            window_bg.set(Color::transparent());
            card_bg.set(Color{1.0f, 1.0f, 1.0f, 0.65f});
            card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
#endif
            muted_text.set(Color{0x59 / 255.0f, 0x60 / 255.0f, 0x6A / 255.0f, 1.0f});
            accent_text.set(Color{0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f});
            theme_btn_text.set("切换深色主题");
        }
    });

#if !defined(__ANDROID__)
    window->titlebar().add_action("+", "新建工具窗口", [this]() { spawn_child_window(); });
#endif

    window->set_content(NavigationView {
        .menu_items = {
            NavigationViewItem{ .id = "home", .label = "全部控件概览", .icon = NavigationIcon::Home },
            NavigationViewItem::header("基础与输入"),
            NavigationViewItem{
                .id = "controls", .label = "基础交互", .icon = NavigationIcon::Controls, .badge = "2", .expanded = true,
                .children = {
                    NavigationViewItem{ .id = "buttons", .label = "按钮与开关", .icon = NavigationIcon::Sparkles },
                    NavigationViewItem{ .id = "inputs",  .label = "文本输入",   .icon = NavigationIcon::Inputs }
                }
            },
            NavigationViewItem{ .id = "selection", .label = "下拉与单选", .icon = NavigationIcon::ComboBox, .badge = "New" },
            NavigationViewItem::separator(),
            NavigationViewItem::header("图形与状态"),
            NavigationViewItem{ .id = "progress", .label = "滑块与进度", .icon = NavigationIcon::Progress },
            NavigationViewItem{ .id = "dialogs",  .label = "弹窗与菜单", .icon = NavigationIcon::Layout, .badge = "New" },
            NavigationViewItem{ .id = "svg",      .label = "NanoSVG 矢量", .icon = NavigationIcon::Palette }
        },
        .footer_items = {
            NavigationViewItem{ .id = "about", .label = "关于 KerneTile", .icon = NavigationIcon::Info }
        },
        .selected_id = Bind(nav_selected),
        .pane_title = "KTUI Gallery",
        .pane_display_mode = Bind(nav_mode),
        .is_pane_open = true,
        .is_pane_toggle_button_visible = true,
        .is_back_button_visible = true,
        .is_settings_visible = true,
        .open_pane_length = 228.0f,
        .page_builder = [this](const std::string& page_id) -> WidgetPtr {
            if (page_id == "buttons")      return pages::build_buttons_page(*this);
            if (page_id == "inputs")       return pages::build_inputs_page(*this);
            if (page_id == "selection")    return pages::build_selection_page(*this);
            if (page_id == "progress")     return pages::build_progress_page(*this);
            if (page_id == "dialogs")      return pages::build_dialogs_menus_page(*this);
            if (page_id == "svg")          return pages::build_svg_page(*this);
            if (page_id == "__settings__") return pages::build_settings_page(*this);
            if (page_id == "about")        return pages::build_about_page(*this);
            return pages::build_home_page(*this);
        }
    });
}

} // namespace gallery

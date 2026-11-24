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
            NavigationViewItem{ .id = "home", .label = "主页", .icon = NavigationIcon::Home },
            NavigationViewItem::separator(),
            NavigationViewItem{
                .id = "cat_basic_input", .label = "基础输入", .icon = NavigationIcon::Controls, .badge = "14", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "button",               .label = "Button" },
                    NavigationViewItem{ .id = "toggle_button",        .label = "ToggleButton",       .badge = "New" },
                    NavigationViewItem{ .id = "segmented_control",    .label = "SegmentedControl",   .badge = "New" },
                    NavigationViewItem{ .id = "dropdown_button",      .label = "DropDownButton" },
                    NavigationViewItem{ .id = "checkbox",             .label = "CheckBox" },
                    NavigationViewItem{ .id = "toggle_switch",        .label = "ToggleSwitch" },
                    NavigationViewItem{ .id = "radio_button",         .label = "RadioButton" },
                    NavigationViewItem{ .id = "radio_card",           .label = "RadioCard" },
                    NavigationViewItem{ .id = "combobox",             .label = "ComboBox" },
                    NavigationViewItem{ .id = "slider",               .label = "Slider" },
                    NavigationViewItem{ .id = "calendar_date_picker", .label = "CalendarDatePicker" },
                    NavigationViewItem{ .id = "time_picker",          .label = "TimePicker" },
                    NavigationViewItem{ .id = "color_picker",         .label = "ColorPicker" },
                    NavigationViewItem{ .id = "rating_control",       .label = "RatingControl" }
                }
            },
            NavigationViewItem{
                .id = "cat_text", .label = "文本与数值", .icon = NavigationIcon::Inputs, .badge = "6", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "label",            .label = "Label" },
                    NavigationViewItem{ .id = "textbox",          .label = "TextBox" },
                    NavigationViewItem{ .id = "text_area",        .label = "TextArea",       .badge = "New" },
                    NavigationViewItem{ .id = "auto_suggest_box", .label = "AutoSuggestBox" },
                    NavigationViewItem{ .id = "password_box",     .label = "PasswordBox" },
                    NavigationViewItem{ .id = "number_box",       .label = "NumberBox" }
                }
            },
            NavigationViewItem{
                .id = "cat_nav", .label = "导航与容器", .icon = NavigationIcon::Layout, .badge = "9", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "navigation_view", .label = "NavigationView" },
                    NavigationViewItem{ .id = "tab_view",        .label = "TabView" },
                    NavigationViewItem{ .id = "breadcrumb_bar",  .label = "BreadcrumbBar" },
                    NavigationViewItem{ .id = "expander",        .label = "Expander" },
                    NavigationViewItem{ .id = "tree_view",       .label = "TreeView" },
                    NavigationViewItem{ .id = "virtual_list",    .label = "VirtualList" },
                    NavigationViewItem{ .id = "data_grid",       .label = "DataGrid" },
                    NavigationViewItem{ .id = "flip_view",       .label = "FlipView",       .badge = "New" },
                    NavigationViewItem{ .id = "pips_pager",      .label = "PipsPager",      .badge = "New" }
                }
            },
            NavigationViewItem{
                .id = "cat_menus", .label = "菜单与工具栏", .icon = NavigationIcon::Layout, .badge = "2", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "command_bar",  .label = "CommandBar" },
                    NavigationViewItem{ .id = "menu_flyout",  .label = "MenuFlyout" }
                }
            },
            NavigationViewItem{
                .id = "cat_status", .label = "弹窗与状态", .icon = NavigationIcon::Progress, .badge = "8", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "content_dialog", .label = "ContentDialog" },
                    NavigationViewItem{ .id = "teaching_tip",   .label = "TeachingTip" },
                    NavigationViewItem{ .id = "info_bar",       .label = "InfoBar" },
                    NavigationViewItem{ .id = "info_badge",     .label = "InfoBadge",     .badge = "New" },
                    NavigationViewItem{ .id = "person_picture", .label = "PersonPicture", .badge = "New" },
                    NavigationViewItem{ .id = "toast",          .label = "Toast" },
                    NavigationViewItem{ .id = "progress_bar",   .label = "ProgressBar" },
                    NavigationViewItem{ .id = "progress_ring",  .label = "ProgressRing" }
                }
            },
            NavigationViewItem{
                .id = "cat_media", .label = "图形与矢量", .icon = NavigationIcon::Palette, .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "svg", .label = "NanoSVG 矢量" }
                }
            }
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
        .is_search_box_visible = true,
        .search_placeholder = "搜索控件",
        .open_pane_length = 236.0f,
        .page_builder = [this](const std::string& page_id) -> WidgetPtr {
            if (page_id == "button")               return pages::build_button_page(*this);
            if (page_id == "toggle_button")        return pages::build_toggle_button_page(*this);
            if (page_id == "segmented_control")    return pages::build_segmented_control_page(*this);
            if (page_id == "dropdown_button")      return pages::build_dropdown_button_page(*this);
            if (page_id == "checkbox")        return pages::build_checkbox_page(*this);
            if (page_id == "toggle_switch")   return pages::build_toggle_switch_page(*this);
            if (page_id == "radio_button")    return pages::build_radio_button_page(*this);
            if (page_id == "radio_card")      return pages::build_radio_card_page(*this);
            if (page_id == "combobox")             return pages::build_combobox_page(*this);
            if (page_id == "slider")               return pages::build_slider_page(*this);
            if (page_id == "calendar_date_picker") return pages::build_calendar_date_picker_page(*this);
            if (page_id == "time_picker")          return pages::build_time_picker_page(*this);
            if (page_id == "color_picker")         return pages::build_color_picker_page(*this);
            if (page_id == "rating_control")       return pages::build_rating_control_page(*this);
            if (page_id == "label")                return pages::build_label_page(*this);
            if (page_id == "textbox")              return pages::build_textbox_page(*this);
            if (page_id == "text_area")            return pages::build_text_area_page(*this);
            if (page_id == "auto_suggest_box")     return pages::build_auto_suggest_box_page(*this);
            if (page_id == "password_box")         return pages::build_password_box_page(*this);
            if (page_id == "number_box")           return pages::build_number_box_page(*this);
            if (page_id == "navigation_view")      return pages::build_navigation_view_page(*this);
            if (page_id == "tab_view")             return pages::build_tab_view_page(*this);
            if (page_id == "breadcrumb_bar")       return pages::build_breadcrumb_bar_page(*this);
            if (page_id == "expander")             return pages::build_expander_page(*this);
            if (page_id == "tree_view")            return pages::build_tree_view_page(*this);
            if (page_id == "virtual_list")         return pages::build_virtual_list_page(*this);
            if (page_id == "data_grid")            return pages::build_data_grid_page(*this);
            if (page_id == "flip_view")            return pages::build_flip_view_page(*this);
            if (page_id == "pips_pager")           return pages::build_pips_pager_page(*this);
            if (page_id == "command_bar")     return pages::build_command_bar_page(*this);
            if (page_id == "menu_flyout")     return pages::build_menu_flyout_page(*this);
            if (page_id == "content_dialog")       return pages::build_content_dialog_page(*this);
            if (page_id == "teaching_tip")         return pages::build_teaching_tip_page(*this);
            if (page_id == "info_bar")        return pages::build_info_bar_page(*this);
            if (page_id == "info_badge")      return pages::build_info_badge_page(*this);
            if (page_id == "person_picture")  return pages::build_person_picture_page(*this);
            if (page_id == "toast")           return pages::build_toast_page(*this);
            if (page_id == "progress_bar")    return pages::build_progress_bar_page(*this);
            if (page_id == "progress_ring")   return pages::build_progress_ring_page(*this);
            if (page_id == "svg")             return pages::build_svg_page(*this);
            if (page_id == "__settings__")    return pages::build_settings_page(*this);
            if (page_id == "about")           return pages::build_about_page(*this);
            return pages::build_home_page(*this);
        }
    });
}

} // namespace gallery

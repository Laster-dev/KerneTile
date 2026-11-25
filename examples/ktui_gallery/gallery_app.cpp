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

namespace {

// Fluent 2 极简图标复制按钮（双叠纸矢量图标，点击复制到剪贴板并切换为绿色 ✓ 反馈 1.5s）
class RenderCopyIconButton : public RenderBox {
public:
    explicit RenderCopyIconButton(std::string copy_text) : m_copy_text(std::move(copy_text)) {
        cursor_shape = CursorShape::PointingHand;
        on_pointer_enter = [this](PointerEvent&) { m_hovered = true; mark_needs_paint(); };
        on_pointer_exit = [this](PointerEvent&) { m_hovered = false; m_pressed = false; mark_needs_paint(); };
        on_pointer_down = [this](PointerEvent& ev) {
            if (ev.button == PointerButton::Primary) {
                m_pressed = true;
                mark_needs_paint();
                ev.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& ev) {
            if (m_pressed) {
                m_pressed = false;
                if (bounds().contains(ev.local_position)) {
                    Clipboard::set_text(m_copy_text);
                    m_copied_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
                    Toast::show(ToastDesc{ .title = "已复制到剪贴板", .message = m_copy_text, .severity = ToastSeverity::Success });
                }
                mark_needs_paint();
                ev.stop_propagation();
            }
        };
    }

    void perform_layout() override {
        m_size = m_constraints.constrain(Vec2{26.0f, 26.0f});
    }

    void paint(PaintContext& context) override {
        auto now = std::chrono::steady_clock::now();
        bool copied = (m_copied_until.time_since_epoch().count() != 0 && now < m_copied_until);
        if (copied) mark_needs_paint();

        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        if (m_hovered || m_pressed || copied) {
            Color bg = copied
                ? Color{0x10 / 255.0f, 0x89 / 255.0f, 0x3E / 255.0f, is_dark ? 0.22f : 0.14f}
                : (is_dark ? Color{1, 1, 1, m_pressed ? 0.05f : 0.09f}
                           : Color{0, 0, 0, m_pressed ? 0.04f : 0.06f});
            context.draw_rect(bounds(), bg, 4.0f, 0.0f);
        }

        const float cx = m_size.x * 0.5f;
        const float cy = m_size.y * 0.5f;
        if (copied) {
            Color ok{0x10 / 255.0f, 0x89 / 255.0f, 0x3E / 255.0f, 1.0f};
            context.draw_line(PointF{cx - 4.5f, cy + 0.5f}, PointF{cx - 1.2f, cy + 3.8f}, ok, 1.6f);
            context.draw_line(PointF{cx - 1.2f, cy + 3.8f}, PointF{cx + 5.0f, cy - 3.5f}, ok, 1.6f);
        } else {
            Color ic = m_hovered ? t.colors.primary : t.colors.text_secondary;
            // 后方纸张轮廓 + 前方纸张轮廓（Fluent 2 标准 Copy 图标）
            context.draw_rect(Rect::from_xywh(cx - 2.5f, cy - 5.5f, 8.5f, 9.5f), ic, 1.5f, 1.1f);
            Color fill_bg = is_dark ? Color{0x22 / 255.0f, 0x23 / 255.0f, 0x26 / 255.0f, 1.0f}
                                    : Color{0xF5 / 255.0f, 0xF7 / 255.0f, 0xFA / 255.0f, 1.0f};
            context.draw_rect(Rect::from_xywh(cx - 5.5f, cy - 2.5f, 8.5f, 9.5f), fill_bg, 1.5f, 0.0f);
            context.draw_rect(Rect::from_xywh(cx - 5.5f, cy - 2.5f, 8.5f, 9.5f), ic, 1.5f, 1.1f);
        }
    }

private:
    std::string m_copy_text;
    bool m_hovered{false};
    bool m_pressed{false};
    std::chrono::steady_clock::time_point m_copied_until{};
};

class CopyIconButtonWidget : public Widget {
public:
    explicit CopyIconButtonWidget(std::string text) : m_text(std::move(text)) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderCopyIconButton>(m_text);
    }
    void update_render_node(std::shared_ptr<RenderBox>) override {}
private:
    std::string m_text;
};

} // namespace

WidgetPtr GalleryApp::make_page_title(const std::string& title, float font_size) {
    return HStack {
        .align_items = AlignItems::Center,
        .spacing = 8.0f,
        .content = {
            Text { .text = title, .font_size = font_size, .font_weight = FontWeight::SemiBold },
            make_widget<CopyIconButtonWidget>(title)
        }
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

WidgetPtr GalleryApp::make_full_page(WidgetPtr body) {
    return Box {
        .background_color = Color::transparent(),
        .padding = EdgeInsets(16.0f, 22.0f),
        .child = std::move(body)
    };
}

WidgetPtr GalleryApp::make_muted(const char* text, float font_size) {
    return Text { .text = text, .font_size = font_size }.bind(&RenderText::set_color, muted_text);
}

WidgetPtr GalleryApp::make_accent(Property<std::string>& prop, float font_size, FontWeight weight) {
    return Text { .font_size = font_size, .font_weight = weight }
        .bind(&RenderText::set_text, prop)
        .bind(&RenderText::set_color, accent_text);
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
        .padding = EdgeInsets(14.0f, 16.0f),
        .child = VStack {
            .align_items = AlignItems::Stretch,
            .spacing = 10.0f,
            .content = {
                make_card(VStack {
                    .spacing = 8.0f,
                    .content = {
                        Text { .font_size = 13.5f }.bind(&RenderText::set_text, *note),
                        Text { .text = "主窗口与子窗口独立合成、共享主题与响应式状态。", .font_size = 12.0f }.bind(&RenderText::set_color, muted_text),
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
    }.bind(&RenderDecoratedBox::set_background_color, window_bg));
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

    struct NavEntry { const char* id; const char* label; const char* badge; };
    auto make_cat = [](const char* id, const char* label, NavigationIcon icon, bool expanded, std::initializer_list<NavEntry> list) {
        NavigationViewItem cat;
        cat.id = id;
        cat.label = label;
        cat.icon = icon;
        cat.badge = std::to_string(list.size());
        cat.expanded = expanded;
        cat.children.reserve(list.size());
        for (const auto& e : list) {
            cat.children.push_back(NavigationViewItem{ .id = e.id, .label = e.label, .badge = e.badge ? e.badge : "" });
        }
        return cat;
    };

    window->set_content(NavigationView {
        .menu_items = {
            NavigationViewItem{ .id = "home", .label = "主页", .icon = NavigationIcon::Home },
            NavigationViewItem::separator(),
            make_cat("cat_layout", "布局系统", NavigationIcon::Layout, false, {
                {"layout_flex",       "Flex (HStack/VStack)", nullptr},
                {"layout_wrap",       "Wrap 流式换行",        nullptr},
                {"layout_grid",       "Grid 二维网格",        nullptr},
                {"layout_zstack",     "ZStack & Positioned",  nullptr},
                {"layout_align",      "Align & AspectRatio",  nullptr},
                {"layout_split_view", "SplitView 可拖拽分隔", nullptr},
                {"layout_dock_panel", "DockPanel 拖拽停靠",   "New"},
                {"layout_visibility", "Visibility 显隐控制",  nullptr}
            }),
            make_cat("cat_basic_input", "基础输入", NavigationIcon::Controls, true, {
                {"button",               "Button",                nullptr},
                {"toggle_button",        "ToggleButton",          nullptr},
                {"pro_hold_confirm",     "HoldConfirmButton",     "New"},
                {"segmented_control",    "SegmentedControl",      nullptr},
                {"dropdown_button",      "DropDownButton",        nullptr},
                {"checkbox",             "CheckBox",              nullptr},
                {"toggle_switch",        "ToggleSwitch",          nullptr},
                {"radio_button",         "RadioButton",           nullptr},
                {"radio_card",           "RadioCard",             nullptr},
                {"combobox",             "ComboBox",              nullptr},
                {"slider",               "Slider",                nullptr},
                {"pro_minimap_time",     "MinimapTimelineSlider", "New"},
                {"calendar_date_picker", "CalendarDatePicker",    nullptr},
                {"time_picker",          "TimePicker",            nullptr},
                {"color_picker",         "ColorPicker",           nullptr},
                {"rating_control",       "RatingControl",         nullptr}
            }),
            make_cat("cat_text", "文本与数值", NavigationIcon::Inputs, false, {
                {"label",              "Label",              nullptr},
                {"rich_text_block",    "RichTextBlock",      "New"},
                {"textbox",            "TextBox",            nullptr},
                {"text_area",          "TextArea",           nullptr},
                {"auto_suggest_box",   "AutoSuggestBox",     nullptr},
                {"pro_ghost_text",     "GhostTextBox",       "New"},
                {"geek_syntax_filter", "SyntaxFilterBar",    "New"},
                {"pro_query_filter",   "QueryFilterBuilder", "New"},
                {"pro_ip_port",        "IpPortInput",        "New"},
                {"password_box",       "PasswordBox",        nullptr},
                {"number_box",         "NumberBox",          nullptr},
                {"pro_scrub_number",   "ScrubNumberBox",     "New"}
            }),
            make_cat("cat_nav", "导航与容器", NavigationIcon::Layout, false, {
                {"navigation_view",     "NavigationView", nullptr},
                {"tab_view",            "TabView",        nullptr},
                {"breadcrumb_bar",      "BreadcrumbBar",  nullptr},
                {"expander",            "Expander",       nullptr},
                {"settings_card",       "SettingsCard",   "New"},
                {"tree_view",           "TreeView",       nullptr},
                {"virtual_list",        "VirtualList",    nullptr},
                {"data_grid",           "DataGrid",       nullptr},
                {"geek_inspector_grid", "InspectorGrid",  "New"},
                {"geek_hex_viewer",     "HexViewer",      "New"},
                {"geek_terminal",       "Terminal",       "New"},
                {"flip_view",           "FlipView",       nullptr},
                {"pips_pager",          "PipsPager",      nullptr}
            }),
            make_cat("cat_menus", "菜单与工具栏", NavigationIcon::Layout, false, {
                {"command_bar",     "CommandBar", nullptr},
                {"menu_flyout",     "MenuFlyout", nullptr},
                {"geek_status_bar", "StatusBar",  "New"}
            }),
            make_cat("cat_status", "弹窗与状态", NavigationIcon::Progress, false, {
                {"content_dialog",     "ContentDialog", nullptr},
                {"teaching_tip",       "TeachingTip",   nullptr},
                {"pro_spotlight_tour", "SpotlightTour", "New"},
                {"tooltip",            "Tooltip",       nullptr},
                {"info_bar",           "InfoBar",       nullptr},
                {"info_badge",         "InfoBadge",     nullptr},
                {"person_picture",     "PersonPicture", nullptr},
                {"toast",              "Toast",         nullptr},
                {"progress_bar",       "ProgressBar",   nullptr},
                {"progress_ring",      "ProgressRing",  nullptr}
            }),
            make_cat("cat_media", "图形与矢量", NavigationIcon::Palette, false, {
                {"svg", "NanoSVG 矢量", nullptr}
            })
        },
        .footer_items = {
            NavigationViewItem{ .id = "about", .label = "关于 KerneTile", .icon = NavigationIcon::Info }
        },
        .pane_title = "KTUI Gallery",
        .is_pane_open = true,
        .is_pane_toggle_button_visible = true,
        .is_back_button_visible = true,
        .is_settings_visible = true,
        .is_search_box_visible = true,
        .search_placeholder = "搜索控件",
        .open_pane_length = 236.0f,
        .page_builder = [this](const std::string& page_id) -> WidgetPtr {
            struct Route { const char* id; WidgetPtr(*fn)(GalleryApp&); };
            static constexpr Route kRoutes[] = {
                {"geek_inspector_grid",  pages::build_inspector_grid_page},
                {"geek_hex_viewer",      pages::build_hex_viewer_page},
                {"geek_terminal",        pages::build_terminal_page},
                {"geek_syntax_filter",   pages::build_syntax_filter_page},
                {"geek_status_bar",      pages::build_status_bar_page},
                {"pro_query_filter",     pages::build_query_filter_page},
                {"pro_scrub_number",     pages::build_scrub_number_box_page},
                {"pro_hold_confirm",     pages::build_hold_confirm_button_page},
                {"pro_ghost_text",       pages::build_ghost_text_box_page},
                {"pro_minimap_time",     pages::build_minimap_timeline_page},
                {"pro_ip_port",          pages::build_ip_port_input_page},
                {"pro_spotlight_tour",   pages::build_spotlight_tour_page},
                {"layout_flex",          pages::build_layout_flex_page},
                {"layout_wrap",          pages::build_layout_wrap_page},
                {"layout_grid",          pages::build_layout_grid_page},
                {"layout_zstack",        pages::build_layout_zstack_page},
                {"layout_align",         pages::build_layout_align_page},
                {"layout_split_view",    pages::build_layout_split_view_page},
                {"layout_dock_panel",    pages::build_layout_dock_panel_page},
                {"layout_visibility",    pages::build_layout_visibility_page},
                {"button",               pages::build_button_page},
                {"toggle_button",        pages::build_toggle_button_page},
                {"segmented_control",    pages::build_segmented_control_page},
                {"dropdown_button",      pages::build_dropdown_button_page},
                {"checkbox",             pages::build_checkbox_page},
                {"toggle_switch",        pages::build_toggle_switch_page},
                {"radio_button",         pages::build_radio_button_page},
                {"radio_card",           pages::build_radio_card_page},
                {"combobox",             pages::build_combobox_page},
                {"slider",               pages::build_slider_page},
                {"calendar_date_picker", pages::build_calendar_date_picker_page},
                {"time_picker",          pages::build_time_picker_page},
                {"color_picker",         pages::build_color_picker_page},
                {"rating_control",       pages::build_rating_control_page},
                {"label",                pages::build_label_page},
                {"rich_text_block",      pages::build_rich_text_block_page},
                {"textbox",              pages::build_textbox_page},
                {"text_area",            pages::build_text_area_page},
                {"auto_suggest_box",     pages::build_auto_suggest_box_page},
                {"password_box",         pages::build_password_box_page},
                {"number_box",           pages::build_number_box_page},
                {"navigation_view",      pages::build_navigation_view_page},
                {"tab_view",             pages::build_tab_view_page},
                {"breadcrumb_bar",       pages::build_breadcrumb_bar_page},
                {"expander",             pages::build_expander_page},
                {"settings_card",        pages::build_settings_card_page},
                {"tree_view",            pages::build_tree_view_page},
                {"virtual_list",         pages::build_virtual_list_page},
                {"data_grid",            pages::build_data_grid_page},
                {"flip_view",            pages::build_flip_view_page},
                {"pips_pager",           pages::build_pips_pager_page},
                {"command_bar",          pages::build_command_bar_page},
                {"menu_flyout",          pages::build_menu_flyout_page},
                {"content_dialog",       pages::build_content_dialog_page},
                {"teaching_tip",         pages::build_teaching_tip_page},
                {"tooltip",              pages::build_tooltip_page},
                {"info_bar",             pages::build_info_bar_page},
                {"info_badge",           pages::build_info_badge_page},
                {"person_picture",       pages::build_person_picture_page},
                {"toast",                pages::build_toast_page},
                {"progress_bar",         pages::build_progress_bar_page},
                {"progress_ring",        pages::build_progress_ring_page},
                {"svg",                  pages::build_svg_page},
                {"__settings__",         pages::build_settings_page},
                {"about",                pages::build_about_page}
            };
            for (const auto& r : kRoutes) {
                if (page_id == r.id) return r.fn(*this);
            }
            return pages::build_home_page(*this);
        }
    }.bind_two_way(nav_selected)
     .bind(&RenderNavigationView::set_pane_display_mode, nav_mode));
}

} // namespace gallery

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

    window->set_content(NavigationView {
        .menu_items = {
            NavigationViewItem{ .id = "home", .label = "主页", .icon = NavigationIcon::Home },
            NavigationViewItem::separator(),
            NavigationViewItem{
                .id = "cat_layout", .label = "布局系统", .icon = NavigationIcon::Layout, .badge = "8", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "layout_flex",       .label = "Flex (HStack/VStack)" },
                    NavigationViewItem{ .id = "layout_wrap",       .label = "Wrap 流式换行" },
                    NavigationViewItem{ .id = "layout_grid",       .label = "Grid 二维网格" },
                    NavigationViewItem{ .id = "layout_zstack",     .label = "ZStack & Positioned" },
                    NavigationViewItem{ .id = "layout_align",      .label = "Align & AspectRatio" },
                    NavigationViewItem{ .id = "layout_split_view", .label = "SplitView 可拖拽分隔" },
                    NavigationViewItem{ .id = "layout_dock_panel", .label = "DockPanel 拖拽停靠", .badge = "New" },
                    NavigationViewItem{ .id = "layout_visibility", .label = "Visibility 显隐控制" }
                }
            },
            NavigationViewItem{
                .id = "cat_basic_input", .label = "基础输入", .icon = NavigationIcon::Controls, .badge = "16", .expanded = true,
                .children = {
                    NavigationViewItem{ .id = "button",               .label = "Button" },
                    NavigationViewItem{ .id = "toggle_button",        .label = "ToggleButton" },
                    NavigationViewItem{ .id = "pro_hold_confirm",     .label = "HoldConfirmButton",    .badge = "New" },
                    NavigationViewItem{ .id = "segmented_control",    .label = "SegmentedControl" },
                    NavigationViewItem{ .id = "dropdown_button",      .label = "DropDownButton" },
                    NavigationViewItem{ .id = "checkbox",             .label = "CheckBox" },
                    NavigationViewItem{ .id = "toggle_switch",        .label = "ToggleSwitch" },
                    NavigationViewItem{ .id = "radio_button",         .label = "RadioButton" },
                    NavigationViewItem{ .id = "radio_card",           .label = "RadioCard" },
                    NavigationViewItem{ .id = "combobox",             .label = "ComboBox" },
                    NavigationViewItem{ .id = "slider",               .label = "Slider" },
                    NavigationViewItem{ .id = "pro_minimap_time",     .label = "MinimapTimelineSlider",.badge = "New" },
                    NavigationViewItem{ .id = "calendar_date_picker", .label = "CalendarDatePicker" },
                    NavigationViewItem{ .id = "time_picker",          .label = "TimePicker" },
                    NavigationViewItem{ .id = "color_picker",         .label = "ColorPicker" },
                    NavigationViewItem{ .id = "rating_control",       .label = "RatingControl" }
                }
            },
            NavigationViewItem{
                .id = "cat_text", .label = "文本与数值", .icon = NavigationIcon::Inputs, .badge = "11", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "label",              .label = "Label" },
                    NavigationViewItem{ .id = "textbox",            .label = "TextBox" },
                    NavigationViewItem{ .id = "text_area",          .label = "TextArea" },
                    NavigationViewItem{ .id = "auto_suggest_box",   .label = "AutoSuggestBox" },
                    NavigationViewItem{ .id = "pro_ghost_text",     .label = "GhostTextBox",       .badge = "New" },
                    NavigationViewItem{ .id = "geek_syntax_filter", .label = "SyntaxFilterBar",    .badge = "New" },
                    NavigationViewItem{ .id = "pro_query_filter",   .label = "QueryFilterBuilder", .badge = "New" },
                    NavigationViewItem{ .id = "pro_ip_port",        .label = "IpPortInput",        .badge = "New" },
                    NavigationViewItem{ .id = "password_box",       .label = "PasswordBox" },
                    NavigationViewItem{ .id = "number_box",         .label = "NumberBox" },
                    NavigationViewItem{ .id = "pro_scrub_number",   .label = "ScrubNumberBox",     .badge = "New" }
                }
            },
            NavigationViewItem{
                .id = "cat_nav", .label = "导航与容器", .icon = NavigationIcon::Layout, .badge = "11", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "navigation_view",     .label = "NavigationView" },
                    NavigationViewItem{ .id = "tab_view",            .label = "TabView" },
                    NavigationViewItem{ .id = "breadcrumb_bar",      .label = "BreadcrumbBar" },
                    NavigationViewItem{ .id = "expander",            .label = "Expander" },
                    NavigationViewItem{ .id = "tree_view",           .label = "TreeView" },
                    NavigationViewItem{ .id = "virtual_list",        .label = "VirtualList" },
                    NavigationViewItem{ .id = "data_grid",           .label = "DataGrid" },
                    NavigationViewItem{ .id = "geek_inspector_grid", .label = "InspectorGrid", .badge = "New" },
                    NavigationViewItem{ .id = "geek_hex_viewer",     .label = "HexViewer",     .badge = "New" },
                    NavigationViewItem{ .id = "flip_view",           .label = "FlipView" },
                    NavigationViewItem{ .id = "pips_pager",          .label = "PipsPager" }
                }
            },
            NavigationViewItem{
                .id = "cat_menus", .label = "菜单与工具栏", .icon = NavigationIcon::Layout, .badge = "3", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "command_bar",     .label = "CommandBar" },
                    NavigationViewItem{ .id = "menu_flyout",     .label = "MenuFlyout" },
                    NavigationViewItem{ .id = "geek_status_bar", .label = "StatusBar", .badge = "New" }
                }
            },
            NavigationViewItem{
                .id = "cat_status", .label = "弹窗与状态", .icon = NavigationIcon::Progress, .badge = "10", .expanded = false,
                .children = {
                    NavigationViewItem{ .id = "content_dialog",     .label = "ContentDialog" },
                    NavigationViewItem{ .id = "teaching_tip",       .label = "TeachingTip" },
                    NavigationViewItem{ .id = "pro_spotlight_tour", .label = "SpotlightTour", .badge = "New" },
                    NavigationViewItem{ .id = "tooltip",            .label = "Tooltip" },
                    NavigationViewItem{ .id = "info_bar",           .label = "InfoBar" },
                    NavigationViewItem{ .id = "info_badge",         .label = "InfoBadge" },
                    NavigationViewItem{ .id = "person_picture",     .label = "PersonPicture" },
                    NavigationViewItem{ .id = "toast",              .label = "Toast" },
                    NavigationViewItem{ .id = "progress_bar",       .label = "ProgressBar" },
                    NavigationViewItem{ .id = "progress_ring",      .label = "ProgressRing" }
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
        .pane_title = "KTUI Gallery",
        .is_pane_open = true,
        .is_pane_toggle_button_visible = true,
        .is_back_button_visible = true,
        .is_settings_visible = true,
        .is_search_box_visible = true,
        .search_placeholder = "搜索控件",
        .open_pane_length = 236.0f,
        .page_builder = [this](const std::string& page_id) -> WidgetPtr {
            if (page_id == "geek_inspector_grid")     return pages::build_inspector_grid_page(*this);
            if (page_id == "geek_hex_viewer")         return pages::build_hex_viewer_page(*this);
            if (page_id == "geek_syntax_filter")      return pages::build_syntax_filter_page(*this);
            if (page_id == "geek_status_bar")         return pages::build_status_bar_page(*this);
            if (page_id == "pro_query_filter")     return pages::build_query_filter_page(*this);
            if (page_id == "pro_scrub_number")     return pages::build_scrub_number_box_page(*this);
            if (page_id == "pro_hold_confirm")     return pages::build_hold_confirm_button_page(*this);
            if (page_id == "pro_ghost_text")       return pages::build_ghost_text_box_page(*this);
            if (page_id == "pro_minimap_time")     return pages::build_minimap_timeline_page(*this);
            if (page_id == "pro_ip_port")          return pages::build_ip_port_input_page(*this);
            if (page_id == "pro_spotlight_tour")   return pages::build_spotlight_tour_page(*this);
            if (page_id == "layout_flex")          return pages::build_layout_flex_page(*this);
            if (page_id == "layout_wrap")          return pages::build_layout_wrap_page(*this);
            if (page_id == "layout_grid")          return pages::build_layout_grid_page(*this);
            if (page_id == "layout_zstack")        return pages::build_layout_zstack_page(*this);
            if (page_id == "layout_align")         return pages::build_layout_align_page(*this);
            if (page_id == "layout_split_view")    return pages::build_layout_split_view_page(*this);
            if (page_id == "layout_dock_panel")    return pages::build_layout_dock_panel_page(*this);
            if (page_id == "layout_visibility")    return pages::build_layout_visibility_page(*this);
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
            if (page_id == "tooltip")              return pages::build_tooltip_page(*this);
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
    }.bind_two_way(nav_selected)
     .bind(&RenderNavigationView::set_pane_display_mode, nav_mode));
}

} // namespace gallery

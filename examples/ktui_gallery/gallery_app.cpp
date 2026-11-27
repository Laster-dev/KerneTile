#include "gallery_app.hpp"
#include "pages/gallery_pages.hpp"
#if defined(__ANDROID__)
#include "mobile_shell.hpp"
#endif

using namespace ktui;

namespace gallery {

void GalleryApp::refresh_count() {
    int step = step_ten.get() ? 10 : 1;
    count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" +
                   (locked.get() ? "  [已锁定]" : ""));
}

namespace {

// 响应式内边距包装器：宽屏保持桌面留白，窄屏 (< breakpoint) 自动收缩为紧凑移动端边距
class RenderResponsivePadding : public RenderBox {
public:
    RenderResponsivePadding(EdgeInsets wide_pad, EdgeInsets narrow_pad, float breakpoint) noexcept
        : m_wide(wide_pad), m_narrow(narrow_pad), m_breakpoint(breakpoint) {}

    void set_child_box(std::shared_ptr<RenderBox> child) {
        if (m_child == child) return;
        if (m_child) remove_child(m_child.get());
        m_child = std::move(child);
        if (m_child) append_child(m_child.get());
        mark_needs_layout();
    }

    void perform_layout() override {
        const bool is_narrow = m_constraints.has_bounded_width() && (m_constraints.max_width < m_breakpoint);
        const EdgeInsets pad = is_narrow ? m_narrow : m_wide;
        if (!m_child) {
            m_size = m_constraints.constrain(Vec2{pad.horizontal(), pad.vertical()});
            return;
        }
        BoxConstraints inner = m_constraints.deflate(pad);
        m_child->layout(inner, true);
        m_child->set_offset(Vec2{pad.left, pad.top});
        m_size = m_constraints.constrain(Vec2{
            m_child->size().x + pad.horizontal(),
            m_child->size().y + pad.vertical()
        });
    }

private:
    EdgeInsets m_wide;
    EdgeInsets m_narrow;
    float m_breakpoint{540.0f};
    std::shared_ptr<RenderBox> m_child;
};

class ResponsivePaddingWidget : public Widget {
public:
    ResponsivePaddingWidget(EdgeInsets wide_pad, EdgeInsets narrow_pad, float breakpoint, WidgetPtr child)
        : m_wide(wide_pad), m_narrow(narrow_pad), m_breakpoint(breakpoint), m_child(std::move(child)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderResponsivePadding>(m_wide, m_narrow, m_breakpoint);
        m_child_box = m_child ? m_child->create_render_node() : nullptr;
        if (m_child && m_child_box) m_child->set_render_node(m_child_box);
        node->set_child_box(m_child_box);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto rp = std::static_pointer_cast<RenderResponsivePadding>(node)) {
            if (m_child) {
                if (!m_child_box) {
                    m_child_box = m_child->create_render_node();
                    m_child->set_render_node(m_child_box);
                    rp->set_child_box(m_child_box);
                } else {
                    m_child->update_render_node(m_child_box);
                }
            } else if (m_child_box) {
                m_child_box = nullptr;
                rp->set_child_box(nullptr);
            }
        }
    }

private:
    EdgeInsets m_wide;
    EdgeInsets m_narrow;
    float m_breakpoint;
    WidgetPtr m_child;
    std::shared_ptr<RenderBox> m_child_box;
};

} // namespace

WidgetPtr GalleryApp::make_card(WidgetPtr child) {
    return Box {
        .border_width = 1.0f,
        .corner_radius = 6.0f,
        .padding = EdgeInsets::zero(),
        .theme_card = true,
        .child = make_widget<ResponsivePaddingWidget>(
            EdgeInsets(12.0f, 14.0f), EdgeInsets(10.0f, 10.0f), 540.0f, std::move(child))
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
#if defined(__ANDROID__)
    font_size = std::min(font_size, 19.0f);
#endif
    return HStack {
        .align_items = AlignItems::Center,
        .spacing = 8.0f,
        .content = {
            Flexible {
                .child = Text {
                    .text = title,
                    .font_size = font_size,
                    .font_weight = FontWeight::SemiBold,
                    .overflow = TextOverflow::Wrap
                }
            },
            make_widget<CopyIconButtonWidget>(title)
        }
    };
}

WidgetPtr GalleryApp::make_scroll_page(WidgetPtr body) {
    return ScrollView {
        .child = make_widget<ResponsivePaddingWidget>(
            EdgeInsets(16.0f, 22.0f), EdgeInsets(12.0f, 12.0f), 540.0f, std::move(body))
    };
}

WidgetPtr GalleryApp::make_full_page(WidgetPtr body) {
    return make_widget<ResponsivePaddingWidget>(
        EdgeInsets(16.0f, 22.0f), EdgeInsets(12.0f, 12.0f), 540.0f, std::move(body));
}

WidgetPtr GalleryApp::make_muted(const char* text, float font_size) {
    return Text { .text = text, .font_size = font_size, .overflow = TextOverflow::Wrap }
        .bind(&RenderText::set_color, muted_text);
}

WidgetPtr GalleryApp::make_accent(Property<std::string>& prop, float font_size, FontWeight weight) {
    return Text { .font_size = font_size, .font_weight = weight, .overflow = TextOverflow::Wrap }
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
    window->titlebar().set_pin_button_visible(true);
    window->titlebar().set_theme_button_visible(true);
    window->titlebar().add_action("+", "新建工具窗口", [this]() { spawn_child_window(); });
#endif

    // 监听操作系统级文件拖放事件 (WM_DROPFILES / HTML5 Drop)
    window->on_files_dropped([this](const std::vector<std::string>& files, PointF pos) {
        if (files.empty()) return;
        std::string list;
        bool loaded_img = false;
        for (size_t i = 0; i < files.size(); ++i) {
            if (i > 0) list += "\n";
            list += std::to_string(i + 1) + ". " + files[i];
            if (!loaded_img) {
                size_t dot = files[i].find_last_of('.');
                if (dot != std::string::npos) {
                    std::string ext = files[i].substr(dot);
                    for (char& c : ext) {
                        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                    }
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" || ext == ".bmp") {
                        if (auto img = ImageBuffer::load_file(files[i]); img && img->valid()) {
                            demo_img_custom = img;
                            image_active_src.set(img);
                            std::string anim = img->is_animated()
                                ? (" 动图 " + std::to_string(img->frame_count()) + " 帧")
                                : "";
                            image_status.set("已通过拖放加载图像 (" + std::to_string(img->width) + "x" +
                                             std::to_string(img->height) + anim + "): " + files[i]);
                            loaded_img = true;
                        }
                    }
                }
            }
        }
        os_drop_files.set(list);
        os_drop_status.set("窗口级 on_files_dropped 收到 " + std::to_string(files.size()) +
                           " 个文件 @(" + std::to_string(static_cast<int>(pos.x)) + ", " +
                           std::to_string(static_cast<int>(pos.y)) + ")" +
                           (loaded_img ? " — 已自动解码首个图片到 Image 预览与壁纸演示" : ""));
        Toast::show(ToastDesc{
            .title = "收到操作系统文件拖放",
            .message = "共 " + std::to_string(files.size()) + " 个文件" +
                       (loaded_img ? "（已自动加载图片预览）" : ""),
            .severity = ToastSeverity::Informational
        });
    });

    auto build_page_by_id = [this](const std::string& page_id) -> WidgetPtr {
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
            {"tactile_font_gamma",      pages::build_tactile_font_gamma_page},
            {"tactile_directional_rim", pages::build_tactile_directional_rim_page},
            {"tactile_oklab",           pages::build_tactile_oklab_page},
            {"tactile_acrylic_grain",   pages::build_tactile_acrylic_grain_page},
            {"tactile_inset_shadow",    pages::build_tactile_inset_shadow_page},
            {"tactile_metaballs",       pages::build_tactile_metaballs_page},
            {"tactile_scroll_feather",    pages::build_tactile_scroll_feather_page},
            {"tactile_hero_transition",   pages::build_tactile_hero_transition_page},
            {"tactile_micro_tilt",        pages::build_tactile_micro_tilt_page},
            {"tactile_spring_caret",      pages::build_tactile_spring_caret_page},
            {"tactile_scroll_parallax",   pages::build_tactile_scroll_parallax_page},
            {"tactile_rolling_counter",   pages::build_tactile_rolling_counter_page},
            {"tactile_concentric_radius", pages::build_tactile_concentric_radius_page},
            {"polish_showcase",         pages::build_polish_showcase_page},
            {"squircle",             pages::build_squircle_page},
            {"image",                pages::build_image_page},
            {"file_dialog",          pages::build_file_dialog_page},
            {"drag_drop",            pages::build_drag_drop_page},
            {"svg",                  pages::build_svg_page},
            {"__settings__",         pages::build_settings_page},
            {"about",                pages::build_about_page}
        };
        for (const auto& r : kRoutes) {
            if (page_id == r.id) return r.fn(*this);
        }
        return pages::build_home_page(*this);
    };

#if defined(__ANDROID__)
    window->set_content(mobile::build_mobile_shell(*this, std::move(build_page_by_id)));
#else
    struct NavEntry { const char* id; const char* label; const char* badge; };
    auto make_cat = [](const char* id, const char* label, NavigationIcon icon, bool expanded, std::initializer_list<NavEntry> list) {
        NavigationViewItem cat;
        cat.id = id;
        cat.label = label;
        cat.icon = icon;
        cat.expanded = expanded;
        cat.children.reserve(list.size());
        for (const auto& e : list) {
            cat.children.push_back(NavigationViewItem{ .id = e.id, .label = e.label });
        }
        return cat;
    };

    window->set_content(NavigationView {
        .menu_items = {
            NavigationViewItem{ .id = "home", .label = "主页", .icon = NavigationIcon::Home },
            make_cat("cat_tactile", "质感交互", NavigationIcon::Sparkles, true, {
                {"polish_showcase",         "质感与微交互特性",   "New"},
                {"squircle",                "Apple 超椭圆圆角",   "New"},
                {"tactile_font_gamma",      "文字光渗补偿",       "New"},
                {"tactile_directional_rim", "方向性玻璃高光",     "New"},
                {"tactile_oklab",           "OkLab 色彩插值",     "New"},
                {"tactile_acrylic_grain",   "零纹理亚克力微颗粒", "New"},
                {"tactile_inset_shadow",    "解析高斯内阴影",     "New"},
                {"tactile_metaballs",       "灵动岛液态融合",     "New"},
                {"tactile_scroll_feather",    "列表边缘渐隐羽化",   "New"},
                {"tactile_hero_transition",   "共享元素转场",       "New"},
                {"tactile_micro_tilt",        "微倾斜与按压缩放",   "New"},
                {"tactile_spring_caret",      "弹簧光滑输入光标",   "New"},
                {"tactile_scroll_parallax",   "滚动吸顶与阻尼视差", "New"},
                {"tactile_rolling_counter",   "翻滚数值与微交叉",   "New"},
                {"tactile_concentric_radius", "同心嵌套圆角几何",   "New"}
            }),
            NavigationViewItem::header("控件"),
            make_cat("cat_basic_input", "基本输入", NavigationIcon::BasicInput, true, {
                {"button",               "Button",                nullptr},
                {"toggle_button",        "ToggleButton",          nullptr},
                {"pro_hold_confirm",     "HoldConfirmButton",     "New"},
                {"dropdown_button",      "DropDownButton",        nullptr},
                {"segmented_control",    "SegmentedControl",      nullptr},
                {"checkbox",             "CheckBox",              nullptr},
                {"toggle_switch",        "ToggleSwitch",          nullptr},
                {"radio_button",         "RadioButton",           nullptr},
                {"radio_card",           "RadioCard",             nullptr},
                {"combobox",             "ComboBox",              nullptr},
                {"slider",               "Slider",                nullptr},
                {"pro_minimap_time",     "MinimapTimelineSlider", "New"},
                {"color_picker",         "ColorPicker",           nullptr},
                {"rating_control",       "RatingControl",         nullptr}
            }),
            make_cat("cat_collections", "集合", NavigationIcon::Collections, false, {
                {"tree_view",           "TreeView",      nullptr},
                {"virtual_list",        "VirtualList",   nullptr},
                {"data_grid",           "DataGrid",      nullptr},
                {"flip_view",           "FlipView",      nullptr},
                {"geek_inspector_grid", "InspectorGrid", "New"},
                {"geek_hex_viewer",     "HexViewer",     "New"}
            }),
            make_cat("cat_datetime", "日期和时间", NavigationIcon::DateTime, false, {
                {"calendar_date_picker", "CalendarDatePicker", nullptr},
                {"time_picker",          "TimePicker",         nullptr}
            }),
            make_cat("cat_dialogs", "对话框和浮出控件", NavigationIcon::Dialogs, false, {
                {"content_dialog",     "ContentDialog",     nullptr},
                {"file_dialog",        "FileDialog 文件框", "New"},
                {"teaching_tip",       "TeachingTip",       nullptr},
                {"pro_spotlight_tour", "SpotlightTour",     "New"}
            }),
            make_cat("cat_layout", "布局", NavigationIcon::LayoutPanel, false, {
                {"layout_flex",       "Flex (HStack/VStack)", nullptr},
                {"layout_wrap",       "Wrap 流式换行",        nullptr},
                {"layout_grid",       "Grid 二维网格",        nullptr},
                {"layout_zstack",     "ZStack & Positioned",  nullptr},
                {"layout_align",      "Align & AspectRatio",  nullptr},
                {"layout_split_view", "SplitView 可拖拽分隔", nullptr},
                {"layout_dock_panel", "DockPanel 拖拽停靠",   "New"},
                {"layout_visibility", "Visibility 显隐控制",  nullptr},
                {"expander",          "Expander",             nullptr},
                {"settings_card",     "SettingsCard",         "New"}
            }),
            make_cat("cat_media", "媒体", NavigationIcon::Media, false, {
                {"image",               "Image 位图与动图", "New"},
                {"svg",                 "NanoSVG 矢量",     nullptr},
                {"person_picture",      "PersonPicture",    nullptr}
            }),
            make_cat("cat_menus", "菜单和工具栏", NavigationIcon::MenusToolbars, false, {
                {"command_bar",     "CommandBar", nullptr},
                {"menu_flyout",     "MenuFlyout", nullptr},
                {"geek_status_bar", "StatusBar",  "New"}
            }),
            make_cat("cat_nav", "导航", NavigationIcon::Navigation, false, {
                {"navigation_view", "NavigationView", nullptr},
                {"tab_view",        "TabView",        nullptr},
                {"breadcrumb_bar",  "BreadcrumbBar",  nullptr},
                {"pips_pager",      "PipsPager",      nullptr}
            }),
            make_cat("cat_status", "状态和信息", NavigationIcon::Info, false, {
                {"info_bar",      "InfoBar",      nullptr},
                {"info_badge",    "InfoBadge",    nullptr},
                {"progress_bar",  "ProgressBar",  nullptr},
                {"progress_ring", "ProgressRing", nullptr},
                {"toast",         "Toast",        nullptr},
                {"tooltip",       "Tooltip",      nullptr}
            }),
            make_cat("cat_system", "系统", NavigationIcon::System, false, {
                {"drag_drop",     "Drag & Drop 拖放", "New"},
                {"geek_terminal", "Terminal",         "New"}
            }),
            make_cat("cat_text", "文本", NavigationIcon::Text, false, {
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
        .page_builder = std::move(build_page_by_id)
    }.bind_two_way(nav_selected)
     .bind(&RenderNavigationView::set_pane_display_mode, nav_mode));
#endif
}

} // namespace gallery

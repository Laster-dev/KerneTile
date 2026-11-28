#pragma once

#include "gallery_app.hpp"
#include "pages/gallery_pages.hpp"
#include <ktui/presentation/svg_engine.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gallery::mobile {

using namespace ktui;

// 24x24 Fluent/Material 风格矢量图标路径
static constexpr const char* kIconHome =
    "M12 3.2 3.5 10.2a1 1 0 0 0 .64 1.77H6v7.53A1.5 1.5 0 0 0 7.5 21h3.25v-5.25h2.5V21h3.25a1.5 1.5 0 0 0 1.5-1.5v-7.53h1.86a1 1 0 0 0 .64-1.77L12 3.2Z";
static constexpr const char* kIconGrid =
    "M4 4h6.5v6.5H4V4Zm9.5 0H20v6.5h-6.5V4ZM4 13.5h6.5V20H4v-6.5Zm9.5 0H20V20h-6.5v-6.5Z";
static constexpr const char* kIconSparkle =
    "M12 2.5 14.3 9l6.5 2.3-6.5 2.3L12 20.1l-2.3-6.5L3.2 11.3 9.7 9 12 2.5Z";
static constexpr const char* kIconSettings =
    "M12 8.5a3.5 3.5 0 1 0 0 7 3.5 3.5 0 0 0 0-7Zm8.94 2.5h-1.8a7.4 7.4 0 0 0-.77-1.86l1.27-1.27a1 1 0 0 0 0-1.41l-1.1-1.1a1 1 0 0 0-1.41 0l-1.27 1.27a7.4 7.4 0 0 0-1.86-.77v-1.8a1 1 0 0 0-1-1h-1.56a1 1 0 0 0-1 1v1.8a7.4 7.4 0 0 0-1.86.77L6.21 5.36a1 1 0 0 0-1.41 0l-1.1 1.1a1 1 0 0 0 0 1.41l1.27 1.27a7.4 7.4 0 0 0-.77 1.86H2.4a1 1 0 0 0-1 1v1.56a1 1 0 0 0 1 1h1.8c.16.65.42 1.28.77 1.86L3.7 17.7a1 1 0 0 0 0 1.41l1.1 1.1a1 1 0 0 0 1.41 0l1.27-1.27c.58.35 1.21.61 1.86.77v1.8a1 1 0 0 0 1 1h1.56a1 1 0 0 0 1-1v-1.8c.65-.16 1.28-.42 1.86-.77l1.27 1.27a1 1 0 0 0 1.41 0l1.1-1.1a1 1 0 0 0 0-1.41l-1.27-1.27c.35-.58.61-1.21.77-1.86h1.8a1 1 0 0 0 1-1v-1.56a1 1 0 0 0-1-1Z";
static constexpr const char* kIconMoon =
    "M20.2 14.7A8.5 8.5 0 0 1 9.3 3.8a.8.8 0 0 0-1.05-.95A9.5 9.5 0 1 0 21.15 15.75a.8.8 0 0 0-.95-1.05Z";
static constexpr const char* kIconSun =
    "M12 7a5 5 0 1 0 0 10 5 5 0 0 0 0-10Zm0-4.5v2.2M12 19.3v2.2M4.93 4.93l1.56 1.56M17.51 17.51l1.56 1.56M2.5 12h2.2M19.3 12h2.2M4.93 19.07l1.56-1.56M17.51 6.49l1.56-1.56";

struct MobileRouteMeta {
    const char* id;
    const char* title;
    const char* desc;
};

inline std::string resolve_page_title(const std::string& id) {
    if (id == "mobile_home" || id == "home") return "KTUI Gallery";
    if (id == "mobile_controls") return "控件展厅";
    if (id == "mobile_tactile") return "质感交互";
    if (id == "__settings__") return "设置";
    if (id == "about") return "关于 KerneTile";

    static constexpr std::pair<const char*, const char*> kTitles[] = {
        {"polish_showcase", "质感与微交互特性"},
        {"squircle", "Apple 超椭圆圆角"},
        {"tactile_font_gamma", "文字光渗补偿"},
        {"tactile_directional_rim", "方向性玻璃高光"},
        {"tactile_oklab", "OkLab 色彩插值"},
        {"tactile_acrylic_grain", "零纹理亚克力微颗粒"},
        {"tactile_inset_shadow", "解析高斯内阴影"},
        {"tactile_metaballs", "灵动岛液态融合"},
        {"tactile_scroll_feather", "列表边缘渐隐羽化"},
        {"tactile_hero_transition", "共享元素转场"},
        {"tactile_micro_tilt", "微倾斜与按压缩放"},
        {"tactile_spring_caret", "弹簧光滑输入光标"},
        {"tactile_scroll_parallax", "滚动吸顶与阻尼视差"},
        {"tactile_rolling_counter", "翻滚数值与微交叉"},
        {"tactile_concentric_radius", "同心嵌套圆角几何"},
        {"button", "Button"},
        {"toggle_button", "ToggleButton"},
        {"pro_hold_confirm", "HoldConfirmButton"},
        {"dropdown_button", "DropDownButton"},
        {"segmented_control", "SegmentedControl"},
        {"checkbox", "CheckBox"},
        {"toggle_switch", "ToggleSwitch"},
        {"radio_button", "RadioButton"},
        {"radio_card", "RadioCard"},
        {"combobox", "ComboBox"},
        {"slider", "Slider"},
        {"pro_minimap_time", "MinimapTimelineSlider"},
        {"color_picker", "ColorPicker"},
        {"rating_control", "RatingControl"},
        {"tree_view", "TreeView"},
        {"virtual_list", "VirtualList"},
        {"data_grid", "DataGrid"},
        {"flip_view", "FlipView"},
        {"geek_inspector_grid", "InspectorGrid"},
        {"geek_hex_viewer", "HexViewer"},
        {"calendar_date_picker", "CalendarDatePicker"},
        {"time_picker", "TimePicker"},
        {"content_dialog", "ContentDialog"},
        {"file_dialog", "FileDialog"},
        {"teaching_tip", "TeachingTip"},
        {"pro_spotlight_tour", "SpotlightTour"},
        {"layout_flex", "Flex (HStack / VStack)"},
        {"layout_wrap", "Wrap 流式换行"},
        {"layout_grid", "Grid 二维网格"},
        {"layout_zstack", "ZStack & Positioned"},
        {"layout_align", "Align & AspectRatio"},
        {"layout_split_view", "SplitView"},
        {"layout_dock_panel", "DockPanel"},
        {"layout_visibility", "Visibility"},
        {"expander", "Expander"},
        {"settings_card", "SettingsCard"},
        {"image", "Image 位图与动图"},
        {"svg", "NanoSVG 矢量"},
        {"person_picture", "PersonPicture"},
        {"command_bar", "CommandBar"},
        {"menu_flyout", "MenuFlyout"},
        {"geek_status_bar", "StatusBar"},
        {"navigation_view", "NavigationView"},
        {"tab_view", "TabView"},
        {"breadcrumb_bar", "BreadcrumbBar"},
        {"pips_pager", "PipsPager"},
        {"info_bar", "InfoBar"},
        {"info_badge", "InfoBadge"},
        {"progress_bar", "ProgressBar"},
        {"progress_ring", "ProgressRing"},
        {"toast", "Toast"},
        {"tooltip", "Tooltip"},
        {"drag_drop", "Drag & Drop"},
        {"geek_terminal", "Terminal"},
        {"label", "Label"},
        {"rich_text_block", "RichTextBlock"},
        {"textbox", "TextBox"},
        {"text_area", "TextArea"},
        {"auto_suggest_box", "AutoSuggestBox"},
        {"pro_ghost_text", "GhostTextBox"},
        {"geek_syntax_filter", "SyntaxFilterBar"},
        {"pro_query_filter", "QueryFilterBuilder"},
        {"pro_ip_port", "IpPortInput"},
        {"password_box", "PasswordBox"},
        {"number_box", "NumberBox"},
        {"pro_scrub_number", "ScrubNumberBox"}
    };
    for (const auto& kv : kTitles) {
        if (id == kv.first) return kv.second;
    }
    return id;
}

inline bool is_root_tab_id(const std::string& id) {
    return id == "mobile_home" || id == "home" ||
           id == "mobile_controls" ||
           id == "mobile_tactile" ||
           id == "__settings__";
}

inline int root_tab_index(const std::string& id) {
    if (id == "mobile_home" || id == "home") return 0;
    if (id == "mobile_controls") return 1;
    if (id == "mobile_tactile") return 2;
    if (id == "__settings__") return 3;
    return -1;
}

// 移动端首页：专为竖屏手机设计的概览卡片与快捷入口
inline WidgetPtr build_mobile_home_page(GalleryApp& app) {
    auto feature_card = [&app](const char* title, const char* sub, const char* route_id) -> WidgetPtr {
        return SettingsCard {
            .header = title,
            .description = sub,
            .is_click_enabled = true,
            .on_click = [&app, id = std::string(route_id)]() {
                app.nav_selected.set(id);
            }
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            Box {
                .background_color = Color{0.08f, 0.34f, 0.66f, 0.24f},
                .border_color = Color{0.32f, 0.68f, 1.0f, 0.34f},
                .border_width = 1.0f,
                .corner_radius = 12.0f,
                .padding = EdgeInsets(14.0f, 14.0f),
                .child = VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 6.0f,
                    .content = {
                        Text { .text = "KerneTile KTUI Mobile", .font_size = 18.0f, .font_weight = FontWeight::SemiBold },
#if defined(__EMSCRIPTEN__)
                        app.make_muted("WebGPU Compute 16×16 瓦片渲染引擎 · 移动端自适应展厅 · 全控件触控与物理惯性适配", 12.0f),
#else
                        app.make_muted("Vulkan Compute 16×16 瓦片渲染引擎 · 原生移动端自适应展厅 · 全控件触控与物理惯性适配", 12.0f),
#endif
                        Wrap {
                            .spacing = 8.0f,
                            .run_spacing = 8.0f,
                            .content = {
                                Button {
                                    .text = "浏览全部控件",
                                    .variant = ButtonVariant::Accent,
                                    .on_click = [&app]() { app.nav_selected.set("mobile_controls"); }
                                },
                                Button {
                                    .text = "质感交互实验室",
                                    .variant = ButtonVariant::Standard,
                                    .on_click = [&app]() { app.nav_selected.set("mobile_tactile"); }
                                }
                            }
                        }
                    }
                }
            },
            Text { .text = "精选推荐", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
            feature_card("Apple 超椭圆圆角 (Squircle)", "G² 连续曲率 SDF 与实时滑块对比", "squircle"),
            feature_card("文字光渗补偿 (Font Gamma)", "深浅色自适应 Gamma 锐利中英混排", "tactile_font_gamma"),
            feature_card("共享元素转场 (Hero)", "物理弹簧跨状态无缝飞行过渡", "tactile_hero_transition"),
            feature_card("Button & 触控反馈", "Fluent 2 按钮家族与按压微动效", "button"),
            feature_card("Slider 滑块家族", "单滑块、刻度步进、双柄区间与竖向滑块", "slider"),
            feature_card("FlipView & PipsPager", "触控左右轻扫轮播与页码联动", "flip_view"),
            feature_card("ContentDialog & Toast", "高斯软阴影模态框与悬浮通知吐司", "content_dialog"),
            feature_card("SettingsCard 响应式设置卡", "窄屏自动上下堆叠与多行描述折行", "settings_card"),
            feature_card("关于 KerneTile", "引擎架构与仓库信息", "about")
        }
    });
}

// 移动端控件目录页：清晰分组折叠列表，点选进入控件详情页
inline WidgetPtr build_mobile_controls_page(GalleryApp& app) {
    struct Entry { const char* id; const char* title; const char* desc; };
    auto make_group = [&app](const char* title, const char* desc, bool expanded, std::initializer_list<Entry> entries) -> WidgetPtr {
        std::vector<WidgetPtr> items;
        items.reserve(entries.size());
        for (const auto& e : entries) {
            items.push_back(SettingsCard {
                .header = e.title,
                .description = e.desc,
                .is_click_enabled = true,
                .is_nested = true,
                .on_click = [&app, id = std::string(e.id)]() {
                    app.nav_selected.set(id);
                }
            });
        }
        return SettingsExpander {
            .header = title,
            .description = desc,
            .expanded = expanded,
            .items = std::move(items)
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 8.0f,
        .content = {
            app.make_muted("按分类浏览 KTUI 控件库，点击任意条目进入交互演示，左上角返回。", 12.0f),
            make_group("基础输入 (Basic Input)", "按钮、开关、单选、下拉框、滑块、取色器与评分", true, {
                {"button", "Button", "标准 / 强调 / 危险 / 幽灵按钮"},
                {"toggle_button", "ToggleButton", "状态切换按钮"},
                {"pro_hold_confirm", "HoldConfirmButton", "长按进度确认安全按钮"},
                {"dropdown_button", "DropDownButton", "下拉与拆分按钮"},
                {"segmented_control", "SegmentedControl", "平滑滑块分段选择器"},
                {"checkbox", "CheckBox", "三态复选框"},
                {"toggle_switch", "ToggleSwitch", "物理弹簧开关"},
                {"radio_button", "RadioButton", "单选按钮组"},
                {"radio_card", "RadioCard", "图标卡片单选"},
                {"combobox", "ComboBox", "下拉选择器"},
                {"slider", "Slider", "单滑块 / 区间滑块 / 竖向滑块"},
                {"pro_minimap_time", "MinimapTimelineSlider", "缩略图时间轴滑块"},
                {"color_picker", "ColorPicker", "HSV 色盘与吸管取色"},
                {"rating_control", "RatingControl", "五星/半星触控评分"}
            }),
            make_group("文本与数值 (Text & Numbers)", "输入框、多行编辑、密码框、自动补全与数值步进", false, {
                {"label", "Label", "文本排版与溢出策略"},
                {"rich_text_block", "RichTextBlock", "多样式富文本"},
                {"textbox", "TextBox", "单行输入与触控长按菜单"},
                {"text_area", "TextArea", "多行编辑与 IME 输入"},
                {"auto_suggest_box", "AutoSuggestBox", "实时联想搜索框"},
                {"pro_ghost_text", "GhostTextBox", "幽灵补全提示框"},
                {"password_box", "PasswordBox", "按住预览密码框"},
                {"number_box", "NumberBox", "表达式求值数值框"},
                {"pro_scrub_number", "ScrubNumberBox", "水平拖拽微调数值"},
                {"pro_ip_port", "IpPortInput", "IP 与端口分段输入"},
                {"geek_syntax_filter", "SyntaxFilterBar", "语法过滤栏"},
                {"pro_query_filter", "QueryFilterBuilder", "可视化查询条件构建"}
            }),
            make_group("集合与数据 (Collections)", "树形视图、十万行虚拟列表、表格与轮播", false, {
                {"flip_view", "FlipView", "触控滑动轮播"},
                {"tree_view", "TreeView", "层级树与勾选联动"},
                {"virtual_list", "VirtualList", "100,000 行 O(1) 虚拟化列表"},
                {"data_grid", "DataGrid", "排序与多列数据表"},
                {"geek_inspector_grid", "InspectorGrid", "紧凑属性检查器"},
                {"geek_hex_viewer", "HexViewer", "十六进制字节流查看器"}
            }),
            make_group("布局系统 (Layout)", "Flex、Wrap、Grid、ZStack、Expander 与设置卡片", false, {
                {"layout_flex", "Flex (HStack / VStack)", "弹性盒与窄屏防溢出折行"},
                {"layout_wrap", "Wrap 流式换行", "自适应多行流式容器"},
                {"layout_grid", "Grid 二维网格", "自适应列数二维网格"},
                {"layout_zstack", "ZStack & Positioned", "层叠与锚点定位"},
                {"layout_align", "Align & AspectRatio", "对齐与宽高比锁定"},
                {"layout_split_view", "SplitView", "可拖拽双栏分隔"},
                {"layout_dock_panel", "DockPanel", "多面板停靠容器"},
                {"layout_visibility", "Visibility", "显隐与占位控制"},
                {"expander", "Expander", "手风琴折叠面板"},
                {"settings_card", "SettingsCard", "WinUI 3 响应式设置卡片"}
            }),
            make_group("弹窗、状态与媒体 (Dialogs, Status & Media)", "对话框、吐司、进度条、日期时间与图片矢量", false, {
                {"content_dialog", "ContentDialog", "模态对话框"},
                {"toast", "Toast", "多方位悬浮通知吐司"},
                {"teaching_tip", "TeachingTip", "气泡引导提示"},
                {"pro_spotlight_tour", "SpotlightTour", "聚光灯新手引导"},
                {"info_bar", "InfoBar", "语义消息横幅"},
                {"info_badge", "InfoBadge", "数字与状态角标"},
                {"progress_bar", "ProgressBar", "确定与不确定进度条"},
                {"progress_ring", "ProgressRing", "环形加载指示器"},
                {"calendar_date_picker", "CalendarDatePicker", "日历日期选择"},
                {"time_picker", "TimePicker", "24h / 12h 时间选择"},
                {"image", "Image 位图与动图", "位图缩放与 GIF 动图"},
                {"svg", "NanoSVG 矢量", "无损矢量路径渲染"},
                {"person_picture", "PersonPicture", "圆形头像与状态角标"}
            }),
            make_group("导航、菜单与系统 (Navigation & System)", "标签页、面包屑、菜单栏与终端", false, {
                {"tab_view", "TabView", "多标签页容器"},
                {"breadcrumb_bar", "BreadcrumbBar", "路径面包屑"},
                {"pips_pager", "PipsPager", "触控翻页指示点"},
                {"navigation_view", "NavigationView", "多模式导航视图"},
                {"command_bar", "CommandBar", "工具栏与溢出菜单"},
                {"menu_flyout", "MenuFlyout", "上下文浮出菜单"},
                {"geek_status_bar", "StatusBar", "状态栏"},
                {"geek_terminal", "Terminal", "内置终端仿真视图"}
            })
        }
    });
}

// 移动端质感交互页
inline WidgetPtr build_mobile_tactile_page(GalleryApp& app) {
    struct Entry { const char* id; const char* title; const char* desc; };
    static constexpr Entry kItems[] = {
        {"polish_showcase", "质感与微交互特性", "全局视觉与动效总览"},
        {"squircle", "Apple 超椭圆圆角", "G² 连续曲率 SDF 与折痕对比"},
        {"tactile_font_gamma", "文字光渗补偿", "深浅背景自适应 Gamma 字重校准"},
        {"tactile_directional_rim", "方向性玻璃高光", "物理菲涅尔方向高光内描边"},
        {"tactile_oklab", "OkLab 色彩插值", "感知均匀色域平滑过渡"},
        {"tactile_acrylic_grain", "零纹理亚克力微颗粒", "过程式无纹理噪点材质"},
        {"tactile_inset_shadow", "解析高斯内阴影", "解析 SDF 凹槽内阴影"},
        {"tactile_metaballs", "灵动岛液态融合", "实时 SDF Metaball 融球动效"},
        {"tactile_scroll_feather", "列表边缘渐隐羽化", "滚动容器上下平滑渐隐"},
        {"tactile_hero_transition", "共享元素转场", "SpringSolver 跨状态连续飞行"},
        {"tactile_micro_tilt", "微倾斜与按压缩放", "触控微按压与弹性回弹"},
        {"tactile_spring_caret", "弹簧光滑输入光标", "阻尼追踪输入光标"},
        {"tactile_scroll_parallax", "滚动吸顶与阻尼视差", "Sticky Header 与视差封面"},
        {"tactile_rolling_counter", "翻滚数值与微交叉", "数字滚轮与交叉切换动效"},
        {"tactile_concentric_radius", "同心嵌套圆角几何", "内外圆角严格同心几何计算"}
    };

    std::vector<WidgetPtr> cards;
    cards.push_back(app.make_muted("KerneTile 底层光学与物理质感实验室，点击任意实验卡片体验。", 12.0f));
    for (const auto& e : kItems) {
        cards.push_back(SettingsCard {
            .header = e.title,
            .description = e.desc,
            .is_click_enabled = true,
            .on_click = [&app, id = std::string(e.id)]() {
                app.nav_selected.set(id);
            }
        });
    }

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 8.0f,
        .content = std::move(cards)
    });
}

// Android 移动端主壳层：顶部 AppBar (52px) + 自适应页面内容区 + 底部四栏 TabBar (58px)
class RenderMobileGalleryShell : public RenderBox {
public:
    static constexpr float kTopBarH = 52.0f;
    static constexpr float kBottomBarH = 58.0f;

    using PageBuilder = std::function<WidgetPtr(const std::string&)>;

    RenderMobileGalleryShell(GalleryApp& app, PageBuilder builder)
        : m_app(app), m_builder(std::move(builder)) {
        m_history.push_back("mobile_home");
        setup_events();

        m_bindings.subscribe(m_app.nav_selected, [this](const std::string& next_id) {
            if (!m_syncing) {
                navigate_to(next_id, false);
            }
        });
        navigate_to("mobile_home", true);
    }

    ~RenderMobileGalleryShell() override {
        if (m_page_box) remove_child(m_page_box.get());
    }

    void navigate_to(std::string id, bool sync_prop = true) {
        if (id.empty() || id == "home") id = "mobile_home";
        if (id == m_current_id && m_page_box) return;

        int tab = root_tab_index(id);
        if (tab >= 0) {
            m_active_tab = tab;
            m_history.clear();
            m_history.push_back(id);
        } else {
            if (m_history.empty() || m_history.back() != id) {
                m_history.push_back(id);
            }
        }

        m_current_id = id;
        m_current_title = resolve_page_title(id);

        if (sync_prop) {
            m_syncing = true;
            m_app.nav_selected.set(id);
            m_syncing = false;
        }

        WidgetPtr w;
        if (id == "mobile_home") w = build_mobile_home_page(m_app);
        else if (id == "mobile_controls") w = build_mobile_controls_page(m_app);
        else if (id == "mobile_tactile") w = build_mobile_tactile_page(m_app);
        else if (m_builder) w = m_builder(id);

        if (m_page_box) {
            remove_child(m_page_box.get());
            m_page_box.reset();
        }
        m_page_widget = std::move(w);
        if (m_page_widget) {
            m_page_box = m_page_widget->create_render_node();
            if (m_page_box) append_child(m_page_box.get());
        }
        mark_needs_layout();
        mark_needs_paint();
    }

    void go_back() {
        if (m_history.size() > 1) {
            m_history.pop_back();
            std::string prev = m_history.back();
            m_current_id.clear();
            navigate_to(prev, true);
        } else if (!is_root_tab_id(m_current_id)) {
            static constexpr const char* kTabIds[4] = {"mobile_home", "mobile_controls", "mobile_tactile", "__settings__"};
            navigate_to(kTabIds[std::clamp(m_active_tab, 0, 3)], true);
        }
    }

    void perform_layout() override {
        float w = std::isfinite(m_constraints.max_width) ? m_constraints.max_width : 390.0f;
        float h = std::isfinite(m_constraints.max_height) ? m_constraints.max_height : 800.0f;
        set_size(m_constraints.constrain(Vec2{w, h}));

        float content_h = std::max(0.0f, size().y - kTopBarH - kBottomBarH);
        if (m_page_box) {
            m_page_box->layout(BoxConstraints::tight(Vec2{size().x, content_h}), false);
            m_page_box->set_offset(Vec2{0.0f, kTopBarH});
        }
    }

    bool hit_test(HitTestResult& result, PointF p) override {
        if (!bounds().contains(p)) return false;
        if (p.y < kTopBarH || p.y >= size().y - kBottomBarH) {
            result.add(this, p);
            return true;
        }
        if (m_page_box) {
            PointF cp{p.x - m_page_box->offset().x, p.y - m_page_box->offset().y};
            if (m_page_box->hit_test(result, cp)) return true;
        }
        result.add(this, p);
        return true;
    }

    bool hit_test_touch(HitTestResult& result, PointF p) override {
        if (!bounds().contains(p)) return false;
        if (p.y < kTopBarH || p.y >= size().y - kBottomBarH) {
            result.add(this, p);
            return true;
        }
        if (m_page_box) {
            PointF cp{p.x - m_page_box->offset().x, p.y - m_page_box->offset().y};
            if (m_page_box->hit_test_touch(result, cp)) return true;
        }
        result.add(this, p);
        return true;
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const float w = size().x;
        const float h = size().y;

        const Color bg = t.is_dark
            ? Color{0x1C / 255.0f, 0x1D / 255.0f, 0x20 / 255.0f, 1.0f}
            : Color{0xEF / 255.0f, 0xF2 / 255.0f, 0xF6 / 255.0f, 1.0f};
        const Color bar_bg = t.is_dark
            ? Color{0x24 / 255.0f, 0x25 / 255.0f, 0x29 / 255.0f, 1.0f}
            : Color{0xFA / 255.0f, 0xFB / 255.0f, 0xFD / 255.0f, 1.0f};
        const Color divider = t.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.08f}
            : Color{0.0f, 0.0f, 0.0f, 0.08f};
        const Color press_bg = t.is_dark
            ? Color{1.0f, 1.0f, 1.0f, 0.10f}
            : Color{0.0f, 0.0f, 0.0f, 0.07f};

        ctx.draw_rect(bounds(), bg, 0.0f, 0.0f);

        // 1. 内容区（严格裁剪防止越界遮挡顶/底栏）
        const float content_h = std::max(0.0f, h - kTopBarH - kBottomBarH);
        if (m_page_box && content_h > 0.0f) {
            ctx.push_clip(Rect::from_xywh(0.0f, kTopBarH, w, content_h), 0.0f);
            PaintContext::OffsetScope scope(ctx, m_page_box->offset());
            m_page_box->paint(ctx);
            ctx.pop_clip();
        }

        // 2. 顶部 Mobile AppBar (52px)
        ctx.draw_rect(Rect::from_xywh(0.0f, 0.0f, w, kTopBarH), bar_bg, 0.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, kTopBarH - 1.0f, w, 1.0f), divider, 0.0f, 0.0f);

        const bool show_back = !is_root_tab_id(m_current_id);
        float title_x = 16.0f;
        if (show_back) {
            Rect br = Rect::from_xywh(4.0f, 4.0f, 44.0f, 44.0f);
            if (m_pressed_part == 1) {
                ctx.draw_rect(br, press_bg, 8.0f, 0.0f);
            }
            const float cx = br.min_x + 22.0f;
            const float cy = br.min_y + 22.0f;
            const Color ic = t.colors.text_primary;
            ctx.draw_line(PointF{cx + 6.0f, cy}, PointF{cx - 6.0f, cy}, ic, 1.8f);
            ctx.draw_line(PointF{cx - 6.0f, cy}, PointF{cx - 0.5f, cy - 5.5f}, ic, 1.8f);
            ctx.draw_line(PointF{cx - 6.0f, cy}, PointF{cx - 0.5f, cy + 5.5f}, ic, 1.8f);
            title_x = 52.0f;
        }

        // 右侧一键深浅主题切换按钮 (44x44 触控热区)
        Rect theme_r = Rect::from_xywh(w - 48.0f, 4.0f, 44.0f, 44.0f);
        if (m_pressed_part == 2) {
            ctx.draw_rect(theme_r, press_bg, 8.0f, 0.0f);
        }
        Rect theme_ic = Rect::from_xywh(theme_r.min_x + 12.0f, theme_r.min_y + 12.0f, 20.0f, 20.0f);
        SvgEngine::draw_svg_path(ctx, t.is_dark ? kIconSun : kIconMoon, 24.0f, 24.0f, theme_ic, t.colors.text_primary);

        // 标题居中/左对齐裁剪防撞击按钮
        float title_w = std::max(40.0f, theme_r.min_x - title_x - 8.0f);
        ctx.push_clip(Rect::from_xywh(title_x, 0.0f, title_w, kTopBarH), 0.0f);
        Vec2 tsz = TextMeasure::measure(m_current_title, 16.5f, FontWeight::SemiBold);
        ctx.draw_text(m_current_title,
                      PointF{title_x, std::round((kTopBarH - tsz.y) * 0.5f)},
                      16.5f, t.colors.text_primary, FontWeight::SemiBold);
        ctx.pop_clip();

        // 3. 底部四栏 TabBar (58px)
        const float by = h - kBottomBarH;
        ctx.draw_rect(Rect::from_xywh(0.0f, by, w, kBottomBarH), bar_bg, 0.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, by, w, 1.0f), divider, 0.0f, 0.0f);

        struct TabDef { const char* label; const char* icon; };
        static constexpr TabDef kTabs[4] = {
            {"首页", kIconHome},
            {"控件", kIconGrid},
            {"质感", kIconSparkle},
            {"设置", kIconSettings}
        };

        const float tab_w = w / 4.0f;
        const Color active_pill = t.is_dark
            ? Color{t.colors.primary.r, t.colors.primary.g, t.colors.primary.b, 0.22f}
            : Color{t.colors.primary.r, t.colors.primary.g, t.colors.primary.b, 0.14f};

        for (int i = 0; i < 4; ++i) {
            const float tx = tab_w * static_cast<float>(i);
            const float cx = tx + tab_w * 0.5f;
            const bool active = (m_active_tab == i);
            const bool pressed = (m_pressed_part == 10 + i);

            if (active) {
                ctx.draw_rect(Rect::from_xywh(std::round(cx - 26.0f), by + 7.0f, 52.0f, 28.0f), active_pill, 14.0f, 0.0f);
            } else if (pressed) {
                ctx.draw_rect(Rect::from_xywh(std::round(cx - 26.0f), by + 7.0f, 52.0f, 28.0f), press_bg, 14.0f, 0.0f);
            }

            const Color fg = active ? t.colors.primary : t.colors.text_secondary;
            Rect ic_r = Rect::from_xywh(std::round(cx - 9.5f), by + 11.5f, 19.0f, 19.0f);
            SvgEngine::draw_svg_path(ctx, kTabs[i].icon, 24.0f, 24.0f, ic_r, fg);

            const FontWeight fw = active ? FontWeight::SemiBold : FontWeight::Regular;
            Vec2 lsz = TextMeasure::measure(kTabs[i].label, 11.0f, fw);
            ctx.draw_text(kTabs[i].label,
                          PointF{std::round(cx - lsz.x * 0.5f), by + 38.0f},
                          11.0f, fg, fw);
        }
    }

private:
    int hit_part(PointF p) const noexcept {
        if (p.y < kTopBarH) {
            if (!is_root_tab_id(m_current_id) && p.x < 52.0f) return 1; // Back
            if (p.x >= size().x - 52.0f) return 2; // Theme toggle
            return 0;
        }
        if (p.y >= size().y - kBottomBarH) {
            int idx = std::clamp(static_cast<int>(p.x / std::max(1.0f, size().x / 4.0f)), 0, 3);
            return 10 + idx;
        }
        return 0;
    }

    void setup_events() {
        on_pointer_down = [this](PointerEvent& e) {
            if (e.button != PointerButton::Primary) return;
            int part = hit_part(e.local_position);
            if (part != 0) {
                m_pressed_part = part;
                mark_needs_paint();
                e.stop_propagation();
            }
        };
        on_pointer_up = [this](PointerEvent& e) {
            if (m_pressed_part == 0) return;
            int pressed = m_pressed_part;
            m_pressed_part = 0;
            mark_needs_paint();
            if (hit_part(e.local_position) == pressed) {
                if (pressed == 1) {
                    go_back();
                } else if (pressed == 2 && m_app.app) {
                    bool is_dark = m_app.app->current_theme().is_dark;
                    m_app.app->set_theme(is_dark ? Theme::light() : Theme::dark());
                } else if (pressed >= 10 && pressed <= 13) {
                    static constexpr const char* kTabIds[4] = {"mobile_home", "mobile_controls", "mobile_tactile", "__settings__"};
                    navigate_to(kTabIds[pressed - 10], true);
                }
            }
            e.stop_propagation();
        };
        on_pointer_cancel = [this](PointerEvent&) {
            if (m_pressed_part != 0) {
                m_pressed_part = 0;
                mark_needs_paint();
            }
        };
    }

    GalleryApp& m_app;
    PageBuilder m_builder;
    BindingGroup m_bindings;
    bool m_syncing{false};
    int m_active_tab{0};
    int m_pressed_part{0};
    std::string m_current_id;
    std::string m_current_title{"KTUI Gallery"};
    std::vector<std::string> m_history;
    WidgetPtr m_page_widget;
    std::shared_ptr<RenderBox> m_page_box;
};

class MobileGalleryShellWidget : public Widget {
public:
    MobileGalleryShellWidget(GalleryApp& app, RenderMobileGalleryShell::PageBuilder builder)
        : m_app(app), m_builder(std::move(builder)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderMobileGalleryShell>(m_app, m_builder);
    }
    void update_render_node(std::shared_ptr<RenderBox>) override {}

private:
    GalleryApp& m_app;
    RenderMobileGalleryShell::PageBuilder m_builder;
};

inline WidgetPtr build_mobile_shell(GalleryApp& app, RenderMobileGalleryShell::PageBuilder builder) {
    return make_widget<MobileGalleryShellWidget>(app, std::move(builder));
}

} // namespace gallery::mobile

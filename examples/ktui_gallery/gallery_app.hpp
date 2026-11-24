#pragma once

#include <ktui/ktui.hpp>
#include <memory>
#include <string>
#include <vector>

namespace gallery {

struct GalleryApp {
    ktui::Application* app{nullptr};
    std::shared_ptr<ktui::Window> window;
    int count{0};
    int child_window_seq{1};
    std::vector<std::shared_ptr<ktui::Property<std::string>>> child_notes;

    ktui::Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    ktui::Property<bool> step_ten{false};
    ktui::Property<bool> locked{false};
    ktui::Property<int> radio_btn_val{1};
    ktui::Property<int> radio_card_val{1};
    ktui::Property<std::string> input_text{"KerneTile KTUI Gallery"};
    ktui::Property<std::string> input_preview{"输入预览: KerneTile KTUI Gallery"};
    ktui::Property<std::string> password_val{"KerneTile2026"};
    ktui::Property<std::string> password_status{"密码长度: 13 字符 (按住右侧眼睛可临时预览)"};
    ktui::Property<double> number_int_val{42.0};
    ktui::Property<double> number_float_val{3.14};
    ktui::Property<std::string> number_status{"当前数值: 42 / 3.14 (支持输入 12+8*2 按 Enter 求值)"};
    ktui::Property<std::string> commandbar_status{"CommandBar 就绪: 点击命令项或右侧 ... 查看溢出/二级菜单"};
    ktui::Property<std::string> toast_status{"点击下方按钮弹出 Windows 11 风格 Toast 通知"};
    ktui::Property<std::string> demo_nav_selected{"overview"};
    ktui::Property<ktui::NavigationPaneDisplayMode> demo_nav_mode{ktui::NavigationPaneDisplayMode::Left};
    ktui::Property<int> demo_nav_mode_idx{0};
    ktui::Property<bool> expander_open_1{true};
    ktui::Property<bool> expander_open_2{false};
    ktui::Property<std::vector<ktui::BreadcrumbItem>> breadcrumb_items{{
        ktui::BreadcrumbItem{ .id = "root", .label = "此电脑" },
        ktui::BreadcrumbItem{ .id = "d", .label = "D:\\" },
        ktui::BreadcrumbItem{ .id = "kt", .label = "KerneTile" },
        ktui::BreadcrumbItem{ .id = "ktui", .label = "KTUI" },
        ktui::BreadcrumbItem{ .id = "widgets", .label = "widgets" }
    }};
    ktui::Property<std::string> breadcrumb_status{"点击任意前置路径节点即可回溯，或点击重置恢复完整路径"};
    ktui::Property<std::vector<ktui::TabViewItem>> tab_items{{
        ktui::TabViewItem{ .id = "tab_1", .header = "欢迎页", .is_closable = false },
        ktui::TabViewItem{ .id = "tab_2", .header = "renderer.cpp", .is_closable = true },
        ktui::TabViewItem{ .id = "tab_3", .header = "tab_view.hpp", .is_closable = true }
    }};
    ktui::Property<int> tab_selected{0};
    int tab_seq{4};
    ktui::Property<std::string> suggest_query{""};
    ktui::Property<std::string> suggest_status{"输入关键字实时过滤建议，支持 ↑/↓/Enter 或点击选择"};
    ktui::Property<std::string> tree_single_selected{"src_ktui"};
    ktui::Property<std::string> tree_single_status{"当前选中节点: KTUI (src_ktui)"};
    ktui::Property<std::string> tree_multi_status{"已勾选 2 个组件 (点击父节点可一键全选/取消)"};
    ktui::Property<ktui::CalendarDate> picked_date{ktui::CalendarDate::today()};
    ktui::Property<std::string> date_status{"当前选中日期: " + ktui::CalendarDate::today().format_iso()};
    ktui::Property<ktui::Color> picked_color{ktui::Color{0.0f, 120.0f / 255.0f, 212.0f / 255.0f, 1.0f}};
    ktui::Property<std::string> color_status{"当前颜色: #FF0078D4 (拖拽色盘或滑条实时预览)"};
    ktui::Property<float> rating_val{4.0f};
    ktui::Property<float> rating_half_val{3.5f};
    ktui::Property<std::string> rating_caption{"4.0 / 5.0"};
    ktui::Property<std::string> rating_half_caption{"3.5 / 5.0 (半星模式)"};
    ktui::Property<std::string> tip_status{"点击下方按钮弹出带尾标或居中的 WinUI 3 引导气泡"};
    ktui::Property<std::string> theme_btn_text{"切换浅色主题"};
    ktui::Property<float> slider_val{45.0f};
    ktui::Property<float> progress_norm{0.45f};
    ktui::Property<std::string> slider_text{"调节数值 (滑块): 45%"};
    ktui::Property<int> combo_idx{0};
    ktui::Property<std::string> combo_preview{"已选后端: Direct3D 12 Compute"};
    ktui::Property<std::string> nav_selected{"home"};
    ktui::Property<ktui::NavigationPaneDisplayMode> nav_mode{ktui::NavigationPaneDisplayMode::Auto};
    ktui::Property<int> nav_mode_idx{0};
    ktui::Property<bool> menu_auto_save{true};
    ktui::Property<std::string> menu_status{"提示: 点击按钮下拉箭头或在下方卡片右键弹出菜单"};
    ktui::Property<std::string> dialog_status{"尚未打开对话框"};
    ktui::Property<bool> infobar_open{true};
    ktui::Property<ktui::InfoBarSeverity> infobar_severity{ktui::InfoBarSeverity::Informational};

    ktui::Property<ktui::Color> window_bg{ktui::Color::transparent()};
    ktui::Property<ktui::Color> card_bg{ktui::Color{1.0f, 1.0f, 1.0f, 0.045f}};
    ktui::Property<ktui::Color> card_border{ktui::Color{1.0f, 1.0f, 1.0f, 0.07f}};
    ktui::Property<ktui::Color> muted_text{ktui::Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f}};
    ktui::Property<ktui::Color> accent_text{ktui::Color{96.0f / 255.0f, 205.0f / 255.0f, 255.0f / 255.0f, 1.0f}};

    void refresh_count();
    void spawn_child_window();

    [[nodiscard]] ktui::WidgetPtr make_card(ktui::WidgetPtr child);
    [[nodiscard]] ktui::WidgetPtr make_scroll_page(ktui::WidgetPtr body);

    void setup(ktui::Application& application, std::shared_ptr<ktui::Window> win);
};

} // namespace gallery

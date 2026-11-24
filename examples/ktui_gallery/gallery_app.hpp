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
    ktui::Property<std::string> theme_btn_text{"切换浅色主题"};
    ktui::Property<float> slider_val{45.0f};
    ktui::Property<float> progress_norm{0.45f};
    ktui::Property<std::string> slider_text{"调节数值 (滑块): 45%"};
    ktui::Property<int> combo_idx{0};
    ktui::Property<std::string> combo_preview{"已选后端: Direct3D 12 Compute"};
    ktui::Property<std::string> nav_selected{"home"};
    ktui::Property<ktui::NavigationPaneDisplayMode> nav_mode{ktui::NavigationPaneDisplayMode::Auto};
    ktui::Property<int> nav_mode_idx{0};

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

#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/widgets/widget.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ktui {

// WinUI 3 NavigationView 面板展示模式
enum class NavigationPaneDisplayMode : uint8_t {
    Auto = 0,        // 响应式自适应 (宽屏 Expanded / 中屏 Compact / 窄屏 Minimal)
    Left = 1,        // 左侧固定展开 (点击汉堡键在 Expanded <-> Compact 间切换)
    LeftCompact = 2, // 左侧紧凑图标栏 (48px)
    LeftMinimal = 3, // 极简隐藏模式 (仅显示汉堡键，点击以浮层滑出)
    Top = 4          // 顶部水平导航栏模式 (底部 3px 强调色滑动指示条)
};

// 导航项类型
enum class NavigationItemKind : uint8_t {
    Item = 0,
    Header = 1,
    Separator = 2
};

// 内置 WinUI 3 矢量线性图标 (无需外部字体/位图即可像素级锐利绘制)
enum class NavigationIcon : uint8_t {
    None = 0,
    Home,
    Controls,
    Inputs,
    Progress,
    ComboBox,
    Layout,
    Palette,
    Settings,
    Info,
    Star,
    Search,
    Folder,
    Code,
    Sparkles
};

// 导航项数据描述 (支持层级子项、InfoBadge 徽标、禁用态、自定义图标绘制器)
struct NavigationViewItem {
    std::string id;                // 唯一标识 (若为空则自动回退到 label)
    std::string label;             // 显示文本
    NavigationItemKind kind{NavigationItemKind::Item};
    NavigationIcon icon{NavigationIcon::None};
    std::string badge;             // 可选 WinUI 3 InfoBadge 徽标文本 (如 "3", "New")
    bool disabled{false};
    bool expanded{false};          // 含子项时是否默认展开
    std::vector<NavigationViewItem> children; // 层级子导航项
    std::function<void(PaintContext&, Rect, Color)> icon_painter{nullptr};

    static NavigationViewItem header(std::string title) {
        NavigationViewItem it;
        it.kind = NavigationItemKind::Header;
        it.label = std::move(title);
        return it;
    }

    static NavigationViewItem separator() {
        NavigationViewItem it;
        it.kind = NavigationItemKind::Separator;
        return it;
    }

    [[nodiscard]] const std::string& effective_id() const noexcept {
        return id.empty() ? label : id;
    }
};

// NavigationView 数据层契约
struct NavigationViewData {
    std::vector<NavigationViewItem> menu_items;
    std::vector<NavigationViewItem> footer_items;

    std::string selected_id;
    std::string pane_title;
    std::string header;

    NavigationPaneDisplayMode pane_display_mode{NavigationPaneDisplayMode::Auto};
    bool is_pane_open{true};
    bool is_pane_toggle_button_visible{true};
    bool is_back_button_visible{true};
    bool is_back_enabled{false};
    bool is_settings_visible{true};
    std::string settings_label{"设置"};
    std::string settings_id{"__settings__"};

    float open_pane_length{240.0f};             // 展开宽度 (WinUI 3 默认 240~320px)
    float compact_pane_length{48.0f};           // 紧凑图标栏宽度 (WinUI 3 48px)
    float compact_mode_threshold_width{641.0f}; // WinUI 3 CompactModeThresholdWidth
    float expanded_mode_threshold_width{900.0f};// WinUI 3 ExpandedModeThresholdWidth

    std::function<void(const std::string&)> on_selection_changed{nullptr};
    std::function<void()> on_back_requested{nullptr};
    std::function<void(bool)> on_pane_toggled{nullptr};
};

} // namespace ktui

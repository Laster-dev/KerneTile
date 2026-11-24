#pragma once

#include <ktui/pal/platform_types.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ktui {

class RenderBox;

// 对标 WinUI 3 AutomationControlType / ARIA Role 语义角色枚举
enum class AccessibilityRole : uint8_t {
    None = 0,
    Window,
    Group,
    Button,
    ToggleButton,
    CheckBox,
    RadioButton,
    Switch,
    TextBox,
    TextArea,
    PasswordBox,
    NumberBox,
    Slider,
    ComboBox,
    Menu,
    MenuItem,
    Tab,
    TabItem,
    Tree,
    TreeItem,
    List,
    ListItem,
    Dialog,
    Tooltip,
    ProgressBar,
    Heading,
    Text,
    Navigation,
    Link,
    Image
};

// 对标 WinUI 3 AutomationLiveSetting / ARIA aria-live
enum class LiveRegionSetting : uint8_t {
    Off = 0,
    Polite = 1,
    Assertive = 2
};

// 对标 WinUI 3 AutomationProperties + UIA Control Patterns (Toggle / Value / RangeValue / ExpandCollapse / SelectionItem)
struct AccessibilityProperties {
    AccessibilityRole role{AccessibilityRole::None};
    std::string name;          // AutomationProperties.Name
    std::string help_text;     // AutomationProperties.HelpText (如 Tooltip 描述)
    std::string value;         // ValuePattern.Value (密码框自动保持空串保护隐私)
    std::optional<bool> checked;   // TogglePattern.ToggleState
    std::optional<bool> expanded;  // ExpandCollapsePattern.ExpandCollapseState
    std::optional<bool> selected;  // SelectionItemPattern.IsSelected
    bool disabled{false};
    bool is_password{false};
    bool focusable{false};
    bool focused{false};
    LiveRegionSetting live_region{LiveRegionSetting::Off};
    std::optional<double> range_min;
    std::optional<double> range_max;
    std::optional<double> range_value;
    Rect screen_bounds{};
};

// 语义自动化树节点快照 (供 UIA / AT-SPI / Web ARIA 桥接与自动化无障碍验证)
struct AccessibilityNode {
    const RenderBox* source{nullptr};
    AccessibilityProperties props;
    std::vector<AccessibilityNode> children;
};

// 全局无障碍服务 (支持屏幕阅读器 LiveRegion 播报队列、高对比度状态与自动化树构建)
class AccessibilityService {
public:
    static AccessibilityService& global() noexcept {
        static AccessibilityService instance;
        return instance;
    }

    void announce(std::string message, LiveRegionSetting politeness = LiveRegionSetting::Polite) {
        if (message.empty()) return;
        m_last_announcement = message;
        m_last_politeness = politeness;
        if (m_announcer) {
            m_announcer(message, politeness);
        }
    }

    void set_announcer(std::function<void(const std::string&, LiveRegionSetting)> cb) {
        m_announcer = std::move(cb);
    }

    [[nodiscard]] const std::string& last_announcement() const noexcept { return m_last_announcement; }
    [[nodiscard]] LiveRegionSetting last_politeness() const noexcept { return m_last_politeness; }

    void set_high_contrast(bool enabled) noexcept { m_high_contrast = enabled; }
    [[nodiscard]] bool is_high_contrast() const noexcept { return m_high_contrast; }

    // 从场景树根节点构建完整 WinUI 3 语义自动化树 (实现位于 render_box.hpp 后)
    static AccessibilityNode build_tree(const RenderBox* root, const RenderBox* focused_node = nullptr);

private:
    AccessibilityService() = default;

    std::string m_last_announcement;
    LiveRegionSetting m_last_politeness{LiveRegionSetting::Off};
    std::function<void(const std::string&, LiveRegionSetting)> m_announcer;
    bool m_high_contrast{false};
};

} // namespace ktui

#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/pal/font_resolver.hpp>
#include <string>

namespace ktui {

// 桌面专业工具紧凑字阶体系 (Desktop Typography Tokens)
struct TypographyTokens {
    float xs = 11.0f;       // 角标、代码行号、快捷键标注、状态栏
    float sm = 12.0f;       // 次级正文、工具栏小按钮、属性表格
    float base = 13.0f;     // 桌面标准正文、输入框、标准按钮 (IDE基准)
    float lg = 15.0f;       // 属性折叠头、组标题
    float xl = 16.0f;       // 面板小标题、侧边栏标题
    float xxl = 18.0f;      // 模态对话框标题
    float xxxl = 22.0f;     // 页面一级主标题
    float xxxxl = 28.0f;    // 欢迎屏 Display 标题

    float line_height_factor = 1.35f; // 紧凑行高倍数
    std::string font_family = "Segoe UI, -apple-system, BlinkMacSystemFont, Roboto, sans-serif";

    bool operator==(const TypographyTokens&) const noexcept = default;
};

// 桌面高信息密度紧凑间距体系 (Desktop Spacing Tokens: 杜绝移动端大间距)
struct SpacingTokens {
    float none = 0.0f;
    float xxs = 2.0f;       // 微间距、指示条边距
    float xs = 4.0f;        // 紧凑间距、图标文字间隙
    float sm = 6.0f;        // 控件间标准间距、列表项间隙
    float md = 8.0f;        // 组间距、工具栏内边距
    float lg = 12.0f;       // 面板内边距、卡片边距
    float xl = 16.0f;       // 大区块间隔
    float xxl = 24.0f;      // 窗口边框主留白
    float xxxl = 32.0f;     // 宽幅间隔

    bool operator==(const SpacingTokens&) const noexcept = default;
};

// 桌面硬朗微圆角体系 (Desktop Corner Radius Tokens: 严禁大圆角与大胶囊)
struct CornerRadiusTokens {
    float none = 0.0f;      // 绝对直角 (选项卡、状态栏、边框贴边)
    float xs = 2.0f;        // 紧凑微圆角 (复选框、标签、徽标)
    float sm = 3.0f;        // 标准桌面控件 (标准按钮、输入框、下拉框)
    float md = 4.0f;        // 桌面面板、卡片容器
    float lg = 6.0f;        // 浮动面板、右键上下文菜单外框
    float xl = 8.0f;        // 模态窗口最大圆角 (严格 <= 8px)
    float full = 12.0f;     // 桌面圆角上限 (杜绝移动端巨型跑马场)

    bool operator==(const CornerRadiusTokens&) const noexcept = default;
};

// 桌面精细微阴影描述符
struct ShadowDef {
    Vec2 offset{0.0f, 1.0f};
    float blur_radius{2.0f};
    float spread{0.0f};
    Color color{0.0f, 0.0f, 0.0f, 0.12f};

    bool operator==(const ShadowDef&) const noexcept = default;
};

// 桌面克制阴影深度体系 (Desktop Elevation Shadow Tokens)
struct ShadowTokens {
    ShadowDef none{Vec2{0.0f, 0.0f}, 0.0f, 0.0f, Color::transparent()};
    ShadowDef sm{Vec2{0.0f, 1.0f}, 2.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.08f}};
    ShadowDef md{Vec2{0.0f, 2.0f}, 4.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.12f}};
    ShadowDef lg{Vec2{0.0f, 4.0f}, 8.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.16f}};
    ShadowDef xl{Vec2{0.0f, 8.0f}, 16.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.20f}};

    bool operator==(const ShadowTokens&) const noexcept = default;
};

} // namespace ktui

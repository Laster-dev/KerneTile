#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/pal/font_resolver.hpp>
#include <string>

namespace ktui {

// 字阶系统设计令牌 (Typography Tokens)
struct TypographyTokens {
    float xs = 12.0f;       // 角标、小提示、辅助说明
    float sm = 14.0f;       // 次级正文、工具栏小按钮、表格表头
    float base = 16.0f;     // 标准正文、输入框、常规按钮
    float lg = 18.0f;       // 卡片小标题、强调列表项
    float xl = 20.0f;       // 浮窗主标题、小节标题
    float xxl = 24.0f;      // 模态对话框主标题
    float xxxl = 32.0f;     // 页面一级主标题
    float xxxxl = 40.0f;    // Display 展板超大标题

    float line_height_factor = 1.4f; // 默认行高倍数
    std::string font_family = "Segoe UI, -apple-system, BlinkMacSystemFont, Roboto, sans-serif";

    bool operator==(const TypographyTokens&) const noexcept = default;
};

// 4px 栅格间距体系 (Spacing Tokens)
struct SpacingTokens {
    float none = 0.0f;
    float xs = 4.0f;
    float sm = 8.0f;
    float md = 16.0f;
    float lg = 24.0f;
    float xl = 32.0f;
    float xxl = 48.0f;
    float xxxl = 64.0f;

    bool operator==(const SpacingTokens&) const noexcept = default;
};

// 控件圆角体系 (Corner Radius Tokens)
struct CornerRadiusTokens {
    float none = 0.0f;
    float sm = 4.0f;        // 徽标、小列表项
    float md = 8.0f;        // 标准按钮、文本框、卡片
    float lg = 12.0f;       // 浮动面板、对话框边缘
    float xl = 16.0f;       // 悬浮抽屉、通知卡片
    float full = 9999.0f;   // 胶囊药丸状、圆形头像

    bool operator==(const CornerRadiusTokens&) const noexcept = default;
};

// 单层软阴影描述符
struct ShadowDef {
    Vec2 offset{0.0f, 2.0f};
    float blur_radius{4.0f};
    float spread{0.0f};
    Color color{0.0f, 0.0f, 0.0f, 0.15f};

    bool operator==(const ShadowDef&) const noexcept = default;
};

// Elevation 软阴影深度体系 (Shadow Tokens)
struct ShadowTokens {
    ShadowDef none{Vec2{0.0f, 0.0f}, 0.0f, 0.0f, Color::transparent()};
    ShadowDef sm{Vec2{0.0f, 1.0f}, 2.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.08f}};
    ShadowDef md{Vec2{0.0f, 4.0f}, 8.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.12f}};
    ShadowDef lg{Vec2{0.0f, 8.0f}, 16.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.16f}};
    ShadowDef xl{Vec2{0.0f, 16.0f}, 24.0f, 0.0f, Color{0.0f, 0.0f, 0.0f, 0.22f}};

    bool operator==(const ShadowTokens&) const noexcept = default;
};

} // namespace ktui

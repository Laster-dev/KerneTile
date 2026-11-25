#pragma once

#include <ktui/pal/platform_types.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ktui {

// Windows Terminal 光标样式 (完全对标 Microsoft Windows Terminal CursorStyle)
enum class TerminalCursorStyle : uint8_t {
    Bar = 0,              // | 竖线光标 (Windows Terminal 默认)
    Vintage,              // _ 下划线光标
    FilledBox,            // █ 实心方块光标
    EmptyBox,             // □ 空心方块光标
    DoubleUnderscore      // ‗ 双下划线光标
};

// 终端颜色标记 (Default / 256色索引 / 24-bit TrueColor)
struct TerminalColor {
    enum class Kind : uint8_t {
        Default = 0,
        Indexed = 1,
        Rgb = 2
    };

    Kind kind{Kind::Default};
    uint8_t r{0}; // Indexed 时存放 0..255 索引
    uint8_t g{0};
    uint8_t b{0};

    static constexpr TerminalColor default_color() noexcept { return TerminalColor{}; }
    static constexpr TerminalColor indexed(uint8_t idx) noexcept { return TerminalColor{Kind::Indexed, idx, 0, 0}; }
    static constexpr TerminalColor rgb(uint8_t cr, uint8_t cg, uint8_t cb) noexcept { return TerminalColor{Kind::Rgb, cr, cg, cb}; }

    bool operator==(const TerminalColor&) const noexcept = default;
};

// 单元格 SGR 样式位掩码
namespace TerminalAttr {
    constexpr uint16_t None            = 0;
    constexpr uint16_t Bold            = 1u << 0;
    constexpr uint16_t Dim             = 1u << 1;
    constexpr uint16_t Italic          = 1u << 2;
    constexpr uint16_t Underline       = 1u << 3;
    constexpr uint16_t DoubleUnderline = 1u << 4;
    constexpr uint16_t Blink           = 1u << 5;
    constexpr uint16_t Inverse         = 1u << 6;
    constexpr uint16_t Invisible       = 1u << 7;
    constexpr uint16_t Strikethrough   = 1u << 8;
    constexpr uint16_t Overline        = 1u << 9;
    constexpr uint16_t Hyperlink       = 1u << 10;
} // namespace TerminalAttr

// 单个终端字符格 (紧凑布局，支持宽字符/CJK/Emoji)
struct TerminalCell {
    uint32_t codepoint{' '}; // 0 表示前一个宽字符的续占格
    TerminalColor fg{TerminalColor::default_color()};
    TerminalColor bg{TerminalColor::default_color()};
    uint16_t attrs{TerminalAttr::None};
    uint8_t width{1};        // 1 = 窄字符, 2 = 宽字符(CJK/Emoji), 0 = 宽字符右半格占位
    uint32_t hyperlink_id{0};

    void reset(const TerminalColor& default_fg = TerminalColor::default_color(),
               const TerminalColor& default_bg = TerminalColor::default_color()) noexcept {
        codepoint = ' ';
        fg = default_fg;
        bg = default_bg;
        attrs = TerminalAttr::None;
        width = 1;
        hyperlink_id = 0;
    }
};

struct TerminalRow {
    std::vector<TerminalCell> cells;
    bool wrapped{false};
};

// OSC 9;4 Windows Terminal 进度指示状态
enum class TerminalProgressState : uint8_t {
    Hidden = 0,
    Normal = 1,
    Error = 2,
    Indeterminate = 3,
    Paused = 4
};

struct TerminalProgress {
    TerminalProgressState state{TerminalProgressState::Hidden};
    uint8_t value{0}; // 0..100
};

// Windows Terminal 配色方案 (内置完整 16 色 ANSI + 前景/背景/光标/选区色)
struct TerminalColorScheme {
    std::string name{"Campbell"};
    Color background{0x0C / 255.0f, 0x0C / 255.0f, 0x0C / 255.0f, 1.0f};
    Color foreground{0xCC / 255.0f, 0xCC / 255.0f, 0xCC / 255.0f, 1.0f};
    Color cursor_color{1.0f, 1.0f, 1.0f, 1.0f};
    Color selection_bg{1.0f, 1.0f, 1.0f, 0.24f};
    std::array<Color, 16> ansi{};

    static constexpr Color hex(uint32_t rgb, float a = 1.0f) noexcept {
        return Color{
            static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f,
            static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f,
            static_cast<float>(rgb & 0xFFu) / 255.0f,
            a
        };
    }

    // Microsoft Windows Terminal 默认 Campbell 配色
    static TerminalColorScheme campbell() {
        TerminalColorScheme s;
        s.name = "Campbell";
        s.background = hex(0x0C0C0C);
        s.foreground = hex(0xCCCCCC);
        s.cursor_color = hex(0xFFFFFF);
        s.selection_bg = hex(0xFFFFFF, 0.25f);
        s.ansi = {
            hex(0x0C0C0C), hex(0xC50F1F), hex(0x13A10E), hex(0xC19C00),
            hex(0x0037DA), hex(0x881798), hex(0x3A96DD), hex(0xCCCCCC),
            hex(0x767676), hex(0xE74856), hex(0x16C60C), hex(0xF9F1A5),
            hex(0x3B78FF), hex(0xB4009E), hex(0x61D6D6), hex(0xF2F2F2)
        };
        return s;
    }

    // Windows Terminal 内置 One Half Dark
    static TerminalColorScheme one_half_dark() {
        TerminalColorScheme s;
        s.name = "One Half Dark";
        s.background = hex(0x282C34);
        s.foreground = hex(0xDCDFE4);
        s.cursor_color = hex(0xFFFFFF);
        s.selection_bg = hex(0x61AFEF, 0.28f);
        s.ansi = {
            hex(0x282C34), hex(0xE06C75), hex(0x98C379), hex(0xE5C07B),
            hex(0x61AFEF), hex(0xC678DD), hex(0x56B6C2), hex(0xDCDFE4),
            hex(0x5A6374), hex(0xE06C75), hex(0x98C379), hex(0xE5C07B),
            hex(0x61AFEF), hex(0xC678DD), hex(0x56B6C2), hex(0xDCDFE4)
        };
        return s;
    }

    // Windows Terminal 内置 One Half Light
    static TerminalColorScheme one_half_light() {
        TerminalColorScheme s;
        s.name = "One Half Light";
        s.background = hex(0xFAFAFA);
        s.foreground = hex(0x383A42);
        s.cursor_color = hex(0x4F525D);
        s.selection_bg = hex(0x0184BC, 0.22f);
        s.ansi = {
            hex(0x383A42), hex(0xE45649), hex(0x50A14F), hex(0xC18301),
            hex(0x0184BC), hex(0xA626A4), hex(0x0997B3), hex(0xFAFAFA),
            hex(0x4F525D), hex(0xDF6C75), hex(0x98C379), hex(0xE4C07A),
            hex(0x61AFEF), hex(0xC577DD), hex(0x56B5C1), hex(0xFFFFFF)
        };
        return s;
    }

    // Windows Terminal 内置 Solarized Dark
    static TerminalColorScheme solarized_dark() {
        TerminalColorScheme s;
        s.name = "Solarized Dark";
        s.background = hex(0x002B36);
        s.foreground = hex(0x839496);
        s.cursor_color = hex(0x93A1A1);
        s.selection_bg = hex(0x268BD2, 0.28f);
        s.ansi = {
            hex(0x073642), hex(0xDC322F), hex(0x859900), hex(0xB58900),
            hex(0x268BD2), hex(0xD33682), hex(0x2AA198), hex(0xEEE8D5),
            hex(0x002B36), hex(0xCB4B16), hex(0x586E75), hex(0x657B83),
            hex(0x839496), hex(0x6C71C4), hex(0x93A1A1), hex(0xFDF6E3)
        };
        return s;
    }

    // Windows Terminal 内置 Tango Dark
    static TerminalColorScheme tango_dark() {
        TerminalColorScheme s;
        s.name = "Tango Dark";
        s.background = hex(0x1E1E1E);
        s.foreground = hex(0xD3D7CF);
        s.cursor_color = hex(0xFFFFFF);
        s.selection_bg = hex(0x729FCF, 0.28f);
        s.ansi = {
            hex(0x000000), hex(0xCC0000), hex(0x4E9A06), hex(0xC4A000),
            hex(0x3465A4), hex(0x75507B), hex(0x06989A), hex(0xD3D7CF),
            hex(0x555753), hex(0xEF2929), hex(0x8AE234), hex(0xFCE94F),
            hex(0x729FCF), hex(0xAD7FA8), hex(0x34E2E2), hex(0xEEEEEC)
        };
        return s;
    }

    // Windows Terminal 内置 Vintage
    static TerminalColorScheme vintage() {
        TerminalColorScheme s;
        s.name = "Vintage";
        s.background = hex(0x000000);
        s.foreground = hex(0xC0C0C0);
        s.cursor_color = hex(0xFFFFFF);
        s.selection_bg = hex(0xFFFFFF, 0.25f);
        s.ansi = {
            hex(0x000000), hex(0x800000), hex(0x008000), hex(0x808000),
            hex(0x000080), hex(0x800080), hex(0x008080), hex(0xC0C0C0),
            hex(0x808080), hex(0xFF0000), hex(0x00FF00), hex(0xFFFF00),
            hex(0x0000FF), hex(0xFF00FF), hex(0x00FFFF), hex(0xFFFFFF)
        };
        return s;
    }

    // 解析 TerminalColor 到最终 RGBA Color（支持 16 色 / 216 色色立方 / 24 级灰阶 / 24-bit TrueColor）
    [[nodiscard]] Color resolve(const TerminalColor& c, bool is_fg, bool bold_bright = false) const noexcept {
        if (c.kind == TerminalColor::Kind::Default) {
            return is_fg ? foreground : background;
        }
        if (c.kind == TerminalColor::Kind::Rgb) {
            return Color{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f};
        }
        uint8_t idx = c.r;
        if (bold_bright && is_fg && idx < 8) {
            idx = static_cast<uint8_t>(idx + 8);
        }
        if (idx < 16) {
            return ansi[idx];
        }
        if (idx < 232) {
            uint8_t n = static_cast<uint8_t>(idx - 16);
            uint8_t r6 = n / 36;
            uint8_t g6 = (n / 6) % 6;
            uint8_t b6 = n % 6;
            auto lvl = [](uint8_t v) noexcept -> float {
                return v == 0 ? 0.0f : (55.0f + 40.0f * static_cast<float>(v)) / 255.0f;
            };
            return Color{lvl(r6), lvl(g6), lvl(b6), 1.0f};
        }
        float gray = (8.0f + 10.0f * static_cast<float>(idx - 232)) / 255.0f;
        return Color{gray, gray, gray, 1.0f};
    }
};

// 终端文本选区锚点 (行号为包含历史回滚的绝对行号)
struct TerminalCoord {
    int col{0};
    int row{0};

    bool operator==(const TerminalCoord&) const noexcept = default;
    bool operator<(const TerminalCoord& o) const noexcept {
        return (row != o.row) ? (row < o.row) : (col < o.col);
    }
    bool operator<=(const TerminalCoord& o) const noexcept {
        return !(o < *this);
    }
};

// 搜索命中项 (供 Ctrl+Shift+F 内联搜索栏使用)
struct TerminalSearchMatch {
    int row{0};       // 绝对行号
    int col_start{0}; // 起始列 (包含)
    int col_end{0};   // 结束列 (不包含)
};

} // namespace ktui

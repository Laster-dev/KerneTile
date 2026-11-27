#pragma once

#include "../menu_flyout/menu_flyout_logic.hpp"
#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <ktui/pal/platform_types.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ktui {


// WinUI 3 RichTextBlock 内联富文本片段 (Run / Hyperlink / InlineCode)
struct RichTextRun {
    std::string text;
    float font_size{0.0f}; // 0 表示继承块默认字号
    FontWeight font_weight{FontWeight::Regular};
    bool bold{false};
    bool italic{false};
    bool underline{false};
    bool strikethrough{false};
    bool is_code{false};
    Color color{Color{-1.0f, -1.0f, -1.0f, -1.0f}};     // r < 0 表示自动语义色
    Color highlight{Color{-1.0f, -1.0f, -1.0f, -1.0f}}; // r >= 0 时绘制荧光高亮底色
    std::string link_url;
    Fn<void()> on_click{nullptr};

    [[nodiscard]] bool is_link() const noexcept {
        return !link_url.empty() || static_cast<bool>(on_click);
    }
};

// WinUI 3 RichTextBlock 段落定义
struct RichTextParagraph {
    std::vector<RichTextRun> runs;
    float spacing_after{10.0f};
};

} // namespace ktui

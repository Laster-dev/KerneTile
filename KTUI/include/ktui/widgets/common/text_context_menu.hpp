#pragma once

#include <ktui/app/window.hpp>
#include <ktui/pal/clipboard.hpp>
#include <ktui/widgets/menu_flyout.hpp>

#include <functional>
#include <string>
#include <vector>

namespace ktui {

// WinUI 3 文本输入控件标准右键上下文菜单动作集合
struct TextContextMenuActions {
    bool can_undo{false};
    bool can_redo{false};
    bool can_cut{false};
    bool can_copy{false};
    bool can_paste{false};
    bool can_delete{false};
    bool can_select_all{false};

    Fn<void()> on_undo{nullptr};
    Fn<void()> on_redo{nullptr};
    Fn<void()> on_cut{nullptr};
    Fn<void()> on_copy{nullptr};
    Fn<void()> on_paste{nullptr};
    Fn<void()> on_delete{nullptr};
    Fn<void()> on_select_all{nullptr};
};

// 弹出对标 WinUI 3 TextCommandBarFlyout / 文本右键菜单的 MenuFlyout
inline uint64_t show_winui_text_context_menu(Vec2 client_pos, const TextContextMenuActions& actions) {
    Window* win = Window::active_window();
    if (!win) return 0;

    // Fluent UI System Icons (24x24 filled path)
    static constexpr const char* kSvgUndo =
        "M7.18 4.22a.75.75 0 0 1 1.06 0 .75.75 0 0 1 0 1.06L5.56 7.96h7.69a6.25 6.25 0 0 1 0 12.5H9.5a.75.75 0 0 1 0-1.5h3.75a4.75 4.75 0 1 0 0-9.5H5.56l2.68 2.68a.75.75 0 1 1-1.06 1.06L3.22 9.25a.75.75 0 0 1 0-1.06l3.96-3.97Z";
    static constexpr const char* kSvgRedo =
        "M16.82 4.22a.75.75 0 0 0-1.06 0 .75.75 0 0 0 0 1.06l2.68 2.68h-7.69a6.25 6.25 0 0 0 0 12.5h3.75a.75.75 0 0 0 0-1.5h-3.75a4.75 4.75 0 1 1 0-9.5h7.69l-2.68 2.68a.75.75 0 1 0 1.06 1.06l3.96-3.95a.75.75 0 0 0 0-1.06l-3.96-3.97Z";
    static constexpr const char* kSvgCut =
        "M6.5 3a3.5 3.5 0 0 0-1.99 6.38l5.15 3.12-5.15 3.12A3.5 3.5 0 1 0 8.9 19.7l3.1-5.16 5.95 3.6a.75.75 0 0 0 .78-1.28L13.4 13.6l5.33-3.23a.75.75 0 1 0-.78-1.28l-5.95 3.6-3.1-5.16A3.5 3.5 0 0 0 6.5 3Zm0 1.5a2 2 0 1 1 0 4 2 2 0 0 1 0-4Zm0 11a2 2 0 1 1 0 4 2 2 0 0 1 0-4Z";
    static constexpr const char* kSvgCopy =
        "M5.5 4.75A2.75 2.75 0 0 1 8.25 2h8.5A2.75 2.75 0 0 1 19.5 4.75v11.5A2.75 2.75 0 0 1 16.75 19h-8.5a2.75 2.75 0 0 1-2.75-2.75V4.75Zm2.75-1.25c-.69 0-1.25.56-1.25 1.25v11.5c0 .69.56 1.25 1.25 1.25h8.5c.69 0 1.25-.56 1.25-1.25V4.75c0-.69-.56-1.25-1.25-1.25h-8.5ZM4 6.5v10.75A3.75 3.75 0 0 0 7.75 21h7.5a.75.75 0 0 1 0 1.5h-7.5A5.25 5.25 0 0 1 2.5 17.25V6.5a.75.75 0 0 1 1.5 0Z";
    static constexpr const char* kSvgPaste =
        "M9.75 2A2.25 2.25 0 0 0 7.5 4.25H6.25A2.25 2.25 0 0 0 4 6.5v13.25A2.25 2.25 0 0 0 6.25 22h11.5A2.25 2.25 0 0 0 20 19.75V6.5a2.25 2.25 0 0 0-2.25-2.25H16.5A2.25 2.25 0 0 0 14.25 2h-4.5Zm0 1.5h4.5c.41 0 .75.34.75.75v.75c0 .41-.34.75-.75.75h-4.5a.75.75 0 0 1-.75-.75v-.75c0-.41.34-.75.75-.75ZM6.25 5.75H7.5v.25a2.25 2.25 0 0 0 2.25 2.25h4.5A2.25 2.25 0 0 0 16.5 6v-.25h1.25c.41 0 .75.34.75.75v13.25a.75.75 0 0 1-.75.75H6.25a.75.75 0 0 1-.75-.75V6.5c0-.41.34-.75.75-.75Z";
    static constexpr const char* kSvgDelete =
        "M10 3a2 2 0 0 0-2 2v1H4.75a.75.75 0 0 0 0 1.5h.58l.84 11.4A2.25 2.25 0 0 0 8.42 21h7.16a2.25 2.25 0 0 0 2.25-2.1l.84-11.4h.58a.75.75 0 0 0 0-1.5H16V5a2 2 0 0 0-2-2h-4Zm4.5 3h-5V5a.5.5 0 0 1 .5-.5h4a.5.5 0 0 1 .5.5v1Zm-7.66 1.5h10.32l-.82 11.29a.75.75 0 0 1-.75.71H8.41a.75.75 0 0 1-.75-.71L6.84 7.5Z";
    static constexpr const char* kSvgSelectAll =
        "M4.5 3A1.5 1.5 0 0 0 3 4.5V6a.75.75 0 0 0 1.5 0V4.5H6A.75.75 0 0 0 6 3H4.5ZM18 3a.75.75 0 0 0 0 1.5h1.5V6A.75.75 0 0 0 21 6V4.5A1.5 1.5 0 0 0 19.5 3H18ZM4.5 18a.75.75 0 0 0-1.5 0v1.5A1.5 1.5 0 0 0 4.5 21H6a.75.75 0 0 0 0-1.5H4.5V18Zm16.5 0a.75.75 0 0 0-1.5 0v1.5H18a.75.75 0 0 0 0 1.5h1.5a1.5 1.5 0 0 0 1.5-1.5V18ZM8.75 7.25h6.5a1.5 1.5 0 0 1 1.5 1.5v6.5a1.5 1.5 0 0 1-1.5 1.5h-6.5a1.5 1.5 0 0 1-1.5-1.5v-6.5a1.5 1.5 0 0 1 1.5-1.5Z";

    std::vector<MenuFlyoutItem> items;
    if (actions.on_undo) {
        items.push_back(MenuFlyoutItem{
            .label = "撤销",
            .shortcut = "Ctrl+Z",
            .svg_path = kSvgUndo,
            .disabled = !actions.can_undo,
            .on_click = actions.on_undo
        });
    }
    if (actions.on_redo) {
        items.push_back(MenuFlyoutItem{
            .label = "重做",
            .shortcut = "Ctrl+Y",
            .svg_path = kSvgRedo,
            .disabled = !actions.can_redo,
            .on_click = actions.on_redo
        });
    }
    if (!items.empty()) {
        items.push_back(MenuFlyoutItem::separator());
    }

    items.push_back(MenuFlyoutItem{
        .label = "剪切",
        .shortcut = "Ctrl+X",
        .svg_path = kSvgCut,
        .disabled = !actions.can_cut || !actions.on_cut,
        .on_click = actions.on_cut
    });
    items.push_back(MenuFlyoutItem{
        .label = "复制",
        .shortcut = "Ctrl+C",
        .svg_path = kSvgCopy,
        .disabled = !actions.can_copy || !actions.on_copy,
        .on_click = actions.on_copy
    });
    items.push_back(MenuFlyoutItem{
        .label = "粘贴",
        .shortcut = "Ctrl+V",
        .svg_path = kSvgPaste,
        .disabled = !actions.can_paste || !actions.on_paste,
        .on_click = actions.on_paste
    });
    if (actions.on_delete) {
        items.push_back(MenuFlyoutItem{
            .label = "删除",
            .shortcut = "Del",
            .svg_path = kSvgDelete,
            .disabled = !actions.can_delete,
            .on_click = actions.on_delete
        });
    }

    items.push_back(MenuFlyoutItem::separator());
    items.push_back(MenuFlyoutItem{
        .label = "全选",
        .shortcut = "Ctrl+A",
        .svg_path = kSvgSelectAll,
        .disabled = !actions.can_select_all || !actions.on_select_all,
        .on_click = actions.on_select_all
    });

    Vec2 win_pos = client_pos;
    win_pos.y += win->titlebar_height();
    return MenuFlyout::show_at(std::move(items), win_pos);
}

} // namespace ktui

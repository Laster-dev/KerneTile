#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_auto_suggest_box_page(GalleryApp& app) {
    static const std::vector<std::string> kAllControls = {
        "AutoSuggestBox", "BreadcrumbBar", "Button", "CalendarDatePicker",
        "CheckBox", "ComboBox", "CommandBar", "ContentDialog",
        "DropDownButton", "Expander", "InfoBar", "Label",
        "MenuFlyout", "NavigationView", "NumberBox", "PasswordBox",
        "ProgressBar", "ProgressRing", "RadioButton", "RadioCard",
        "Slider", "TabView", "TextBox", "Toast", "ToggleSwitch", "TreeView"
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("AutoSuggestBox", 22.0f),
            app.make_muted("WinUI 3 自动补全搜索框，内置清除与搜索按钮、真高斯阴影悬浮候选列表，支持 ↑/↓/Enter/Esc 键盘导航。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "快速检索 KTUI 控件", .font_size = 14.0f },
                    AutoSuggestBox {
                        .placeholder = "输入控件名 (如 Tab, Box, Bar, Tree)...",
                        .header = "控件搜索",
                        .items = kAllControls,
                        .width = 340.0f,
                        .on_text_changed = [&app](const std::string& q) {
                            app.suggest_status.set(q.empty() ? "已清空搜索词，点击输入框可浏览全部候选" : ("正在过滤关键字: " + q));
                        },
                        .on_suggestion_chosen = [&app](const std::string& chosen) {
                            app.suggest_status.set("已选择建议项: " + chosen);
                        },
                        .on_query_submitted = [&app](const std::string& q) {
                            app.suggest_status.set("已提交搜索查询: " + (q.empty() ? "(空)" : q));
                        }
                    }.bind_two_way(app.suggest_query),
                    app.make_accent(app.suggest_status, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

#include <ktui/widgets/navigation_view.hpp>
#include <ktui/widgets/text.hpp>
#include <ktui/theme/theme.hpp>
#include <cassert>
#include <iostream>
#include <string>

using namespace ktui;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " KTUI WinUI 3 NavigationView Unit Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. 样式解析验证
    auto dark = NavigationViewStyleResolver::resolve(Theme::dark());
    auto light = NavigationViewStyleResolver::resolve(Theme::light());
    assert(dark.item_height == 36.0f);
    assert(dark.top_bar_height == 44.0f);
    assert(dark.accent_pill.b > 0.9f);  // #60CDFF
    assert(light.accent_pill.b > 0.6f); // #005FB8

    // 2. 响应式 Auto 断点测试 (Expanded / Compact / Minimal)
    RenderNavigationView nav;
    nav.set_menu_items({
        NavigationViewItem{ .id = "home", .label = "首页", .icon = NavigationIcon::Home },
        NavigationViewItem::header("控件库"),
        NavigationViewItem{
            .id = "inputs", .label = "基础输入", .icon = NavigationIcon::Inputs, .badge = "4",
            .children = {
                NavigationViewItem{ .id = "buttons", .label = "按钮与开关", .icon = NavigationIcon::Controls },
                NavigationViewItem{ .id = "textbox", .label = "文本输入", .icon = NavigationIcon::Code }
            }
        },
        NavigationViewItem::separator(),
        NavigationViewItem{ .id = "progress", .label = "进度与滑块", .icon = NavigationIcon::Progress }
    });

    assert(nav.selected_id() == "home");

    nav.layout(BoxConstraints::tight(Vec2{1100.0f, 700.0f}));
    assert(nav.effective_display_mode() == NavigationPaneDisplayMode::Left);
    assert(nav.is_pane_open());

    nav.layout(BoxConstraints::tight(Vec2{760.0f, 700.0f}));
    assert(nav.effective_display_mode() == NavigationPaneDisplayMode::LeftCompact);
    assert(!nav.is_pane_open());

    nav.layout(BoxConstraints::tight(Vec2{500.0f, 700.0f}));
    assert(nav.effective_display_mode() == NavigationPaneDisplayMode::LeftMinimal);
    assert(!nav.is_pane_open());

    // 3. Top 模式切换 & 返回历史栈测试
    nav.set_pane_display_mode(NavigationPaneDisplayMode::Top);
    nav.layout(BoxConstraints::tight(Vec2{900.0f, 600.0f}));
    assert(nav.effective_display_mode() == NavigationPaneDisplayMode::Top);

    std::string last_selected;
    nav.set_callbacks([&](const std::string& id) { last_selected = id; }, nullptr, nullptr);

    nav.set_selected_id("progress", true, true);
    assert(nav.selected_id() == "progress");
    assert(last_selected == "progress");
    assert(nav.can_go_back());

    assert(nav.go_back());
    assert(nav.selected_id() == "home");
    assert(last_selected == "home");

    // 4. C++20 声明式 DSL、双向绑定与 page_builder 测试
    Property<std::string> current_page{"home"};
    Property<NavigationPaneDisplayMode> mode{NavigationPaneDisplayMode::Left};
    int built_pages = 0;

    WidgetPtr ptr = NavigationView{
        .menu_items = {
            NavigationViewItem{ .id = "home", .label = "首页", .icon = NavigationIcon::Home },
            NavigationViewItem{ .id = "about", .label = "关于", .icon = NavigationIcon::Info }
        },
        .pane_title = "KTUI Gallery",
        .page_builder = [&](const std::string& id) -> WidgetPtr {
            built_pages++;
            return Text{ .text = "Page: " + id };
        }
    }.bind_two_way(current_page)
     .bind(&RenderNavigationView::set_pane_display_mode, mode);
    auto node = std::static_pointer_cast<RenderNavigationView>(ptr->create_render_node());
    assert(node != nullptr);
    assert(node->selected_id() == "home");
    assert(built_pages == 1);

    current_page.set("about");
    assert(node->selected_id() == "about");
    assert(built_pages == 2);

    mode.set(NavigationPaneDisplayMode::Top);
    assert(node->pane_display_mode() == NavigationPaneDisplayMode::Top);

    std::cout << "All NavigationView tests passed successfully!" << std::endl;
    return 0;
}

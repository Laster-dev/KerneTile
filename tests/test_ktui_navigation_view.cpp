#include <ktui/widgets/navigation_view.hpp>
#include <ktui/widgets/pivot.hpp>
#include <ktui/widgets/segmented_control.hpp>
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

    // 5. Fluent 2 Pivot (无边框下划线页签：紧凑模式 + 内容联动模式) & SegmentedControl
    {
        Property<int> pivot_tab{0};
        int cb_idx = -1;
        WidgetPtr p_strip = Pivot{
            .items = {
                PivotItem{ .header = "历史记录" },
                PivotItem{ .header = "内存" },
                PivotItem{ .header = "禁用项", .disabled = true }
            },
            .on_selection_changed = [&](int i) { cb_idx = i; }
        }.bind_two_way(pivot_tab);
        auto pnode = std::static_pointer_cast<RenderPivot>(p_strip->create_render_node());
        assert(pnode != nullptr);
        pnode->layout(BoxConstraints::loose(Vec2{600.0f, 300.0f}));
        assert(pnode->size().y >= 32.0f && pnode->size().y <= 36.0f);
        assert(pnode->size().x < 260.0f); // 紧凑贴合内容宽度，不撑满父宽

        pnode->select(1);
        assert(pivot_tab.get() == 1);
        assert(cb_idx == 1);

        pnode->select(2); // 禁用项不可选中
        assert(pnode->selected_index() == 1);
        assert(pivot_tab.get() == 1);

        // 带 content_builder 的完整 Pivot 容器
        int built_tab = -1;
        WidgetPtr p_full = Pivot{
            .items = {
                PivotItem{ .header = "差值计算" },
                PivotItem{ .header = "加减天数" }
            },
            .content_builder = [&](int idx) -> WidgetPtr {
                built_tab = idx;
                return Text{ .text = (idx == 0) ? "Tab0" : "Tab1" };
            }
        };
        auto fnode = std::static_pointer_cast<RenderPivot>(p_full->create_render_node());
        fnode->layout(BoxConstraints::loose(Vec2{500.0f, 400.0f}));
        assert(built_tab == 0);
        assert(fnode->size().y > 40.0f);
        fnode->select(1);
        assert(built_tab == 1);

        // SegmentedControl 双向绑定验证
        Property<int> seg_idx{0};
        WidgetPtr sptr = SegmentedControl{
            .items = { SegmentedItem{ .label = "浅色" }, SegmentedItem{ .label = "深色" } }
        }.bind_two_way(seg_idx);
        auto snode = std::static_pointer_cast<RenderSegmentedControl>(sptr->create_render_node());
        snode->layout(BoxConstraints::loose(Vec2{400.0f, 100.0f}));
        snode->select(1);
        assert(seg_idx.get() == 1);
    }

    std::cout << "All NavigationView, Pivot & SegmentedControl tests passed successfully!" << std::endl;
    return 0;
}

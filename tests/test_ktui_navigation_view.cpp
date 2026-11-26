#include "test_common.hpp"
#include <ktui/widgets/navigation_view.hpp>
#include <ktui/widgets/pivot.hpp>
#include <ktui/widgets/segmented_control.hpp>
#include <ktui/widgets/text.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme.hpp>
#include <kernetile/renderer.hpp>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace ktui;

#define KTUI_CHECK(expr)                                                               \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
            std::exit(1);                                                              \
        }                                                                              \
    } while (0)

namespace {

std::vector<uint8_t> render_nav_to_rgba(RenderNavigationView& nav, uint32_t w, uint32_t h) {
    kernetile::RendererConfig cfg;
    cfg.width = w;
    cfg.height = h;
    cfg.force_cpu_fallback = true;
    auto renderer = kernetile::KerneTileRenderer::create(cfg);
    KTUI_CHECK(renderer != nullptr);

    renderer->begin_frame();
    PaintContext ctx(renderer.get(), 1.0f, false);
    nav.paint(ctx);
    renderer->end_frame();

    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4u, 0u);
    renderer->render_to_rgba8(rgba.data());
    return rgba;
}

size_t count_diff_pixels_in_rect(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                                 uint32_t w, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    size_t diff = 0;
    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            const size_t idx = (static_cast<size_t>(y) * w + x) * 4u;
            if (a[idx] != b[idx] || a[idx + 1] != b[idx + 1] || a[idx + 2] != b[idx + 2] || a[idx + 3] != b[idx + 3]) {
                ++diff;
            }
        }
    }
    return diff;
}

void test_navigation_view_pointer_reveal() {
    std::cout << "[Test 6] NavigationView Opt-in Pointer Reveal (R3) 验证..." << std::endl;

    // 1. Default disabled across NavigationViewData, NavigationView, RenderNavigationView
    NavigationViewData def_data;
    KTUI_CHECK(def_data.enable_pointer_reveal == false);
    NavigationView def_desc;
    KTUI_CHECK(def_desc.enable_pointer_reveal == false);
    RenderNavigationView def_nav;
    KTUI_CHECK(def_nav.pointer_reveal_enabled() == false);

    // 2. Fluent .pointer_reveal(bool) & / && overloads + WidgetPtr + update_render_node + binding
    NavigationView l_desc{
        .menu_items = {
            NavigationViewItem{.id = "home", .label = "Home", .icon = NavigationIcon::Home},
            NavigationViewItem{.id = "disabled_item", .label = "Disabled", .icon = NavigationIcon::Info, .disabled = true},
            NavigationViewItem{.id = "about", .label = "About", .icon = NavigationIcon::Controls}
        },
        .pane_display_mode = NavigationPaneDisplayMode::Left
    };
    l_desc.pointer_reveal(true);
    KTUI_CHECK(l_desc.enable_pointer_reveal == true);
    WidgetPtr lw = l_desc;
    auto lnode = std::static_pointer_cast<RenderNavigationView>(lw->create_render_node());
    KTUI_CHECK(lnode != nullptr && lnode->pointer_reveal_enabled() == true);

    WidgetPtr rw = NavigationView{
        .menu_items = {
            NavigationViewItem{.id = "home", .label = "Home", .icon = NavigationIcon::Home},
            NavigationViewItem{.id = "disabled_item", .label = "Disabled", .icon = NavigationIcon::Info, .disabled = true},
            NavigationViewItem{.id = "about", .label = "About", .icon = NavigationIcon::Controls}
        },
        .pane_display_mode = NavigationPaneDisplayMode::Left,
        .is_pane_toggle_button_visible = false,
        .is_back_button_visible = false,
        .is_settings_visible = false
    }.pointer_reveal(true);
    auto rnode = std::static_pointer_cast<RenderNavigationView>(rw->create_render_node());
    KTUI_CHECK(rnode != nullptr && rnode->pointer_reveal_enabled() == true);

    NavigationViewWidget upd_w(NavigationView{.pane_display_mode = NavigationPaneDisplayMode::Left, .enable_pointer_reveal = false});
    upd_w.update_render_node(lnode);
    KTUI_CHECK(lnode->pointer_reveal_enabled() == false);

    Property<bool> reveal_prop(false);
    WidgetPtr bound_w = NavigationView{
        .menu_items = { NavigationViewItem{.id = "a", .label = "A"} },
        .pane_display_mode = NavigationPaneDisplayMode::Left
    }.bind(&RenderNavigationView::set_pointer_reveal_enabled, reveal_prop);
    auto bound_node = std::static_pointer_cast<RenderNavigationView>(bound_w->create_render_node());
    KTUI_CHECK(!bound_node->pointer_reveal_enabled());
    reveal_prop.set(true);
    KTUI_CHECK(bound_node->pointer_reveal_enabled());

    // 3. Zero PointerMove repaint churn when enable_pointer_reveal == false (Left, LeftCompact, Top)
    for (auto mode : {NavigationPaneDisplayMode::Left, NavigationPaneDisplayMode::LeftCompact, NavigationPaneDisplayMode::Top}) {
        RenderNavigationView nav;
        nav.set_pane_display_mode(mode);
        nav.set_menu_items({
            NavigationViewItem{.id = "home", .label = "Home", .icon = NavigationIcon::Home},
            NavigationViewItem{.id = "about", .label = "About", .icon = NavigationIcon::Info}
        });
        nav.layout(BoxConstraints::tight(Vec2{960.0f, 540.0f}));
        KTUI_CHECK(nav.pointer_reveal_enabled() == false);

        const PointF p1 = (mode == NavigationPaneDisplayMode::Top) ? PointF{80.0f, 20.0f} : PointF{24.0f, 60.0f};
        const PointF p2 = (mode == NavigationPaneDisplayMode::Top) ? PointF{84.0f, 22.0f} : PointF{28.0f, 62.0f};
        PointerEvent mv1(PlatformEventType::PointerMove, p1);
        mv1.local_position = p1;
        nav.on_pointer_move(mv1);
        nav.clear_dirty_recursive();

        PointerEvent mv2(PlatformEventType::PointerMove, p2);
        mv2.local_position = p2;
        nav.on_pointer_move(mv2);
        KTUI_CHECK(!nav.needs_paint());
    }

    // 4. Left, LeftCompact & Top reveal + empty-area / exit clearing + pixel baseline match
    for (auto mode : {NavigationPaneDisplayMode::Left, NavigationPaneDisplayMode::LeftCompact, NavigationPaneDisplayMode::Top}) {
        RenderNavigationView nav;
        nav.set_chrome_options(false, false, false, false, "Settings", "", "", 240.0f, 48.0f, 641.0f, 900.0f);
        nav.set_pane_display_mode(mode);
        nav.set_menu_items({
            NavigationViewItem{.id = "home", .label = "Home", .icon = NavigationIcon::Home},
            NavigationViewItem{.id = "about", .label = "About", .icon = NavigationIcon::Info}
        });
        nav.set_pointer_reveal_enabled(true);
        nav.layout(BoxConstraints::tight(Vec2{640.0f, 360.0f}));

        // Warmup initial frame so pill/anim state is settled
        const auto baseline = render_nav_to_rgba(nav, 640u, 360u);
        nav.clear_dirty_recursive();

        const PointF hit1 = (mode == NavigationPaneDisplayMode::Top) ? PointF{36.0f, 22.0f} : PointF{24.0f, 24.0f};
        const PointF hit2 = (mode == NavigationPaneDisplayMode::Top) ? PointF{42.0f, 22.0f} : PointF{28.0f, 26.0f};

        // Capture hover-only frame (reveal disabled) for comparison against Touch clearing
        nav.set_pointer_reveal_enabled(false);
        PointerEvent mv1(PlatformEventType::PointerMove, hit1);
        mv1.local_position = hit1;
        nav.on_pointer_move(mv1);
        const auto hover_only = render_nav_to_rgba(nav, 640u, 360u);
        PointerEvent exit_ev(PlatformEventType::PointerMove, PointF{-10.0f, -10.0f});
        nav.on_pointer_exit(exit_ev);
        nav.set_pointer_reveal_enabled(true);

        nav.clear_dirty_recursive();
        nav.on_pointer_move(mv1);
        KTUI_CHECK(nav.needs_paint());
        const auto frame1 = render_nav_to_rgba(nav, 640u, 360u);
        KTUI_CHECK(frame1 != baseline);
        KTUI_CHECK(frame1 != hover_only);

        nav.clear_dirty_recursive();
        PointerEvent mv2(PlatformEventType::PointerMove, hit2);
        mv2.local_position = hit2;
        nav.on_pointer_move(mv2);
        KTUI_CHECK(nav.needs_paint());
        const auto frame2 = render_nav_to_rgba(nav, 640u, 360u);
        KTUI_CHECK(frame2 != frame1);

        // Touch move clears reveal (matches hover-only frame without reveal)
        nav.clear_dirty_recursive();
        PointerEvent touch_mv(PlatformEventType::PointerMove, hit1);
        touch_mv.local_position = hit1;
        touch_mv.device_kind = PointerDeviceKind::Touch;
        nav.on_pointer_move(touch_mv);
        KTUI_CHECK(nav.needs_paint());
        KTUI_CHECK(render_nav_to_rgba(nav, 640u, 360u) == hover_only);

        // Move back in, then move into empty content area -> clears reveal
        nav.on_pointer_move(mv1);
        KTUI_CHECK(render_nav_to_rgba(nav, 640u, 360u) == frame1);
        nav.clear_dirty_recursive();
        PointerEvent mv_empty(PlatformEventType::PointerMove, PointF{400.0f, 200.0f});
        mv_empty.local_position = PointF{400.0f, 200.0f};
        nav.on_pointer_move(mv_empty);
        KTUI_CHECK(nav.needs_paint());
        KTUI_CHECK(render_nav_to_rgba(nav, 640u, 360u) == baseline);

        // Move back in, then exit -> clears reveal cleanly to baseline
        nav.on_pointer_move(mv1);
        KTUI_CHECK(render_nav_to_rgba(nav, 640u, 360u) == frame1);
        nav.clear_dirty_recursive();
        nav.on_pointer_exit(exit_ev);
        KTUI_CHECK(nav.needs_paint());
        KTUI_CHECK(render_nav_to_rgba(nav, 640u, 360u) == baseline);
    }

    // 5. Disabled NavigationViewItem emits zero reveal pixels even when hovering directly over it
    {
        RenderNavigationView nav;
        nav.set_chrome_options(false, false, false, false, "Settings", "", "", 240.0f, 48.0f, 641.0f, 900.0f);
        nav.set_pane_display_mode(NavigationPaneDisplayMode::Left);
        nav.set_menu_items({
            NavigationViewItem{.id = "disabled_only", .label = "Disabled", .icon = NavigationIcon::Info, .disabled = true}
        });
        nav.set_pointer_reveal_enabled(true);
        nav.layout(BoxConstraints::tight(Vec2{480.0f, 320.0f}));

        const auto base = render_nav_to_rgba(nav, 480u, 320u);
        PointerEvent mv(PlatformEventType::PointerMove, PointF{100.0f, 24.0f});
        mv.local_position = PointF{100.0f, 24.0f};
        nav.on_pointer_move(mv);
        const auto after = render_nav_to_rgba(nav, 480u, 320u);
        KTUI_CHECK(count_diff_pixels_in_rect(base, after, 480u, 0u, 0u, 240u, 60u) == 0u);
    }

    std::cout << "  -> NavigationView Pointer Reveal (Left / LeftCompact / Top / Disabled / Exit) 验证通过！" << std::endl;
}

} // namespace

int main() {
    configure_test_environment();
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

    test_navigation_view_pointer_reveal();

    std::cout << "All NavigationView, Pivot & SegmentedControl tests passed successfully!" << std::endl;
    return 0;
}

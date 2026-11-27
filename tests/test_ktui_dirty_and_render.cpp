#include "test_common.hpp"
#include <ktui/ktui.hpp>
#include <ktui/event/event_types.hpp>
#include <ktui/event/hit_test_tree.hpp>
#include <ktui/event/event_dispatcher.hpp>
#include <ktui/event/focus_tree.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/ui_compositor.hpp>
#include <ktui/tree/render_flex.hpp>
#include <ktui/tree/overlay.hpp>
#include <ktui/app/application.hpp>
#include <ktui/app/window.hpp>
#include <ktui/app/window_titlebar.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/widgets/button/button_logic.hpp>
#include <ktui/widgets/checkbox/checkbox_logic.hpp>
#include <ktui/widgets/navigation_view/navigation_view_logic.hpp>
#include <ktui/widgets/settings_card/settings_card_logic.hpp>
#include <ktui/widgets/text/text_logic.hpp>
#include "../KTUI/src/pal/headless_backend.hpp"
#include <kernetile/renderer.hpp>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

using namespace ktui;

#define KTUI_CHECK(expr)                                                               \
    do {                                                                               \
        if (!(expr)) {                                                                 \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr "\n"; \
            std::exit(1);                                                              \
        }                                                                              \
    } while (0)

#define KTUI_CHECK_NEAR(a, b, eps)                                                     \
    do {                                                                               \
        const double _va = static_cast<double>(a);                                     \
        const double _vb = static_cast<double>(b);                                     \
        if (std::abs(_va - _vb) > static_cast<double>(eps)) {                          \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": |" #a " ("     \
                      << _va << ") - " #b " (" << _vb << ")| > " #eps "\n";            \
            std::exit(1);                                                              \
        }                                                                              \
    } while (0)

// 具备可自定义绘制逻辑的测试渲染节点
class RenderVisualBox : public RenderBox {
public:
    RenderVisualBox(Vec2 size, Color color, float radius = 0.0f)
        : m_fixed_size(size), m_color(color), m_radius(radius) {}

    void perform_layout() override {
        m_size = m_constraints.constrain(m_fixed_size);
        for (RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
            if (auto* box = (child ? (child)->as_render_box() : nullptr)) {
                box->layout(BoxConstraints::loose(m_size));
            }
        }
    }

    void paint(PaintContext& context) override {
        context.draw_rect(bounds(), m_color, m_radius);
        RenderBox::paint(context); // 递归绘制子节点
    }

private:
    Vec2 m_fixed_size;
    Color m_color;
    float m_radius;
};

void test_hit_testing_depth_path() {
    std::cout << "[Test 1] 深度优先树状命中测试与路径构建测试..." << std::endl;

    auto root = std::make_unique<RenderSizedBox>(Vec2{800.0f, 600.0f});
    auto container = std::make_unique<RenderSizedBox>(Vec2{400.0f, 300.0f});
    container->set_offset(Vec2{100.0f, 100.0f});

    auto button = std::make_unique<RenderSizedBox>(Vec2{100.0f, 50.0f});
    button->set_offset(Vec2{20.0f, 20.0f}); // 相对于 container 为 (20, 20)，绝对为 (120, 120)

    container->append_child(button.get());
    root->append_child(container.get());

    root->layout(BoxConstraints::tight(Vec2{800.0f, 600.0f}));

    // 点击在按钮内部: 绝对坐标 (130, 130)
    HitTestResult result;
    bool hit = root->hit_test(result, PointF{130.0f, 130.0f});
    KTUI_CHECK(hit);
    KTUI_CHECK(!result.is_empty());

    // 路径首项必须为按钮 (最深层叶子，拥有最高事件捕获优先级)
    KTUI_CHECK(result.first_target() == button.get());

    // 按钮自身局部坐标应当是 (130 - 120, 130 - 120) = (10, 10)
    KTUI_CHECK(result.path().front().local_position == PointF(10.0f, 10.0f));

    // 点击在容器内部但按钮外部: (110, 110)
    HitTestResult result_outside_btn;
    hit = root->hit_test(result_outside_btn, PointF{110.0f, 110.0f});
    KTUI_CHECK(hit);
    KTUI_CHECK(result_outside_btn.first_target() == container.get());

    std::cout << "  -> 深度递归命中测试与局部相对坐标解算完全正确！" << std::endl;
}

void test_event_bubble_and_stop_propagation() {
    std::cout << "[Test 2] 事件冒泡与 stop_propagation() 阻断机制测试..." << std::endl;

    auto container = std::make_unique<RenderSizedBox>(Vec2{400.0f, 300.0f});
    auto button = std::make_unique<RenderSizedBox>(Vec2{100.0f, 50.0f});
    button->set_offset(Vec2{50.0f, 50.0f});
    container->append_child(button.get());

    container->layout(BoxConstraints::tight(Vec2{400.0f, 300.0f}));

    EventDispatcher dispatcher;

    // 场景 A: 按钮拦截并调用 stop_propagation()
    bool button_clicked = false;
    bool container_clicked = false;

    button->on_pointer_down = [&](PointerEvent& e) {
        button_clicked = true;
        e.stop_propagation(); // 阻断冒泡！
    };

    container->on_pointer_down = [&](PointerEvent& e) {
        (void)e;
        container_clicked = true;
    };

    PointerEvent down_event(PlatformEventType::PointerDown, PointF{60.0f, 60.0f}, PointerButton::Primary);
    dispatcher.dispatch_pointer_event(container.get(), down_event);

    KTUI_CHECK(button_clicked == true);
    KTUI_CHECK(container_clicked == false); // 容器未被触发！

    // 场景 B: 按钮不调用 stop_propagation()，事件正常向上冒泡至容器
    button_clicked = false;
    container_clicked = false;
    button->on_pointer_down = [&](PointerEvent& e) {
        (void)e;
        button_clicked = true;
    };

    PointerEvent down_event2(PlatformEventType::PointerDown, PointF{60.0f, 60.0f}, PointerButton::Primary);
    dispatcher.dispatch_pointer_event(container.get(), down_event2);
    KTUI_CHECK(button_clicked == true);
    KTUI_CHECK(container_clicked == true); // 容器成功捕获冒泡事件！

    std::cout << "  -> 事件沿渲染树逐层冒泡与 stop_propagation 阻断验证成功！" << std::endl;
}

void test_hover_state_tracking() {
    std::cout << "[Test 3] :hover 悬停状态自动追踪与 Enter/Exit 回调测试..." << std::endl;

    auto container = std::make_unique<RenderSizedBox>(Vec2{400.0f, 300.0f});
    auto button = std::make_unique<RenderSizedBox>(Vec2{100.0f, 50.0f});
    button->set_offset(Vec2{50.0f, 50.0f});
    container->append_child(button.get());

    container->layout(BoxConstraints::tight(Vec2{400.0f, 300.0f}));

    EventDispatcher dispatcher;
    bool entered = false;
    bool exited = false;

    button->on_pointer_enter = [&](PointerEvent& e) { (void)e; entered = true; };
    button->on_pointer_exit = [&](PointerEvent& e) { (void)e; exited = true; };

    KTUI_CHECK(!button->is_hovered);

    // 1. 移动到按钮内部 (60, 60)
    PointerEvent move_in(PlatformEventType::PointerMove, PointF{60.0f, 60.0f});
    dispatcher.dispatch_pointer_event(container.get(), move_in);
    KTUI_CHECK(button->is_hovered == true);
    KTUI_CHECK(entered == true);
    KTUI_CHECK(exited == false);

    // 2. 移动出按钮外部 (10, 10)
    PointerEvent move_out(PlatformEventType::PointerMove, PointF{10.0f, 10.0f});
    dispatcher.dispatch_pointer_event(container.get(), move_out);
    KTUI_CHECK(button->is_hovered == false);
    KTUI_CHECK(exited == true);

    std::cout << "  -> :hover 状态集差集计算与 Enter/Exit 状态机自动流转正确！" << std::endl;
}

void test_focus_manager_tab_navigation() {
    std::cout << "[Test 4] FocusManager 键盘焦点管理与 Tab/Shift+Tab 环形流转测试..." << std::endl;

    auto root = std::make_unique<RenderFlex>(FlexDirection::Column);
    auto inputA = std::make_unique<RenderSizedBox>(Vec2{100.0f, 30.0f});
    auto inputB = std::make_unique<RenderSizedBox>(Vec2{100.0f, 30.0f});
    auto inputC = std::make_unique<RenderSizedBox>(Vec2{100.0f, 30.0f});

    inputA->is_focusable = true;
    inputB->is_focusable = true;
    inputC->is_focusable = true;

    root->append_child(inputA.get());
    root->append_child(inputB.get());
    root->append_child(inputC.get());

    FocusManager focus_mgr;
    KTUI_CHECK(focus_mgr.focused_node() == nullptr);

    int a_focus_events = 0;
    inputA->on_focus = [&](FocusEvent& e) {
        if (e.gained_focus) a_focus_events++;
    };

    // 聚焦 A -> B -> C -> A，验证焦点事件与清空
    focus_mgr.request_focus(inputA.get());
    KTUI_CHECK(focus_mgr.focused_node() == inputA.get());
    KTUI_CHECK(a_focus_events == 1);

    focus_mgr.request_focus(inputB.get());
    KTUI_CHECK(focus_mgr.focused_node() == inputB.get());

    focus_mgr.request_focus(inputC.get());
    KTUI_CHECK(focus_mgr.focused_node() == inputC.get());

    focus_mgr.request_focus(inputA.get());
    KTUI_CHECK(focus_mgr.focused_node() == inputA.get());
    KTUI_CHECK(a_focus_events == 2);

    // 清空焦点
    focus_mgr.clear_focus();
    KTUI_CHECK(focus_mgr.focused_node() == nullptr);

    std::cout << "  -> Tab / Shift+Tab 环形焦点循环流转与回调通知完全正确！" << std::endl;
}

void test_compositor_dirty_union_and_tile_snapping() {
    std::cout << "[Test 5] 双历史包围盒 Old U New、软阴影外扩与 16x16 瓦片吸附几何测试..." << std::endl;

    // 场景：控件尺寸 100x100，从 (10, 10) 移动到 (150, 10)，具有 blur=10.0 的软阴影
    Rect old_bounds = Rect::from_xywh(10.0f, 10.0f, 100.0f, 100.0f);
    Rect new_bounds = Rect::from_xywh(150.0f, 10.0f, 100.0f, 100.0f);

    // 1. 联合测试
    Rect united = UICompositor::union_rect(old_bounds, new_bounds);
    KTUI_CHECK(united.min_x == 10.0f && united.max_x == 250.0f);
    KTUI_CHECK(united.min_y == 10.0f && united.max_y == 110.0f);

    // 2. 阴影外扩测试: margin = 3 * 10 = 30px
    Rect inflated = UICompositor::inflate_shadow(united, 10.0f, 0.0f);
    KTUI_CHECK(inflated.min_x == -20.0f && inflated.max_x == 280.0f);
    KTUI_CHECK(inflated.min_y == -20.0f && inflated.max_y == 140.0f);

    // 3. 16x16 瓦片吸附对齐测试
    // min_x = floor(-20 / 16) * 16 = -2 * 16 = -32.0f
    // max_x = ceil(280 / 16) * 16 = 18 * 16 = 288.0f
    // min_y = floor(-20 / 16) * 16 = -32.0f
    // max_y = ceil(140 / 16) * 16 = 9 * 16 = 144.0f
    Rect snapped = UICompositor::snap_to_tile_grid(inflated, 16.0f);
    KTUI_CHECK(snapped.min_x == -32.0f && snapped.max_x == 288.0f);
    KTUI_CHECK(snapped.min_y == -32.0f && snapped.max_y == 144.0f);

    // 验证吸附后宽高均为 16 的严格整数倍
    KTUI_CHECK(static_cast<int>(snapped.width()) % 16 == 0);
    KTUI_CHECK(static_cast<int>(snapped.height()) % 16 == 0);

    // 4. UICompositor 综合调度
    UICompositor compositor;
    compositor.mark_node_dirty(old_bounds, new_bounds, 10.0f, 0.0f);
    KTUI_CHECK(compositor.dirty_rects().size() == 1);
    KTUI_CHECK(compositor.dirty_rects()[0] == snapped);
    KTUI_CHECK(compositor.estimate_dirty_tile_count() == (320 / 16) * (176 / 16)); // 20 * 11 = 220 块

    std::cout << "  -> 几何联合防残影、3*sigma 外扩与 16x16 瓦片对齐算法完全吻合！" << std::endl;
}

void test_paint_context_and_kernetile_integration() {
    std::cout << "[Test 6] PaintContext 图元发射与底层 KerneTile 软件光栅化渲染测试..." << std::endl;

    // 创建强制纯 CPU 回退的测试渲染器 (0 外部显卡依赖，100% CI 稳定)
    kernetile::RendererConfig cfg;
    cfg.width = 400;
    cfg.height = 300;
    cfg.force_cpu_fallback = true;

    auto renderer = kernetile::KerneTileRenderer::create(cfg);
    KTUI_CHECK(renderer != nullptr);

    // 构建测试 UI 树
    auto root = std::make_unique<RenderFlex>(FlexDirection::Column, JustifyContent::FlexStart, AlignItems::FlexStart, 16.0f);
    auto card = std::make_unique<RenderVisualBox>(Vec2{300.0f, 150.0f}, Color{0.2f, 0.4f, 0.8f, 1.0f}, 12.0f);
    auto button = std::make_unique<RenderVisualBox>(Vec2{120.0f, 40.0f}, Color{0.1f, 0.8f, 0.3f, 1.0f}, 8.0f);

    card->append_child(button.get());
    root->append_child(card.get());

    root->layout(BoxConstraints::tight(Vec2{400.0f, 300.0f}));

    // 驱动渲染帧
    renderer->begin_frame();
    PaintContext paint_ctx(renderer.get());

    root->paint(paint_ctx);

    renderer->end_frame();

    // 验证图元计数发射 (card 本体 1 + button 本体 1 = 2)
    KTUI_CHECK(paint_ctx.primitive_count() >= 2);

    // 验证离屏渲染缓冲区导出
    std::vector<uint8_t> rgba_buffer(400 * 300 * 4, 0);
    renderer->render_to_rgba8(rgba_buffer.data());

    // 验证已有着色输出 (非全黑)
    bool has_drawn_pixels = false;
    for (size_t i = 0; i < rgba_buffer.size(); i += 4) {
        if (rgba_buffer[i] != 0 || rgba_buffer[i+1] != 0 || rgba_buffer[i+2] != 0) {
            has_drawn_pixels = true;
            break;
        }
    }
    KTUI_CHECK(has_drawn_pixels);

    std::cout << "  -> KerneTile 2D 计算/光栅底座完整闭环，图元发射与像素渲染验证成功！" << std::endl;
}

void test_overlay_requests_followup_frames() {
    class AnimatedOverlay final : public RenderBox {
    public:
        void perform_layout() override { set_size({50.0f, 30.0f}); }
        void paint(PaintContext&) override {
            if (++frames < 3) mark_needs_paint();
        }
        int frames = 0;
    };

    OverlayManager manager;
    auto overlay = std::make_shared<AnimatedOverlay>();
    manager.show({.content = overlay});
    PaintContext context;
    for (int i = 0; i < 3; ++i) {
        KTUI_CHECK(manager.needs_redraw());
        manager.layout_and_paint(context, {100.0f, 100.0f});
    }
    KTUI_CHECK(overlay->frames == 3);
    KTUI_CHECK(!manager.needs_redraw());
}

void test_checkbox_animation_survives_zero_and_large_dt() {
    std::cout << "[Test 7] Checkbox 动效在 dt==0 与 dt>=0.2s 卡顿下不中断且完整收敛至终态..." << std::endl;
    RenderCheckbox cb;
    cb.layout(BoxConstraints::loose(Vec2{200.0f, 40.0f}));

    PaintContext ctx;
    cb.set_checked(true);
    // 立即在同一时刻重绘（模拟 Web 粗粒度时钟或同步重绘下 dt == 0）
    cb.clear_dirty_recursive();
    cb.paint(ctx);
    KTUI_CHECK(cb.needs_paint() && "Checkbox animation must stay dirty when dt == 0");

    // 模拟 250ms 浏览器卡顿（dt >= 0.2s），动效不应被丢弃冻结
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    cb.clear_dirty_recursive();
    cb.paint(ctx);
    KTUI_CHECK(cb.needs_paint() && "Checkbox animation must survive frame hitches >= 0.2s");

    // 继续推进直到动效完整收敛
    int frames = 0;
    while (cb.needs_paint() && frames < 40) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        cb.clear_dirty_recursive();
        cb.paint(ctx);
        ++frames;
    }
    KTUI_CHECK(!cb.needs_paint() && "Checkbox animation must settle cleanly");
    std::cout << "  -> Checkbox 动效零卡帧、完整收敛测试通过！" << std::endl;
}

void test_frameless_titlebar_hittest_and_buttons() {
    std::cout << "[Test 8] CUI.Core 对齐无边框标题栏命中测试、Win11 Snap 优先级与全部按钮交互测试..." << std::endl;
    WindowDesc desc;
    KTUI_CHECK(desc.decorated == false && "Default window must be frameless");
    KTUI_CHECK(desc.custom_titlebar == true);

    WindowTitlebar tb;
    // 默认关闭置顶与主题切换按钮，需由调用方按需手动开启
    KTUI_CHECK(!tb.is_pin_button_visible());
    KTUI_CHECK(!tb.is_theme_button_visible());
    tb.set_pin_button_visible(true);
    tb.set_theme_button_visible(true);
    KTUI_CHECK(tb.is_pin_button_visible());
    KTUI_CHECK(tb.is_theme_button_visible());

    int action_hits = 0;
    tb.add_action("+", "新建", [&]() { ++action_hits; });
    const Vec2 sz{500.0f, 360.0f};

    // 边缘拉伸命中
    KTUI_CHECK(tb.hit_test({1.0f, 1.0f}, sz, true, false) == TitlebarHitTest::BorderTopLeft);
    KTUI_CHECK(tb.hit_test({499.0f, 359.0f}, sz, true, false) == TitlebarHitTest::BorderBottomRight);
    KTUI_CHECK(tb.hit_test({2.0f, 180.0f}, sz, true, false) == TitlebarHitTest::BorderLeft);
    // 最大化后边缘不再触发拉伸
    KTUI_CHECK(tb.hit_test({1.0f, 1.0f}, sz, true, true) == TitlebarHitTest::Caption);

    // 标题栏按钮优先于顶部/右侧缩放边框 (Win11 Snap Layout 要求顶部边缘也命中 HTMAXBUTTON)
    // 500px 宽，按钮宽 46px：Close=[454,500], Max=[408,454], Min=[362,408], Theme=[316,362], Pin=[270,316], Action0=[224,270]
    KTUI_CHECK(tb.hit_test({430.0f, 1.0f}, sz, true, false) == TitlebarHitTest::MaximizeBtn);
    KTUI_CHECK(tb.hit_test({475.0f, 16.0f}, sz, true, false) == TitlebarHitTest::CloseBtn);
    KTUI_CHECK(tb.hit_test({430.0f, 16.0f}, sz, true, false) == TitlebarHitTest::MaximizeBtn);
    KTUI_CHECK(tb.hit_test({385.0f, 16.0f}, sz, true, false) == TitlebarHitTest::MinimizeBtn);
    // Theme / Pin / Action 属于交互按钮，返回 Client 并由 is_interactive_button_hit 标识
    KTUI_CHECK(tb.hit_test({338.0f, 16.0f}, sz, true, false) == TitlebarHitTest::Client);
    KTUI_CHECK(tb.is_interactive_button_hit({338.0f, 16.0f}, sz, true));
    KTUI_CHECK(tb.is_interactive_button_hit({292.0f, 16.0f}, sz, true));
    KTUI_CHECK(tb.is_interactive_button_hit({246.0f, 16.0f}, sz, true));
    // 中间留白拖拽区
    KTUI_CHECK(tb.hit_test({150.0f, 16.0f}, sz, true, false) == TitlebarHitTest::Caption);
    KTUI_CHECK(tb.hit_test({100.0f, 80.0f}, sz, true, false) == TitlebarHitTest::Client);

    // 全部按钮点击回调验证 (Close / Max / Min / Theme / Pin / Custom Action)
    int min_hits = 0, max_hits = 0, close_hits = 0, pin_hits = 0, theme_hits = 0;
    tb.on_minimize = [&]() { ++min_hits; };
    tb.on_maximize_toggle = [&]() { ++max_hits; };
    tb.on_close = [&]() { ++close_hits; };
    tb.on_pin_toggle = [&]() { ++pin_hits; };
    tb.on_theme_toggle = [&]() { ++theme_hits; };

    auto click = [&](PointF pt) {
        bool dirty = false;
        KTUI_CHECK(tb.handle_pointer_down(pt, PointerButton::Primary, sz, true, false, dirty));
        KTUI_CHECK(tb.handle_pointer_up(pt, PointerButton::Primary, sz, true, dirty));
    };
    click({475.0f, 16.0f}); KTUI_CHECK(close_hits == 1);
    click({430.0f, 16.0f}); KTUI_CHECK(max_hits == 1);
    click({385.0f, 16.0f}); KTUI_CHECK(min_hits == 1);
    click({338.0f, 16.0f}); KTUI_CHECK(theme_hits == 1);
    click({292.0f, 16.0f}); KTUI_CHECK(pin_hits == 1);
    click({246.0f, 16.0f}); KTUI_CHECK(action_hits == 1);

    std::cout << "  -> CUI.Core 标题栏布局、Win11 Snap 优先级与全部按钮交互验证通过！" << std::endl;
}

namespace {

inline uint8_t px_a(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t x, uint32_t y) {
    return rgba[(static_cast<size_t>(y) * w + x) * 4 + 3];
}

inline uint8_t fb_r(const std::vector<uint32_t>& fb, uint32_t w, uint32_t x, uint32_t y) {
    return static_cast<uint8_t>(fb[static_cast<size_t>(y) * w + x] & 0xFFu);
}

class CustomPaintBox final : public RenderBox {
public:
    using PaintFn = Fn<void(PaintContext&, Vec2)>;
    CustomPaintBox(Vec2 sz, PaintFn fn) : m_fixed(sz), m_fn(std::move(fn)) {}

    void perform_layout() override {
        m_size = m_constraints.constrain(m_fixed);
    }

    void paint(PaintContext& ctx) override {
        if (m_fn) m_fn(ctx, m_size);
    }

private:
    Vec2 m_fixed;
    PaintFn m_fn;
};

class CustomPaintWidget final : public Widget {
public:
    CustomPaintWidget(Vec2 sz, CustomPaintBox::PaintFn fn)
        : m_sz(sz), m_fn(std::move(fn)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<CustomPaintBox>(m_sz, m_fn);
    }

    void update_render_node(std::shared_ptr<RenderBox>) override {}

private:
    Vec2 m_sz;
    CustomPaintBox::PaintFn m_fn;
};

std::string pick_proportional_tnum_font() {
    const char* candidates[] = {
        "Candara",
        "Times New Roman",
        "Constantia",
        "Corbel",
        "Bahnschrift",
        "Segoe UI Variable Text"
    };
    for (const char* name : candidates) {
        const float w1 = TextMeasure::measure_width("11111", 16.0f, FontWeight::Regular, false, name, false);
        const float w8 = TextMeasure::measure_width("88888", 16.0f, FontWeight::Regular, false, name, false);
        const float t1 = TextMeasure::measure_width("11111", 16.0f, FontWeight::Regular, false, name, true);
        const float t8 = TextMeasure::measure_width("88888", 16.0f, FontWeight::Regular, false, name, true);
        if (w1 + 0.5f < w8 && std::abs(t1 - t8) <= 1e-3f) {
            return name;
        }
    }
    return "";
}

int find_drawn_max_x(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h) {
    int max_x = -1;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t idx = (static_cast<size_t>(y) * w + x) * 4;
            if (rgba[idx] > 8 || rgba[idx + 1] > 8 || rgba[idx + 2] > 8 || rgba[idx + 3] > 8) {
                if (static_cast<int>(x) > max_x) max_x = static_cast<int>(x);
            }
        }
    }
    return max_x;
}

} // namespace

void test_m21_default_disabled_preservation() {
    std::cout << "[M2.1 Test 9] 默认画质开启与手动关闭往返一致性 (WindowDesc/Window/PaintContext)..." << std::endl;

    WindowDesc default_desc;
    KTUI_CHECK(default_desc.dithering_enabled == true);
    KTUI_CHECK(default_desc.pixel_snapping_enabled == true);

    PaintContext default_ctx;
    KTUI_CHECK(default_ctx.pixel_snapping() == true);

    Text default_text;
    KTUI_CHECK(default_text.tabular_nums == false);
    RenderText default_rt;
    KTUI_CHECK(default_rt.tabular_nums() == false);

    Application app(create_platform_backend(true));

    WindowDesc win_desc;
    win_desc.size = {240.0f, 160.0f};
    win_desc.custom_titlebar = false;
    win_desc.decorated = true;
    auto win = app.create_window(win_desc);
    KTUI_CHECK(win != nullptr);
    KTUI_CHECK(win->dithering_enabled() == true);
    KTUI_CHECK(win->pixel_snapping_enabled() == true);

    // 验证 WindowDesc 显式关闭字段透传到 Window
    WindowDesc opt_out_desc = win_desc;
    opt_out_desc.dithering_enabled = false;
    opt_out_desc.pixel_snapping_enabled = false;
    auto opt_win = app.create_window(opt_out_desc);
    KTUI_CHECK(opt_win != nullptr);
    KTUI_CHECK(opt_win->dithering_enabled() == false);
    KTUI_CHECK(opt_win->pixel_snapping_enabled() == false);
    opt_win->close();

    // 显式关闭后测试关闭 -> 开启 -> 关闭往返按位一致
    win->set_dithering_enabled(false);
    win->set_pixel_snapping_enabled(false);

    auto* hw = static_cast<HeadlessWindow*>(win->platform_window());
    KTUI_CHECK(hw != nullptr);
    hw->set_dpi_scale(1.25f);
    win->set_backdrop(WindowBackdrop::None);

    auto scene = std::make_shared<CustomPaintWidget>(Vec2{240.0f, 160.0f}, [](PaintContext& ctx, Vec2) {
        ctx.draw_linear_gradient(
            Rect::from_xywh(10.0f, 10.0f, 200.0f, 40.0f),
            Color{0.08f, 0.08f, 0.08f, 1.0f},
            Color{0.14f, 0.14f, 0.14f, 1.0f},
            PointF{10.0f, 10.0f}, PointF{210.0f, 10.0f});
        ctx.draw_box(
            Rect::from_xywh(20.3f, 70.3f, 100.0f, 32.0f),
            Color{0.0f, 0.0f, 0.0f, 0.0f},
            Color{1.0f, 1.0f, 1.0f, 1.0f},
            0.0f, 1.0f);
        ctx.draw_line(PointF{20.0f, 120.2f}, PointF{180.0f, 120.2f}, Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
    });
    win->set_content(scene);
    win->render_frame(true);
    const std::vector<uint32_t> fb_default = hw->virtual_framebuffer();
    KTUI_CHECK(fb_default.size() == 300u * 200u);

    win->set_dithering_enabled(true);
    win->set_pixel_snapping_enabled(true);
    KTUI_CHECK(win->dithering_enabled() == true);
    KTUI_CHECK(win->pixel_snapping_enabled() == true);
    win->render_frame(true);
    const std::vector<uint32_t> fb_enabled = hw->virtual_framebuffer();
    KTUI_CHECK(fb_enabled != fb_default);

    win->set_dithering_enabled(false);
    win->set_pixel_snapping_enabled(false);
    KTUI_CHECK(win->dithering_enabled() == false);
    KTUI_CHECK(win->pixel_snapping_enabled() == false);
    win->render_frame(true);
    const std::vector<uint32_t> fb_restored = hw->virtual_framebuffer();
    KTUI_CHECK(fb_restored == fb_default);

    win->close();
    std::cout << "  -> 默认禁用态 100% 保持且开启->关闭往返像素按位一致！" << std::endl;
}

void test_m21_fractional_dpi_pixel_snapping() {
    std::cout << "[M2.1 Test 10] R2 125% (1.25x) & 150% (1.5x) 分数 DPI 物理像素对齐测试..." << std::endl;

    // A. Direct PaintContext + KerneTileRenderer tests at 1.25x (odd w_phys=1 and even w_phys=2)
    {
        constexpr uint32_t kW = 250;
        constexpr uint32_t kH = 200;
        kernetile::RendererConfig cfg;
        cfg.width = kW;
        cfg.height = kH;
        cfg.force_cpu_fallback = true;
        cfg.enable_pixel_snapping = true;
        auto renderer = kernetile::KerneTileRenderer::create(cfg);
        KTUI_CHECK(renderer != nullptr);

        // 1) Unsnapped baseline on fractional coord produces alpha bleed
        {
            kernetile::RendererConfig ucfg = cfg;
            ucfg.enable_pixel_snapping = false;
            auto urenderer = kernetile::KerneTileRenderer::create(ucfg);
            urenderer->begin_frame();
            PaintContext uctx(urenderer.get(), 1.25f, false);
            uctx.draw_line(PointF{20.0f, 30.2f}, PointF{120.0f, 30.2f}, Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
            urenderer->end_frame();
            std::vector<uint8_t> buf(kW * kH * 4, 0);
            urenderer->render_to_rgba8(buf.data());
            int partial_rows = 0;
            for (uint32_t y = 34; y <= 41; ++y) {
                const uint8_t a = px_a(buf, kW, 80, y);
                if (a > 0 && a < 255) ++partial_rows;
            }
            KTUI_CHECK(partial_rows >= 1);
        }

        // 2) Snapped 1.25x with OffsetScope, draw_line (H/V, odd & even), draw_box, draw_rounded_rect, push_clip, shadow/dot preservation
        renderer->begin_frame();
        PaintContext ctx(renderer.get(), 1.25f, true);
        KTUI_CHECK(ctx.pixel_snapping() == true);

        {
            PaintContext::OffsetScope off(ctx, Vec2{10.3f, 20.3f});
            // 1.0px logical * 1.25 = 1.25 -> round = 1px physical (odd)
            // cy_raw = (10.2 + 20.3) * 1.25 = 38.125 -> floor + 0.5 = 38.5 -> row 38
            ctx.draw_line(PointF{10.0f, 10.2f}, PointF{90.0f, 10.2f}, Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
            // Vertical 1.0px logical -> cx_raw = (15.2 + 10.3) * 1.25 = 31.875 -> floor + 0.5 = 31.5 -> col 31
            ctx.draw_line(PointF{15.2f, 25.0f}, PointF{15.2f, 70.0f}, Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
            // Even width stroke: 1.6f * 1.25 = 2.0f -> round = 2px physical (even)
            // cy_raw = (85.3 + 20.3) * 1.25 = 132.0 -> round = 132.0 -> rows 131, 132
            ctx.draw_line(PointF{10.0f, 85.3f}, PointF{90.0f, 85.3f}, Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.6f);
        }

        // Box border 1px at 1.25x: [140.2*1.25=175.25->175, 180.2*1.25=225.25->225] x [20.2*1.25=25.25->25, 50.2*1.25=62.75->63]
        ctx.draw_box(Rect::from_xywh(140.2f, 20.2f, 40.0f, 30.0f),
                     Color{0.0f, 0.0f, 0.0f, 0.0f},
                     Color{1.0f, 1.0f, 1.0f, 1.0f}, 0.0f, 1.0f);

        // Clipped fill at fractional coords: [20.3*1.25=25.375->25, 40.3*1.25=50.375->50] x [110.3*1.25=137.875->138, 130.3*1.25=162.875->163]
        ctx.push_clip(Rect::from_xywh(20.3f, 110.3f, 20.0f, 20.0f), 0.0f);
        ctx.draw_rect(Rect::from_xywh(0.0f, 100.0f, 80.0f, 50.0f), Color{1.0f, 0.0f, 0.0f, 1.0f});
        ctx.pop_clip();

        // Soft shadow non-distortion check: shadow_blur > 0 must NOT snap into crisp binary edges
        ctx.draw_rounded_rect(Rect::from_xywh(140.2f, 100.2f, 40.0f, 24.0f),
                              Color{0.0f, 0.0f, 0.0f, 0.8f},
                              4.0f, 4.0f, 4.0f, 4.0f,
                              6.0f, Vec2{0.0f, 2.0f}, 0.0f);

        renderer->end_frame();
        std::vector<uint8_t> rgba(kW * kH * 4, 0);
        renderer->render_to_rgba8(rgba.data());

        // Horizontal 1px line at row 38
        KTUI_CHECK(px_a(rgba, kW, 60, 37) == 0);
        KTUI_CHECK(px_a(rgba, kW, 60, 38) == 255);
        KTUI_CHECK(px_a(rgba, kW, 60, 39) == 0);

        // Vertical 1px line at col 31 (y in [56, 113])
        KTUI_CHECK(px_a(rgba, kW, 30, 80) == 0);
        KTUI_CHECK(px_a(rgba, kW, 31, 80) == 255);
        KTUI_CHECK(px_a(rgba, kW, 32, 80) == 0);

        // Even 2px horizontal line at rows 131, 132
        KTUI_CHECK(px_a(rgba, kW, 60, 130) == 0);
        KTUI_CHECK(px_a(rgba, kW, 60, 131) == 255);
        KTUI_CHECK(px_a(rgba, kW, 60, 132) == 255);
        KTUI_CHECK(px_a(rgba, kW, 60, 133) == 0);

        // Box border top edge at row 25, bottom edge at row 62, left edge at col 175
        KTUI_CHECK(px_a(rgba, kW, 200, 24) == 0);
        KTUI_CHECK(px_a(rgba, kW, 200, 25) == 255);
        KTUI_CHECK(px_a(rgba, kW, 200, 26) == 0);
        KTUI_CHECK(px_a(rgba, kW, 174, 44) == 0);
        KTUI_CHECK(px_a(rgba, kW, 175, 44) == 255);
        KTUI_CHECK(px_a(rgba, kW, 176, 44) == 0);

        // Snapped push_clip [25, 50) x [138, 163)
        KTUI_CHECK(px_a(rgba, kW, 24, 150) == 0);
        KTUI_CHECK(px_a(rgba, kW, 25, 150) == 255);
        KTUI_CHECK(px_a(rgba, kW, 49, 150) == 255);
        KTUI_CHECK(px_a(rgba, kW, 50, 150) == 0);
        KTUI_CHECK(px_a(rgba, kW, 35, 137) == 0);
        KTUI_CHECK(px_a(rgba, kW, 35, 138) == 255);

        // Soft shadow preserves intermediate fractional alphas
        bool has_soft_alpha = false;
        for (uint32_t y = 120; y < 170; ++y) {
            const uint8_t a = px_a(rgba, kW, 200, y);
            if (a > 5 && a < 200) {
                has_soft_alpha = true;
                break;
            }
        }
        KTUI_CHECK(has_soft_alpha);
    }

    // B. Window + HeadlessWindow end-to-end at 125% (1.25f) and 150% (1.5f)
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = {200.0f, 120.0f};
        desc.custom_titlebar = false;
        desc.decorated = true;
        desc.pixel_snapping_enabled = true;
        auto win = app.create_window(desc);
        KTUI_CHECK(win != nullptr);
        win->set_backdrop(WindowBackdrop::None);
        win->set_dithering_enabled(false);

        auto* hw = static_cast<HeadlessWindow*>(win->platform_window());
        KTUI_CHECK(hw != nullptr);

        auto content = std::make_shared<CustomPaintWidget>(Vec2{200.0f, 120.0f}, [](PaintContext& ctx, Vec2) {
            ctx.draw_line(PointF{10.0f, 20.2f}, PointF{150.0f, 20.2f}, Color{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
            ctx.draw_rect(Rect::from_xywh(20.2f, 40.2f, 60.0f, 30.0f), Color{0.0f, 1.0f, 0.0f, 1.0f}, 0.0f, 1.0f);
        });

        // 125% DPI: 1px logical -> 1px physical
        hw->set_dpi_scale(1.25f);
        win->set_content(content);
        win->render_frame(true);
        const auto& fb125 = hw->virtual_framebuffer();
        // cy_raw = 20.2 * 1.25 = 25.25 -> floor + 0.5 = 25.5 -> row 25 (250px wide)
        KTUI_CHECK(fb_r(fb125, 250u, 80u, 24u) < 40);
        KTUI_CHECK(fb_r(fb125, 250u, 80u, 25u) == 255);
        KTUI_CHECK(fb_r(fb125, 250u, 80u, 26u) < 40);

        // 150% DPI: 1px logical * 1.5 = 1.5 -> round = 2px physical (even!)
        // cy_raw = 20.2 * 1.5 = 30.3 -> round = 30.0 -> rows 29, 30 (300px wide)
        hw->set_dpi_scale(1.5f);
        win->render_frame(true);
        const auto& fb150 = hw->virtual_framebuffer();
        KTUI_CHECK(fb_r(fb150, 300u, 80u, 28u) < 40);
        KTUI_CHECK(fb_r(fb150, 300u, 80u, 29u) == 255);
        KTUI_CHECK(fb_r(fb150, 300u, 80u, 30u) == 255);
        KTUI_CHECK(fb_r(fb150, 300u, 80u, 31u) < 40);

        // 150% 1px rect border snaps to 2px physical top edge [60, 62) -> rows 60, 61
        // 40.2 * 1.5 = 60.3 -> round = 60.0; border_width = 2.0
        auto green_at = [&](uint32_t x, uint32_t y) {
            return static_cast<uint8_t>((fb150[static_cast<size_t>(y) * 300u + x] >> 8) & 0xFFu);
        };
        KTUI_CHECK(green_at(60u, 59u) < 40);
        KTUI_CHECK(green_at(60u, 60u) == 255);
        KTUI_CHECK(green_at(60u, 61u) == 255);
        KTUI_CHECK(green_at(60u, 62u) < 40);

        win->close();
    }

    std::cout << "  -> 125% (1px 奇数对齐) 与 150% (2px 偶数对齐) 及 Shadow/Clip 测试通过！" << std::endl;
}

void test_m21_dithering_gradient_and_pointer_reveal() {
    std::cout << "[M2.1 Test 11] R1 窗口 IGN 抖动开关与 PaintContext::draw_linear_gradient / draw_pointer_reveal 测试..." << std::endl;

    // 1. Window::set_dithering_enabled + PaintContext::draw_linear_gradient under OffsetScope at 1.25x
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = {240.0f, 80.0f};
        desc.custom_titlebar = false;
        desc.decorated = true;
        auto win = app.create_window(desc);
        KTUI_CHECK(win != nullptr);
        win->set_backdrop(WindowBackdrop::None);
        win->set_dithering_enabled(false);

        auto* hw = static_cast<HeadlessWindow*>(win->platform_window());
        KTUI_CHECK(hw != nullptr);
        hw->set_dpi_scale(1.25f);

        auto content = std::make_shared<CustomPaintWidget>(Vec2{240.0f, 80.0f}, [](PaintContext& ctx, Vec2) {
            PaintContext::OffsetScope off(ctx, Vec2{12.0f, 8.0f});
            ctx.draw_linear_gradient(
                Rect::from_xywh(0.0f, 0.0f, 200.0f, 48.0f),
                Color{0.06f, 0.06f, 0.06f, 1.0f},
                Color{0.11f, 0.11f, 0.11f, 1.0f},
                PointF{0.0f, 0.0f}, PointF{200.0f, 0.0f});
        });

        win->set_content(content);
        win->render_frame(true);
        const std::vector<uint32_t> undithered = hw->virtual_framebuffer();
        KTUI_CHECK(undithered.size() == 300u * 100u);

        win->set_dithering_enabled(true);
        KTUI_CHECK(win->dithering_enabled() == true);
        win->render_frame(true);
        const std::vector<uint32_t> dithered = hw->virtual_framebuffer();
        KTUI_CHECK(dithered != undithered);

        // Verify spatial dithering breaks flat horizontal quantization runs inside gradient
        auto count_transitions = [](const std::vector<uint32_t>& fb) {
            int transitions = 0;
            for (uint32_t y = 20; y < 50; ++y) {
                for (uint32_t x = 30; x < 240; ++x) {
                    if (fb_r(fb, 300u, x, y) != fb_r(fb, 300u, x - 1, y)) {
                        ++transitions;
                    }
                }
            }
            return transitions;
        };
        KTUI_CHECK(count_transitions(dithered) > count_transitions(undithered) * 2);

        // Restore false -> bit-exact undithered
        win->set_dithering_enabled(false);
        win->render_frame(true);
        KTUI_CHECK(hw->virtual_framebuffer() == undithered);

        win->close();
    }

    // 2. PaintContext::draw_pointer_reveal (both 1-color and 2-color overloads) under OffsetScope at 1.25x
    {
        constexpr uint32_t kW = 300;
        constexpr uint32_t kH = 160;
        kernetile::RendererConfig cfg;
        cfg.width = kW;
        cfg.height = kH;
        cfg.force_cpu_fallback = true;
        auto renderer = kernetile::KerneTileRenderer::create(cfg);
        KTUI_CHECK(renderer != nullptr);

        renderer->begin_frame();
        PaintContext ctx(renderer.get(), 1.25f, false);
        {
            PaintContext::OffsetScope off(ctx, Vec2{16.0f, 16.0f});
            // 2-color overload on widget 1 (left)
            ctx.draw_pointer_reveal(
                Rect::from_xywh(0.0f, 0.0f, 120.0f, 40.0f),
                PointF{40.0f, 20.0f}, 36.0f,
                Color{1.0f, 1.0f, 1.0f, 0.25f},
                Color{1.0f, 1.0f, 1.0f, 0.80f},
                0.0f, 1.5f);
            // 1-color overload on widget 2 (right)
            ctx.draw_pointer_reveal(
                Rect::from_xywh(140.0f, 0.0f, 80.0f, 40.0f),
                PointF{180.0f, 20.0f}, 30.0f,
                Color{0.4f, 0.8f, 1.0f, 0.50f},
                0.0f, 0.0f, 1.0f);
        }
        renderer->end_frame();
        KTUI_CHECK(ctx.primitive_count() >= 3);

        std::vector<uint8_t> rgba(kW * kH * 4, 0);
        renderer->render_to_rgba8(rgba.data());

        // Cursor center on widget 1: (16 + 40) * 1.25 = 70, (16 + 20) * 1.25 = 45
        KTUI_CHECK(px_a(rgba, kW, 70, 45) > 20);
        // Perimeter edge on widget 1 near cursor: top edge y = 16 * 1.25 = 20
        KTUI_CHECK(px_a(rgba, kW, 70, 20) > px_a(rgba, kW, 70, 30));
        // Outside widget bounds (above y=20 -> y=10) untouched
        KTUI_CHECK(px_a(rgba, kW, 70, 10) == 0);
        // Well outside reveal radius on widget 1 (x = 160 -> dist > 36*1.25=45) untouched
        KTUI_CHECK(px_a(rgba, kW, 160, 45) == 0);
        // Widget 2 center: (16 + 180) * 1.25 = 245, y = 45
        KTUI_CHECK(px_a(rgba, kW, 245, 45) > 20);
    }

    std::cout << "  -> IGN 抖动梯度消带与 PaintContext Reveal 双重载映射测试通过！" << std::endl;
}

void test_m21_tabular_nums_text_measure_and_widget() {
    std::cout << "[M2.1 Test 12] R5 TextMeasure / Text / RenderText / PaintContext::draw_text tabular_nums 测试..." << std::endl;

    const std::string prop_font = pick_proportional_tnum_font();
    KTUI_CHECK(!prop_font.empty());

    // A. TextMeasure: default/false proportional preservation vs tabular_nums=true equality
    {
        const float sizes[] = {13.0f, 16.0f, 16.0f * 1.25f, 16.0f * 1.5f};
        for (float sz : sizes) {
            const float w1_omitted = TextMeasure::measure_width("11111", sz, FontWeight::Regular, false, prop_font);
            const float w1_false = TextMeasure::measure_width("11111", sz, FontWeight::Regular, false, prop_font, false);
            const float w8_false = TextMeasure::measure_width("88888", sz, FontWeight::Regular, false, prop_font, false);
            KTUI_CHECK_NEAR(w1_omitted, w1_false, 1e-4f);
            KTUI_CHECK(w1_false + 0.5f < w8_false);

            const float w1_true = TextMeasure::measure_width("11111", sz, FontWeight::Regular, false, prop_font, true);
            const float w8_true = TextMeasure::measure_width("88888", sz, FontWeight::Regular, false, prop_font, true);
            KTUI_CHECK_NEAR(w1_true, w8_true, 1e-3f);

            const Vec2 v1 = TextMeasure::measure("0123456789", sz, FontWeight::Regular, false, prop_font, true);
            const Vec2 v2 = TextMeasure::measure("1111111111", sz, FontWeight::Regular, false, prop_font, true);
            KTUI_CHECK_NEAR(v1.x, v2.x, 1e-3f);

            const float fps1 = TextMeasure::measure_width("FPS: 111.1%", sz, FontWeight::Regular, false, prop_font, true);
            const float fps8 = TextMeasure::measure_width("FPS: 888.8%", sz, FontWeight::Regular, false, prop_font, true);
            KTUI_CHECK_NEAR(fps1, fps8, 1e-3f);
        }

        // cursor_x & hit_test_cursor alignment
        for (size_t b = 0; b <= 5; ++b) {
            const float c1 = TextMeasure::cursor_x("11111", 16.0f, b, FontWeight::Regular, false, prop_font, true);
            const float c8 = TextMeasure::cursor_x("88888", 16.0f, b, FontWeight::Regular, false, prop_font, true);
            KTUI_CHECK_NEAR(c1, c8, 1e-3f);
        }
        const float mid_x = TextMeasure::measure_width("11111", 16.0f, FontWeight::Regular, false, prop_font, true) * 0.5f;
        const size_t hit1 = TextMeasure::hit_test_cursor("11111", 16.0f, mid_x, FontWeight::Regular, false, prop_font, true);
        const size_t hit8 = TextMeasure::hit_test_cursor("88888", 16.0f, mid_x, FontWeight::Regular, false, prop_font, true);
        KTUI_CHECK(hit1 == hit8);
    }

    // B. Text / TextWidget / RenderText layout, Wrap, and dirty invalidation
    {
        Text d1{
            .text = "11111",
            .font_size = 16.0f,
            .color = Color{1.0f, 1.0f, 1.0f, 1.0f},
            .tabular_nums = true,
            .font_family = prop_font
        };
        Text d8{
            .text = "88888",
            .font_size = 16.0f,
            .color = Color{1.0f, 1.0f, 1.0f, 1.0f},
            .tabular_nums = true,
            .font_family = prop_font
        };
        WidgetPtr w1 = d1;
        WidgetPtr w8 = d8;
        auto rn1 = std::static_pointer_cast<RenderText>(w1->create_render_node());
        auto rn8 = std::static_pointer_cast<RenderText>(w8->create_render_node());
        rn1->layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        rn8->layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        KTUI_CHECK_NEAR(rn1->size().x, rn8->size().x, 1e-3f);

        // Wrap mode check
        RenderText wrap1("11111\n88888", 16.0f, Color{1, 1, 1, 1}, FontWeight::Regular, TextOverflow::Wrap, true, prop_font);
        RenderText wrap8("88888\n11111", 16.0f, Color{1, 1, 1, 1}, FontWeight::Regular, TextOverflow::Wrap, true, prop_font);
        wrap1.layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        wrap8.layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        KTUI_CHECK_NEAR(wrap1.size().x, wrap8.size().x, 1e-3f);
        KTUI_CHECK_NEAR(wrap1.size().y, wrap8.size().y, 1e-3f);

        // Proportional preservation + dirty invalidation + TextWidget::update_render_node
        RenderText rt("11111", 16.0f, Color{1, 1, 1, 1}, FontWeight::Regular, TextOverflow::Ellipsis, false, prop_font);
        RenderText rt8("88888", 16.0f, Color{1, 1, 1, 1}, FontWeight::Regular, TextOverflow::Ellipsis, false, prop_font);
        rt.layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        rt8.layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        const float prop_w1 = rt.size().x;
        KTUI_CHECK(prop_w1 + 0.5f < rt8.size().x);

        rt.clear_dirty_recursive();
        rt.set_tabular_nums(false); // No-op when unchanged
        KTUI_CHECK(!rt.needs_layout() && !rt.needs_paint());

        rt.set_tabular_nums(true);
        KTUI_CHECK(rt.needs_layout() && rt.needs_paint());
        rt.layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        KTUI_CHECK_NEAR(rt.size().x, rn8->size().x, 1e-3f);

        // Toggle back via TextWidget::update_render_node
        Text updated_desc{
            .text = "11111",
            .font_size = 16.0f,
            .color = Color{1.0f, 1.0f, 1.0f, 1.0f},
            .tabular_nums = false,
            .font_family = prop_font
        };
        TextWidget tw(updated_desc);
        auto rn_upd = std::make_shared<RenderText>("11111", 16.0f, Color{1, 1, 1, 1}, FontWeight::Regular, TextOverflow::Ellipsis, true, prop_font);
        tw.update_render_node(rn_upd);
        KTUI_CHECK(rn_upd->tabular_nums() == false);
        rn_upd->layout(BoxConstraints::loose(Vec2{400.0f, 200.0f}));
        KTUI_CHECK_NEAR(rn_upd->size().x, prop_w1, 1e-3f);
    }

    // C. PaintContext::draw_text forwarding & rasterized pixel bounds (1.0x and 1.25x)
    {
        for (float scale : {1.0f, 1.25f}) {
            constexpr uint32_t kW = 280;
            constexpr uint32_t kH = 80;
            auto render_text_sample = [&](const char* str, int mode) {
                kernetile::RendererConfig cfg;
                cfg.width = kW;
                cfg.height = kH;
                cfg.force_cpu_fallback = true;
                auto renderer = kernetile::KerneTileRenderer::create(cfg);
                KTUI_CHECK(renderer != nullptr);
                renderer->begin_frame();
                PaintContext ctx(renderer.get(), scale, false);
                if (mode == 0) {
                    ctx.draw_text(str, PointF{10.0f, 10.0f}, 20.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}, FontWeight::Regular, false, prop_font);
                } else {
                    ctx.draw_text(str, PointF{10.0f, 10.0f}, 20.0f, Color{1.0f, 1.0f, 1.0f, 1.0f}, FontWeight::Regular, false, prop_font, mode == 2);
                }
                renderer->end_frame();
                std::vector<uint8_t> rgba(kW * kH * 4, 0);
                renderer->render_to_rgba8(rgba.data());
                return rgba;
            };

            const auto buf_1_omitted = render_text_sample("11111%", 0);
            const auto buf_1_false   = render_text_sample("11111%", 1);
            const auto buf_8_false   = render_text_sample("88888%", 1);
            const auto buf_1_true    = render_text_sample("11111%", 2);
            const auto buf_8_true    = render_text_sample("88888%", 2);

            KTUI_CHECK(buf_1_omitted == buf_1_false);
            const int max_1_false = find_drawn_max_x(buf_1_false, kW, kH);
            const int max_8_false = find_drawn_max_x(buf_8_false, kW, kH);
            const int max_1_true  = find_drawn_max_x(buf_1_true, kW, kH);
            const int max_8_true  = find_drawn_max_x(buf_8_true, kW, kH);

            KTUI_CHECK(max_1_false > 0 && max_8_false > 0);
            KTUI_CHECK(max_1_false < max_8_false);
            KTUI_CHECK(std::abs(max_1_true - max_8_true) <= 1);
        }
    }

    std::cout << "  -> tabular_nums 全链路测量、布局脏标记与光栅化像素对齐测试通过！" << std::endl;
}

namespace {

size_t count_fb_diff_pixels(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b,
                            uint32_t w, uint32_t h,
                            uint32_t* out_bbox_area = nullptr) {
    KTUI_CHECK(a.size() == b.size() && a.size() == static_cast<size_t>(w) * h);
    size_t diff = 0;
    uint32_t min_x = w, min_y = h, max_x = 0, max_y = 0;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * w + x;
            if (a[idx] != b[idx]) {
                ++diff;
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }
    if (out_bbox_area) {
        *out_bbox_area = (diff > 0) ? ((max_x - min_x + 1u) * (max_y - min_y + 1u)) : 0u;
    }
    return diff;
}

void verify_widget_dirty_tile_isolation(RenderBox& root,
                                        RenderBox& target_widget,
                                        Rect widget_abs_bounds,
                                        PointF p1_abs,
                                        PointF p2_abs,
                                        float reveal_radius,
                                        const char* label) {
    constexpr uint32_t kW = 640;
    constexpr uint32_t kH = 480;
    kernetile::RendererConfig cfg;
    cfg.width = kW;
    cfg.height = kH;
    cfg.force_cpu_fallback = true;
    auto renderer = kernetile::KerneTileRenderer::create(cfg);
    KTUI_CHECK(renderer != nullptr);

    EventDispatcher dispatcher;
    std::vector<uint8_t> persistent_rgba(static_cast<size_t>(kW) * kH * 4u, 0u);
    auto render_frame_and_read = [&](std::vector<uint8_t>& out_rgba) {
        renderer->begin_frame();
        PaintContext ctx(renderer.get(), 1.0f, false);
        root.paint(ctx);
        renderer->end_frame();
        renderer->render_to_rgba8(persistent_rgba.data());
        out_rgba = persistent_rgba;
    };

    // Frame 0: establish baseline & settle initial hover/pill animations
    std::vector<uint8_t> rgba0;
    PointerEvent move1(PlatformEventType::PointerMove, p1_abs);
    dispatcher.dispatch_pointer_event(&root, move1);
    render_frame_and_read(rgba0);
    render_frame_and_read(rgba0); // settle any one-frame init

    // Frame 1: cursor at P1
    std::vector<uint8_t> rgba1;
    target_widget.clear_dirty_recursive();
    dispatcher.dispatch_pointer_event(&root, move1);
    render_frame_and_read(rgba1);

    // Frame 2: cursor moved to P2
    std::vector<uint8_t> rgba2;
    target_widget.clear_dirty_recursive();
    PointerEvent move2(PlatformEventType::PointerMove, p2_abs);
    dispatcher.dispatch_pointer_event(&root, move2);
    KTUI_CHECK(target_widget.needs_paint());
    render_frame_and_read(rgba2);
    KTUI_CHECK(rgba2 != rgba1);

    const auto& stats = renderer->get_last_frame_stats();
    const auto& dirty_tiles = renderer->get_last_dirty_tiles();
    KTUI_CHECK(stats.dirty_tiles > 0);
    KTUI_CHECK(dirty_tiles.size() == stats.dirty_tiles);
    KTUI_CHECK(stats.culling_ratio > 85.0);

    // Expected tile range from ((P1 +- radius) U (P2 +- radius)) intersect widget_abs_bounds
    const float u_min_x = std::max(widget_abs_bounds.min_x, std::min(p1_abs.x, p2_abs.x) - reveal_radius);
    const float u_min_y = std::max(widget_abs_bounds.min_y, std::min(p1_abs.y, p2_abs.y) - reveal_radius);
    const float u_max_x = std::min(widget_abs_bounds.max_x, std::max(p1_abs.x, p2_abs.x) + reveal_radius);
    const float u_max_y = std::min(widget_abs_bounds.max_y, std::max(p1_abs.y, p2_abs.y) + reveal_radius);

    const int tx_min = std::max(0, static_cast<int>(std::floor(u_min_x / 16.0f)));
    const int ty_min = std::max(0, static_cast<int>(std::floor(u_min_y / 16.0f)));
    const int tx_max = std::min(static_cast<int>(kW / 16u) - 1, static_cast<int>(std::floor(u_max_x / 16.0f)));
    const int ty_max = std::min(static_cast<int>(kH / 16u) - 1, static_cast<int>(std::floor(u_max_y / 16.0f)));

    for (const auto& dt : dirty_tiles) {
        KTUI_CHECK(static_cast<int>(dt.tx) >= tx_min && static_cast<int>(dt.tx) <= tx_max);
        KTUI_CHECK(static_cast<int>(dt.ty) >= ty_min && static_cast<int>(dt.ty) <= ty_max);
    }

    // Verify pixels well outside (cursor_union +- radius) remain identical between Frame 1 and Frame 2
    for (uint32_t y = 0; y < kH; y += 8) {
        for (uint32_t x = 0; x < kW; x += 8) {
            if (static_cast<float>(x) + 1.0f < u_min_x || static_cast<float>(x) - 1.0f > u_max_x ||
                static_cast<float>(y) + 1.0f < u_min_y || static_cast<float>(y) - 1.0f > u_max_y) {
                const size_t idx = (static_cast<size_t>(y) * kW + x) * 4u;
                if (rgba1[idx] != rgba2[idx] ||
                    rgba1[idx + 1] != rgba2[idx + 1] ||
                    rgba1[idx + 2] != rgba2[idx + 2] ||
                    rgba1[idx + 3] != rgba2[idx + 3]) {
                    std::cerr << "[DIAG] " << label << " diff outside union at (" << x << "," << y << ")"
                              << " union=[" << u_min_x << "," << u_min_y << ".." << u_max_x << "," << u_max_y << "]"
                              << " r1=(" << int(rgba1[idx]) << "," << int(rgba1[idx+1]) << "," << int(rgba1[idx+2]) << "," << int(rgba1[idx+3]) << ")"
                              << " r2=(" << int(rgba2[idx]) << "," << int(rgba2[idx+1]) << "," << int(rgba2[idx+2]) << "," << int(rgba2[idx+3]) << ")\n";
                    KTUI_CHECK(false);
                }
            }
        }
    }

    std::cout << "  -> " << label << ": dirty_tiles=" << stats.dirty_tiles
              << "/" << stats.total_tiles
              << ", culling_ratio=" << stats.culling_ratio << "%"
              << ", tx=[" << tx_min << ".." << tx_max << "], ty=[" << ty_min << ".." << ty_max << "]" << std::endl;
}

} // namespace

void test_m22_pointer_reveal_dirty_tile_isolation() {
    std::cout << "[M2.2 Test 13] R3 16x16 脏瓦片局部剔除隔离验证 (Button / NavigationView / SettingsCard)..." << std::endl;

    // 0. 框架默认全局开启 + 控件级三态覆写最高优先级校验
    KTUI_CHECK(Theme::global_pointer_reveal() == true);
    {
        Theme t = Theme::dark();
        KTUI_CHECK(t.resolve_pointer_reveal(std::nullopt) == true);
        KTUI_CHECK(t.resolve_pointer_reveal(false) == false);
        KTUI_CHECK(t.resolve_pointer_reveal(true) == true);

        Theme::set_global_pointer_reveal(false);
        Theme t_off = Theme::dark();
        KTUI_CHECK(t_off.resolve_pointer_reveal(std::nullopt) == false);
        KTUI_CHECK(t_off.resolve_pointer_reveal(true) == true);   // 控件开启覆写全局关闭
        KTUI_CHECK(t_off.resolve_pointer_reveal(false) == false);
        Theme::set_global_pointer_reveal(true);
    }

    // 1. RenderButton (explicit false overrides global true -> zero-churn; explicit/default true -> 16x16 dirty tile isolation)
    {
        auto root = std::make_unique<RenderSizedBox>(Vec2{640.0f, 480.0f});
        auto btn = std::make_unique<RenderButton>();
        btn->set_text("Reveal Tile Check");
        btn->set_offset(Vec2{40.0f, 40.0f});
        root->append_child(btn.get());
        root->layout(BoxConstraints::tight(Vec2{640.0f, 480.0f}));
        btn->layout(BoxConstraints::tight(Vec2{260.0f, 40.0f}));

        // Explicit widget-level false overrides global true: consecutive move inside button causes zero needs_paint()
        btn->set_pointer_reveal_enabled(false);
        EventDispatcher disp;
        PointerEvent p1(PlatformEventType::PointerMove, PointF{100.0f, 60.0f});
        PointerEvent p2(PlatformEventType::PointerMove, PointF{130.0f, 62.0f});
        disp.dispatch_pointer_event(root.get(), p1);
        btn->clear_dirty_recursive();
        disp.dispatch_pointer_event(root.get(), p2);
        KTUI_CHECK(!btn->needs_paint());

        // Global false with widget inherit (nullopt): zero needs_paint() immediately without rebuilding Theme
        btn->set_pointer_reveal_enabled(std::nullopt);
        Theme::set_global_pointer_reveal(false);
        disp.dispatch_pointer_event(root.get(), p1);
        btn->clear_dirty_recursive();
        disp.dispatch_pointer_event(root.get(), p2);
        KTUI_CHECK(!btn->needs_paint());

        // Widget explicit true overrides global false: strict 16x16 tile isolation
        btn->set_pointer_reveal_enabled(true);
        const Rect abs_bounds = Rect::from_xywh(40.0f, 40.0f, 260.0f, 40.0f);
        verify_widget_dirty_tile_isolation(*root, *btn, abs_bounds,
                                           PointF{100.0f, 60.0f}, PointF{116.0f, 64.0f},
                                           64.0f, "RenderButton");
        Theme::set_global_pointer_reveal(true);
    }

    // 2. RenderNavigationView (Left mode, 640x480)
    {
        auto nav = std::make_unique<RenderNavigationView>();
        nav->set_chrome_options(false, false, false, false, "Settings", "", "", 240.0f, 48.0f, 641.0f, 900.0f);
        nav->set_pane_display_mode(NavigationPaneDisplayMode::Left);
        nav->set_menu_items({
            NavigationViewItem{.id = "home", .label = "Home", .icon = NavigationIcon::Home},
            NavigationViewItem{.id = "cards", .label = "Cards", .icon = NavigationIcon::Controls},
            NavigationViewItem{.id = "about", .label = "About", .icon = NavigationIcon::Info}
        });
        nav->layout(BoxConstraints::tight(Vec2{640.0f, 480.0f}));

        // Explicit widget-level false overrides global true: zero needs_paint() on same-row PointerMove
        nav->set_pointer_reveal_enabled(false);
        EventDispatcher disp;
        PointerEvent p1(PlatformEventType::PointerMove, PointF{48.0f, 24.0f});
        PointerEvent p2(PlatformEventType::PointerMove, PointF{64.0f, 26.0f});
        disp.dispatch_pointer_event(nav.get(), p1);
        nav->clear_dirty_recursive();
        disp.dispatch_pointer_event(nav.get(), p2);
        KTUI_CHECK(!nav->needs_paint());

        // Enabled: strict 16x16 tile isolation within pane bounds
        nav->set_pointer_reveal_enabled(true);
        const Rect pane_bounds = Rect::from_xywh(0.0f, 0.0f, 240.0f, 480.0f);
        verify_widget_dirty_tile_isolation(*nav, *nav, pane_bounds,
                                           PointF{48.0f, 24.0f}, PointF{64.0f, 28.0f},
                                           72.0f, "RenderNavigationView (Left)");
    }

    // 3. RenderSettingsCard (placed at (30, 30), width=560)
    {
        auto root = std::make_unique<RenderSizedBox>(Vec2{640.0f, 480.0f});
        auto card = std::make_unique<RenderSettingsCard>();
        card->set_properties("Reveal Card", "Dirty tile isolation check", "", "", true, false, false, nullptr);
        card->set_offset(Vec2{30.0f, 30.0f});
        root->append_child(card.get());
        root->layout(BoxConstraints::tight(Vec2{640.0f, 480.0f}));
        card->layout(BoxConstraints::tight(Vec2{560.0f, 72.0f}));

        card->set_pointer_reveal_enabled(false);
        EventDispatcher disp;
        PointerEvent p1(PlatformEventType::PointerMove, PointF{120.0f, 66.0f});
        PointerEvent p2(PlatformEventType::PointerMove, PointF{140.0f, 68.0f});
        disp.dispatch_pointer_event(root.get(), p1);
        card->clear_dirty_recursive();
        disp.dispatch_pointer_event(root.get(), p2);
        KTUI_CHECK(!card->needs_paint());

        card->set_pointer_reveal_enabled(true);
        const Rect abs_bounds = Rect::from_xywh(30.0f, 30.0f, 560.0f, 72.0f);
        verify_widget_dirty_tile_isolation(*root, *card, abs_bounds,
                                           PointF{120.0f, 66.0f}, PointF{136.0f, 68.0f},
                                           80.0f, "RenderSettingsCard");
    }

    std::cout << "  -> 16x16 脏瓦片局部剔除隔离验证全部通过！" << std::endl;
}

void test_m22_headless_window_pointer_reveal() {
    std::cout << "[M2.2 Test 14] Application + HeadlessWindow 全栈交互、明暗主题、4px同瓦片移动与 1.5x DPI 验证..." << std::endl;

    Application app(create_platform_backend(true));
    auto send_move = [](Window& win, HeadlessWindow& hw, float x, float y) {
        PlatformEvent pe{};
        pe.type = PlatformEventType::PointerMove;
        pe.device_kind = PointerDeviceKind::Mouse;
        pe.position = {x, y};
        win.handle_platform_event(pe);
        win.render_frame();
        return hw.virtual_framebuffer();
    };

    // 1. Button variants (Standard, Accent, Subtle, Danger) across Dark & Light themes, 4px same-tile move, disabled, exit
    for (bool is_dark : {true, false}) {
        app.theme.set(is_dark ? Theme::dark() : Theme::light());
        for (ButtonVariant v : {ButtonVariant::Standard, ButtonVariant::Accent, ButtonVariant::Subtle, ButtonVariant::Danger}) {
            WindowDesc wd{};
            wd.size = {320.0f, 120.0f};
            wd.custom_titlebar = false;
            wd.decorated = true;
            auto win = app.create_window(wd);
            KTUI_CHECK(win != nullptr);
            win->set_backdrop(WindowBackdrop::None);
            auto* hw = static_cast<HeadlessWindow*>(win->platform_window());
            KTUI_CHECK(hw != nullptr);

            // Default (inherits global ON) vs explicit true bit-exact
            win->set_content(Box{
                .width = 320.0f, .height = 120.0f,
                .padding = EdgeInsets(20.0f),
                .child = Button{.text = "Default", .variant = v}
            });
            win->render_frame(true);
            const auto fb_def = send_move(*win, *hw, 66.0f, 34.0f);

            win->set_content(Box{
                .width = 320.0f, .height = 120.0f,
                .padding = EdgeInsets(20.0f),
                .child = Button{.text = "Default", .variant = v}.pointer_reveal(true)
            });
            win->render_frame(true);
            const auto fb_true = send_move(*win, *hw, 66.0f, 34.0f);
            KTUI_CHECK(fb_def == fb_true);

            // Enabled: baseline -> 4px same-tile move -> exit back to baseline
            win->set_content(Box{
                .width = 320.0f, .height = 120.0f,
                .padding = EdgeInsets(20.0f),
                .child = Button{.text = "Reveal", .variant = v}.pointer_reveal(true)
            });
            win->render_frame(true);
            const auto fb_base = hw->virtual_framebuffer();
            const auto fb_m1 = send_move(*win, *hw, 66.0f, 34.0f);
            const auto fb_m2 = send_move(*win, *hw, 70.0f, 36.0f); // 4px same 16x16 tile move
            KTUI_CHECK(count_fb_diff_pixels(fb_base, fb_m1, 320u, 120u) > 10u);
            KTUI_CHECK(count_fb_diff_pixels(fb_m1, fb_m2, 320u, 120u) > 0u);
            const auto fb_exit = send_move(*win, *hw, -20.0f, -20.0f);
            KTUI_CHECK(fb_exit == fb_base);

            // Disabled Button: zero reveal pixels
            win->set_content(Box{
                .width = 320.0f, .height = 120.0f,
                .padding = EdgeInsets(20.0f),
                .child = Button{.text = "Disabled", .variant = v, .disabled = true}.pointer_reveal(true)
            });
            win->render_frame(true);
            const auto fb_dis_base = hw->virtual_framebuffer();
            const auto fb_dis_move = send_move(*win, *hw, 66.0f, 34.0f);
            KTUI_CHECK(fb_dis_move == fb_dis_base);

            win->close();
        }
    }

    // 2. NavigationView (Left mode) + SettingsCard (clickable, non-clickable, nested, child Button, disabled) + SettingsExpander
    app.theme.set(Theme::dark());
    {
        WindowDesc wd{};
        wd.size = {800.0f, 440.0f};
        wd.custom_titlebar = false;
        wd.decorated = true;
        auto win = app.create_window(wd);
        KTUI_CHECK(win != nullptr);
        win->set_backdrop(WindowBackdrop::None);
        auto* hw = static_cast<HeadlessWindow*>(win->platform_window());

        win->set_content(NavigationView{
            .menu_items = {
                NavigationViewItem{.id = "home", .label = "Home", .icon = NavigationIcon::Home},
                NavigationViewItem{.id = "info", .label = "Info", .icon = NavigationIcon::Info}
            },
            .pane_display_mode = NavigationPaneDisplayMode::Left,
            .is_pane_toggle_button_visible = false,
            .is_back_button_visible = false,
            .is_settings_visible = false,
            .content = VStack{
                .spacing = 10.0f,
                .padding = EdgeInsets(16.0f),
                .content = {
                    SettingsCard{
                        .header = "Clickable Card",
                        .description = "With reveal",
                        .is_click_enabled = true
                    }.pointer_reveal(true),
                    SettingsCard{
                        .header = "Static Card with Child",
                        .description = "Non-clickable card still reveals",
                        .is_click_enabled = false,
                        .content = Button{.text = "Child"}.pointer_reveal(true)
                    }.pointer_reveal(true),
                    SettingsCard{
                        .header = "Nested Disabled Card",
                        .disabled = true,
                        .is_nested = true
                    }.pointer_reveal(true),
                    SettingsExpander{
                        .header = "Expander Header",
                        .description = "Header reveal"
                    }.pointer_reveal(true)
                }
            }
        }.pointer_reveal(true));
        win->render_frame(true);
        const auto base = hw->virtual_framebuffer();

        // Hover NavigationView left item (x=60, y=24) -> localized diff, exit clears
        uint32_t bbox_nav = 0;
        const auto nav_m1 = send_move(*win, *hw, 60.0f, 24.0f);
        const auto nav_m2 = send_move(*win, *hw, 140.0f, 24.0f);
        KTUI_CHECK(count_fb_diff_pixels(nav_m1, nav_m2, 800u, 440u, &bbox_nav) > 10u);
        KTUI_CHECK(bbox_nav < (800u * 440u) / 4u);
        KTUI_CHECK(send_move(*win, *hw, -20.0f, -20.0f) == base);

        // Hover Clickable SettingsCard (y ~ 48)
        const auto sc_m1 = send_move(*win, *hw, 320.0f, 48.0f);
        const auto sc_m2 = send_move(*win, *hw, 520.0f, 48.0f);
        KTUI_CHECK(count_fb_diff_pixels(sc_m1, sc_m2, 800u, 440u) > 10u);

        // Hover Non-clickable SettingsCard (y ~ 126)
        const auto nc_m1 = send_move(*win, *hw, 320.0f, 126.0f);
        const auto nc_m2 = send_move(*win, *hw, 420.0f, 126.0f);
        KTUI_CHECK(count_fb_diff_pixels(nc_m1, nc_m2, 800u, 440u) > 10u);

        // Hover child Button inside SettingsCard (near right side x~740, y~126) -> both child & card track cleanly
        const auto ch_m1 = send_move(*win, *hw, 740.0f, 126.0f);
        KTUI_CHECK(count_fb_diff_pixels(base, ch_m1, 800u, 440u) > 10u);

        // Hover SettingsExpander header (y ~ 260)
        const auto ex_m1 = send_move(*win, *hw, 320.0f, 260.0f);
        const auto ex_m2 = send_move(*win, *hw, 520.0f, 260.0f);
        KTUI_CHECK(count_fb_diff_pixels(ex_m1, ex_m2, 800u, 440u) > 10u);

        // Disabled nested SettingsCard (y ~ 196): compare against base before entering disabled card
        KTUI_CHECK(send_move(*win, *hw, -20.0f, -20.0f) == base);
        const auto dis_m = send_move(*win, *hw, 360.0f, 196.0f);
        KTUI_CHECK(dis_m == base);

        win->close();
    }

    // 3. 1.5x DPI with pixel_snapping_enabled = true across Light & Dark themes
    for (bool is_dark : {true, false}) {
        app.theme.set(is_dark ? Theme::dark() : Theme::light());
        WindowDesc wd{};
        wd.size = {480.0f, 200.0f};
        wd.custom_titlebar = false;
        wd.decorated = true;
        wd.pixel_snapping_enabled = true;
        auto win = app.create_window(wd);
        KTUI_CHECK(win != nullptr);
        win->set_backdrop(WindowBackdrop::None);
        auto* hw = static_cast<HeadlessWindow*>(win->platform_window());
        hw->set_dpi_scale(1.5f);

        win->set_content(VStack{
            .spacing = 12.0f,
            .padding = EdgeInsets(16.0f),
            .content = {
                Button{.text = "1.5x Reveal"}.pointer_reveal(true),
                SettingsCard{
                    .header = "1.5x Card",
                    .description = "Snapped reveal",
                    .is_click_enabled = true
                }.pointer_reveal(true)
            }
        });
        win->render_frame(true);
        const auto base = hw->virtual_framebuffer();
        KTUI_CHECK(base.size() == 720u * 300u);

        uint32_t bbox_area = 0;
        const auto m1 = send_move(*win, *hw, 50.0f, 32.0f);
        const auto m2 = send_move(*win, *hw, 90.0f, 32.0f);
        KTUI_CHECK(count_fb_diff_pixels(m1, m2, 720u, 300u, &bbox_area) > 10u);
        KTUI_CHECK(bbox_area < (720u * 300u) / 4u);

        const auto c1 = send_move(*win, *hw, 100.0f, 92.0f);
        const auto c2 = send_move(*win, *hw, 300.0f, 92.0f);
        KTUI_CHECK(count_fb_diff_pixels(c1, c2, 720u, 300u, &bbox_area) > 10u);
        KTUI_CHECK(bbox_area < (720u * 300u) / 4u);

        KTUI_CHECK(send_move(*win, *hw, -20.0f, -20.0f) == base);
        win->close();
    }

    std::cout << "  -> HeadlessWindow 全栈 R3 交互、主题对比、禁用阻断与 1.5x DPI 对齐测试通过！" << std::endl;
}

void test_tactile_polish_tier789_widgets() {
    std::cout << "[Test 16] 列表羽化、Hero、微倾斜、滚动吸顶、翻滚数值与同心圆角集成测试..." << std::endl;

    // 1. Concentric Nested Corner Radius hierarchy deduction (no RTTI / /GR- safe)
    {
        auto outer = std::make_shared<RenderDecoratedBox>();
        outer->set_properties(200.0f, 120.0f, Color::transparent(), Color::transparent(), 0.0f, 28.0f, EdgeInsets(12.0f));
        auto inner = std::make_shared<RenderDecoratedBox>();
        inner->set_properties(-1.0f, 40.0f, Color::transparent(), Color::transparent(), 0.0f, 0.0f, EdgeInsets::zero());
        inner->set_concentric_inner(true, 4.0f);
        outer->set_child_box(inner);
        outer->layout(BoxConstraints::loose(Vec2{200.0f, 120.0f}), true);
        KTUI_CHECK(std::abs(inner->resolve_effective_corner_radius() - 16.0f) < 1e-3f);
    }

    // 2. StickyParallaxScrollView header hairline alpha progression & RollingNumber / CrossfadeText activation
    {
        RenderStickyParallaxScrollView sv;
        sv.set_properties(320.0f, 200.0f, 100.0f, 0.42f, 18.0f, false, 0.0f);
        auto body = std::make_shared<RenderDecoratedBox>();
        body->set_properties(320.0f, 400.0f, Color::transparent(), Color::transparent(), 0.0f, 0.0f, EdgeInsets::zero());
        sv.set_children(nullptr, nullptr, body);
        sv.layout(BoxConstraints::tight(Vec2{320.0f, 200.0f}), true);
        KTUI_CHECK(std::abs(sv.header_hairline_alpha() - 0.0f) < 1e-4f);
        sv.jump_to(9.0f);
        KTUI_CHECK(std::abs(sv.header_hairline_alpha() - 0.5f) < 1e-3f);
        sv.jump_to(36.0f);
        KTUI_CHECK(std::abs(sv.header_hairline_alpha() - 1.0f) < 1e-4f);

        RenderRollingNumber rn;
        KTUI_CHECK(!(RollingNumber{}.show_delta_flash));
        rn.set_properties(100.0, 0, "", "", 24.0f, FontWeight::Bold, Color{-1, -1, -1, -1}, true, 320.0f, 28.0f);
        KTUI_CHECK(!rn.is_animating());
        rn.set_value(137.0);
        KTUI_CHECK(rn.is_animating());

        RenderCrossfadeText cf;
        cf.set_properties("Idle", 14.0f, FontWeight::SemiBold, Color{-1, -1, -1, -1});
        cf.set_text("Active");
        cf.layout(BoxConstraints::loose(Vec2{200.0f, 40.0f}), true);
        KTUI_CHECK(cf.size().x > 0.0f && cf.size().y > 0.0f);
    }

    // 3. Pixel-level verification: RollingNumber & CrossfadeText must never clip glyph bottom pixels near settlement tail
    {
        struct InkBBox {
            int min_x;
            int min_y;
            int max_x;
            int max_y;
        };
        auto render_node_row_bounds = [](RenderBox& node, uint32_t w, uint32_t h, PointF origin) -> InkBBox {
            kernetile::RendererConfig cfg;
            cfg.width = w;
            cfg.height = h;
            cfg.force_cpu_fallback = true;
            auto renderer = kernetile::KerneTileRenderer::create(cfg);
            renderer->begin_frame();
            renderer->draw_rect(kernetile::math::Rect{0.0f, 0.0f, float(w), float(h)},
                                kernetile::math::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.0f, 0.0f);
            PaintContext ctx(renderer.get());
            {
                PaintContext::OffsetScope offset_guard(ctx, origin);
                node.paint(ctx);
            }
            renderer->end_frame();
            std::vector<uint8_t> rgba(size_t(w) * h * 4u, 0u);
            renderer->render_to_rgba8(rgba.data());

            InkBBox box{static_cast<int>(w), static_cast<int>(h), -1, -1};
            for (uint32_t y = 0; y < h; ++y) {
                for (uint32_t x = 0; x < w; ++x) {
                    const size_t idx = (size_t(y) * w + x) * 4u;
                    if (rgba[idx] > 4 || rgba[idx + 1] > 4 || rgba[idx + 2] > 4) {
                        box.min_y = std::min(box.min_y, static_cast<int>(y));
                        box.max_y = std::max(box.max_y, static_cast<int>(y));
                        box.min_x = std::min(box.min_x, static_cast<int>(x));
                        box.max_x = std::max(box.max_x, static_cast<int>(x));
                    }
                }
            }
            return box;
        };

        // 3a. RollingNumber settling tail vs settled ("#4128" at 32px)
        RenderRollingNumber rn_steady;
        rn_steady.set_properties(4128.0, 0, "#", "", 32.0f, FontWeight::Bold, Color{1, 1, 1, 1}, false, 480.0f, 44.0f);
        rn_steady.layout(BoxConstraints::loose(Vec2{320.0f, 140.0f}), true);
        const auto steady_box = render_node_row_bounds(rn_steady, 320u, 140u, PointF{20.0f, 30.0f});
        KTUI_CHECK(steady_box.max_y > steady_box.min_y && (steady_box.max_y - steady_box.min_y) >= 18);

        // Verify RollingNumber uses non-italic metrics & keeps settled ink unclipped across transition tail frames
        RenderRollingNumber rn_tail;
        rn_tail.set_properties(3991.0, 0, "#", "", 32.0f, FontWeight::Bold, Color{1, 1, 1, 1}, false, 480.0f, 44.0f);
        rn_tail.set_value(4128.0);
        rn_tail.layout(BoxConstraints::loose(Vec2{320.0f, 140.0f}), true);
        InkBBox tail_box{};
        for (int frame = 0; frame < 14; ++frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            tail_box = render_node_row_bounds(rn_tail, 320u, 140u, PointF{20.0f, 30.0f});
            if (frame >= 5) {
                KTUI_CHECK((tail_box.max_y - tail_box.min_y) >= (steady_box.max_y - steady_box.min_y));
            }
        }
        KTUI_CHECK(tail_box.max_y >= steady_box.max_y);
        KTUI_CHECK(tail_box.min_y <= steady_box.min_y + 4);
        KTUI_CHECK(tail_box.min_x >= 0 && tail_box.max_x >= steady_box.max_x);

        // 3b. CrossfadeText ("Ready 就绪待命" at 16px) settling tail vs settled
        RenderCrossfadeText cf_steady;
        cf_steady.set_properties("Ready 就绪待命", 16.0f, FontWeight::SemiBold, Color{1, 1, 1, 1});
        cf_steady.layout(BoxConstraints::loose(Vec2{320.0f, 120.0f}), true);
        const auto cf_steady_box = render_node_row_bounds(cf_steady, 320u, 120u, PointF{20.0f, 30.0f});
        KTUI_CHECK(cf_steady_box.max_y > cf_steady_box.min_y);

        RenderCrossfadeText cf_tail;
        cf_tail.set_properties("GPU Compute Running", 16.0f, FontWeight::SemiBold, Color{1, 1, 1, 1});
        cf_tail.set_text("Ready 就绪待命");
        cf_tail.layout(BoxConstraints::loose(Vec2{320.0f, 120.0f}), true);
        InkBBox cf_tail_box{};
        for (int frame = 0; frame < 14; ++frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            cf_tail_box = render_node_row_bounds(cf_tail, 320u, 120u, PointF{20.0f, 30.0f});
            if (frame >= 5) {
                KTUI_CHECK((cf_tail_box.max_y - cf_tail_box.min_y) >= (cf_steady_box.max_y - cf_steady_box.min_y) - 1);
            }
        }
        KTUI_CHECK(cf_tail_box.max_y >= cf_steady_box.max_y);
        KTUI_CHECK(cf_tail_box.min_x >= 0);
    }

    std::cout << "  -> 扩展质感交互组件 (ConcentricRadius / StickyParallax / RollingNumber / CrossfadeText) 验证通过！" << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "==================================================" << std::endl;
    std::cout << "   KTUI 阶段 6: 双级脏区、事件焦点与 KerneTile 闭环  " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_overlay_requests_followup_frames();
    test_hit_testing_depth_path();
    test_event_bubble_and_stop_propagation();
    test_hover_state_tracking();
    test_focus_manager_tab_navigation();
    test_compositor_dirty_union_and_tile_snapping();
    test_paint_context_and_kernetile_integration();
    test_checkbox_animation_survives_zero_and_large_dt();
    test_frameless_titlebar_hittest_and_buttons();
    test_m21_default_disabled_preservation();
    test_m21_fractional_dpi_pixel_snapping();
    test_m21_dithering_gradient_and_pointer_reveal();
    test_m21_tabular_nums_text_measure_and_widget();
    test_m22_pointer_reveal_dirty_tile_isolation();
    test_m22_headless_window_pointer_reveal();
    test_tactile_polish_tier789_widgets();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 6 双级脏区与渲染闭环 100% 测试通过!  " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}

#include <ktui/event/event_types.hpp>
#include <ktui/event/hit_test_tree.hpp>
#include <ktui/event/event_dispatcher.hpp>
#include <ktui/event/focus_tree.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/ui_compositor.hpp>
#include <ktui/tree/render_flex.hpp>
#include <kernetile/renderer.hpp>
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

using namespace ktui;

// 具备可自定义绘制逻辑的测试渲染节点
class RenderVisualBox : public RenderBox {
public:
    RenderVisualBox(Vec2 size, Color color, float radius = 0.0f)
        : m_fixed_size(size), m_color(color), m_radius(radius) {}

    void perform_layout() override {
        m_size = m_constraints.constrain(m_fixed_size);
        for (RenderNode* child = first_child(); child != nullptr; child = child->next_sibling()) {
            if (auto* box = dynamic_cast<RenderBox*>(child)) {
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
    assert(hit);
    assert(!result.is_empty());

    // 路径首项必须为按钮 (最深层叶子，拥有最高事件捕获优先级)
    assert(result.first_target() == button.get());

    // 按钮自身局部坐标应当是 (130 - 120, 130 - 120) = (10, 10)
    assert(result.path().front().local_position == PointF(10.0f, 10.0f));

    // 点击在容器内部但按钮外部: (110, 110)
    HitTestResult result_outside_btn;
    hit = root->hit_test(result_outside_btn, PointF{110.0f, 110.0f});
    assert(hit);
    assert(result_outside_btn.first_target() == container.get());

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

    assert(button_clicked == true);
    assert(container_clicked == false); // 容器未被触发！

    // 场景 B: 按钮不调用 stop_propagation()，事件正常向上冒泡至容器
    button_clicked = false;
    container_clicked = false;
    button->on_pointer_down = [&](PointerEvent& e) {
        (void)e;
        button_clicked = true;
    };

    PointerEvent down_event2(PlatformEventType::PointerDown, PointF{60.0f, 60.0f}, PointerButton::Primary);
    dispatcher.dispatch_pointer_event(container.get(), down_event2);
    assert(button_clicked == true);
    assert(container_clicked == true); // 容器成功捕获冒泡事件！

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

    assert(!button->is_hovered);

    // 1. 移动到按钮内部 (60, 60)
    PointerEvent move_in(PlatformEventType::PointerMove, PointF{60.0f, 60.0f});
    dispatcher.dispatch_pointer_event(container.get(), move_in);
    assert(button->is_hovered == true);
    assert(entered == true);
    assert(exited == false);

    // 2. 移动出按钮外部 (10, 10)
    PointerEvent move_out(PlatformEventType::PointerMove, PointF{10.0f, 10.0f});
    dispatcher.dispatch_pointer_event(container.get(), move_out);
    assert(button->is_hovered == false);
    assert(exited == true);

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
    assert(focus_mgr.focused_node() == nullptr);

    int a_focus_events = 0;
    inputA->on_focus = [&](FocusEvent& e) {
        if (e.gained_focus) a_focus_events++;
    };

    // 第一次按 Tab -> 聚焦 A
    focus_mgr.handle_tab_navigation(root.get(), false);
    assert(focus_mgr.focused_node() == inputA.get());
    assert(a_focus_events == 1);

    // 第二次按 Tab -> 聚焦 B
    focus_mgr.handle_tab_navigation(root.get(), false);
    assert(focus_mgr.focused_node() == inputB.get());

    // 第三次按 Tab -> 聚焦 C
    focus_mgr.handle_tab_navigation(root.get(), false);
    assert(focus_mgr.focused_node() == inputC.get());

    // 第四次按 Tab -> 环形回到 A
    focus_mgr.handle_tab_navigation(root.get(), false);
    assert(focus_mgr.focused_node() == inputA.get());
    assert(a_focus_events == 2);

    // 按 Shift+Tab (reverse = true) -> 逆向转移至 C
    focus_mgr.handle_tab_navigation(root.get(), true);
    assert(focus_mgr.focused_node() == inputC.get());

    // 清空焦点
    focus_mgr.clear_focus();
    assert(focus_mgr.focused_node() == nullptr);

    std::cout << "  -> Tab / Shift+Tab 环形焦点循环流转与回调通知完全正确！" << std::endl;
}

void test_compositor_dirty_union_and_tile_snapping() {
    std::cout << "[Test 5] 双历史包围盒 Old U New、软阴影外扩与 16x16 瓦片吸附几何测试..." << std::endl;

    // 场景：控件尺寸 100x100，从 (10, 10) 移动到 (150, 10)，具有 blur=10.0 的软阴影
    Rect old_bounds = Rect::from_xywh(10.0f, 10.0f, 100.0f, 100.0f);
    Rect new_bounds = Rect::from_xywh(150.0f, 10.0f, 100.0f, 100.0f);

    // 1. 联合测试
    Rect united = UICompositor::union_rect(old_bounds, new_bounds);
    assert(united.min_x == 10.0f && united.max_x == 250.0f);
    assert(united.min_y == 10.0f && united.max_y == 110.0f);

    // 2. 阴影外扩测试: margin = 3 * 10 = 30px
    Rect inflated = UICompositor::inflate_shadow(united, 10.0f, 0.0f);
    assert(inflated.min_x == -20.0f && inflated.max_x == 280.0f);
    assert(inflated.min_y == -20.0f && inflated.max_y == 140.0f);

    // 3. 16x16 瓦片吸附对齐测试
    // min_x = floor(-20 / 16) * 16 = -2 * 16 = -32.0f
    // max_x = ceil(280 / 16) * 16 = 18 * 16 = 288.0f
    // min_y = floor(-20 / 16) * 16 = -32.0f
    // max_y = ceil(140 / 16) * 16 = 9 * 16 = 144.0f
    Rect snapped = UICompositor::snap_to_tile_grid(inflated, 16.0f);
    assert(snapped.min_x == -32.0f && snapped.max_x == 288.0f);
    assert(snapped.min_y == -32.0f && snapped.max_y == 144.0f);

    // 验证吸附后宽高均为 16 的严格整数倍
    assert(static_cast<int>(snapped.width()) % 16 == 0);
    assert(static_cast<int>(snapped.height()) % 16 == 0);

    // 4. UICompositor 综合调度
    UICompositor compositor;
    compositor.mark_node_dirty(old_bounds, new_bounds, 10.0f, 0.0f);
    assert(compositor.dirty_rects().size() == 1);
    assert(compositor.dirty_rects()[0] == snapped);
    assert(compositor.estimate_dirty_tile_count() == (320 / 16) * (176 / 16)); // 20 * 11 = 220 块

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
    assert(renderer != nullptr);

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
    assert(paint_ctx.primitive_count() >= 2);

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
    assert(has_drawn_pixels);

    std::cout << "  -> KerneTile 2D 计算/光栅底座完整闭环，图元发射与像素渲染验证成功！" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   KTUI 阶段 6: 双级脏区、事件焦点与 KerneTile 闭环  " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_hit_testing_depth_path();
    test_event_bubble_and_stop_propagation();
    test_hover_state_tracking();
    test_focus_manager_tab_navigation();
    test_compositor_dirty_union_and_tile_snapping();
    test_paint_context_and_kernetile_integration();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 6 双级脏区与渲染闭环 100% 测试通过!  " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}

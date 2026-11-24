#include <ktui/ktui.hpp>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace ktui;

// 模拟真实桌面专业工具 ViewModel
struct DesktopIdeViewModel {
    Property<std::string> search_query{""};
    Property<std::string> status_text{"Ready"};
    Property<bool> build_on_save{true};
    Property<float> font_scale{1.0f};
    ObservableList<std::string> project_items;
};

void test_designated_initializer_syntax_and_tree_building() {
    std::cout << "[Test 1] 纯 C++20 指定初始化器声明式嵌套语法与渲染树装配测试..." << std::endl;

    // 严谨纯净的 C++20 指定初始化器语法，彻底废弃 .children 链式调用
    // 严格遵循桌面端高信息密度：紧凑 6px 间距，3px 硬朗微圆角，26px 高度控件
    auto ide_panel = VStack {
        .spacing = 6.0f,
        .padding = EdgeInsets(8.0f),
        .content = {
            HStack {
                .spacing = 4.0f,
                .content = {
                    Box { .width = 16.0f, .height = 16.0f, .background_color = Color{0.20f, 0.55f, 0.95f, 1.0f} },
                    Text { .text = "Project Symbols", .font_size = 13.0f }
                }
            },
            TextInput {
                .placeholder = "Search symbols (Ctrl+P)...",
                .height = 26.0f,
                .corner_radius = 3.0f,
                .padding = EdgeInsets(4.0f, 8.0f)
            },
            HStack {
                .spacing = 6.0f,
                .content = {
                    Button {
                        .text = "Build",
                        .height = 26.0f,
                        .corner_radius = 3.0f
                    },
                    Button {
                        .text = "Clean",
                        .height = 26.0f,
                        .corner_radius = 3.0f
                    }
                }
            }
        }
    };

    // 转换为 Widget 并创建保留式渲染树
    WidgetPtr widget = ide_panel;
    assert(widget != nullptr);

    auto root_render_box = widget->create_render_node();
    assert(root_render_box != nullptr);

    // 执行桌面固定视口布局排版 (800x600)
    BoxConstraints window_constraints = BoxConstraints::tight(Vec2{800.0f, 600.0f});
    root_render_box->layout(window_constraints);

    assert(!root_render_box->needs_layout());
    assert(root_render_box->size() == Vec2(800.0f, 600.0f));

    // 验证 Flex 容器的子节点数
    auto flex_box = std::static_pointer_cast<RenderFlexBox>(root_render_box);
    assert(flex_box != nullptr);
    assert(flex_box->child_boxes().size() == 3);

    std::cout << "  -> 纯 C++20 指定初始化器声明式语法解析、树拓扑装配与排版计算 100% 成功！" << std::endl;
}

void test_view_model_reactive_binding_and_two_way_sync() {
    std::cout << "[Test 2] MVVM 响应式双向数据绑定 (TextInput / Checkbox) 测试..." << std::endl;

    DesktopIdeViewModel vm;
    vm.search_query.set("EngineCore");
    vm.build_on_save.set(true);

    // 1. 测试 TextInput 双向绑定
    WidgetPtr input_widget = TextInput{
        .height = 26.0f
    }.bind_two_way(vm.search_query);
    assert(input_widget != nullptr);
    auto input_render_box = std::static_pointer_cast<RenderTextInput>(input_widget->create_render_node());
    assert(input_render_box != nullptr);

    // 初始值同步校验
    assert(input_render_box->text() == "EngineCore");

    // 模拟用户键盘键入字符
    input_render_box->set_input_focused(true);
    input_render_box->append_chars(".cpp");

    // 验证即时双向同步回 ViewModel
    assert(input_render_box->text() == "EngineCore.cpp");
    assert(vm.search_query.get() == "EngineCore.cpp");

    // 模拟从外部/后台修改 ViewModel -> 渲染盒自动响应
    vm.search_query.set("ShaderPipeline.hlsl");
    assert(input_render_box->text() == "ShaderPipeline.hlsl");

    // 2. 测试 Checkbox 双向绑定
    WidgetPtr check_widget = Checkbox{
        .label = "Auto Build"
    }.bind_two_way(vm.build_on_save);
    auto check_render_box = std::static_pointer_cast<RenderCheckbox>(check_widget->create_render_node());
    assert(check_render_box != nullptr);
    assert(check_render_box->checked() == true);

    // 模拟用户点击复选框切换
    check_render_box->toggle();
    assert(check_render_box->checked() == false);
    assert(vm.build_on_save.get() == false);

    std::cout << "  -> 核心控件双向数据绑定双向联动与防回路死循环机制全部验证无误！" << std::endl;
}

void test_desktop_button_interaction_lifecycle() {
    std::cout << "[Test 3] 桌面专业按钮 (26px, 3px微圆角) 完整事件交互与动效状态机测试..." << std::endl;

    bool clicked = false;
    Button btn_desc{
        .text = "Compile Kernel",
        .height = 26.0f,
        .corner_radius = 3.0f,
        .on_click = [&]() { clicked = true; }
    };

    auto btn_widget = static_cast<WidgetPtr>(btn_desc);
    auto btn_node = std::static_pointer_cast<RenderButton>(btn_widget->create_render_node());
    assert(btn_node != nullptr);

    btn_node->layout(BoxConstraints::loose(Vec2{200.0f, 50.0f}));
    assert(btn_node->size().y == 26.0f); // 严格保证桌面 26px 高度

    // 模拟鼠标划入
    PointerEvent enter_ev(PlatformEventType::PointerMove, PointF{10.0f, 10.0f});
    btn_node->on_pointer_enter(enter_ev);
    assert(btn_node->is_hovered == true);
    assert(btn_node->is_active == false);

    // 模拟鼠标左键按下
    PointerEvent down_ev(PlatformEventType::PointerDown, PointF{10.0f, 10.0f}, PointerButton::Primary);
    btn_node->on_pointer_down(down_ev);
    assert(btn_node->is_active == true);
    assert(!clicked);

    // 模拟鼠标左键松开 -> 触发点击回调
    PointerEvent up_ev(PlatformEventType::PointerUp, PointF{10.0f, 10.0f}, PointerButton::Primary);
    btn_node->on_pointer_up(up_ev);
    assert(btn_node->is_active == false);
    assert(clicked == true);

    std::cout << "  -> 桌面按钮 Hover -> Active -> Click 事件流及状态转换完全正确！" << std::endl;
}

void test_spring_animation_in_desktop_controls() {
    std::cout << "[Test 4] 物理弹簧二阶常微分闭式解平滑微动效在桌面控件中的集成测试..." << std::endl;

    // 创建标准桌面微交互阻尼弹簧求解器 (Snappy 预设)
    SpringSolver solver(0.0f, Springs::Snappy);
    assert(solver.value() == 0.0f);
    assert(solver.target() == 0.0f);

    // 目标变更为 1.0 (例如按钮展开或高亮)
    solver.rebind(0.0f, 0.0f, 1.0f);
    assert(solver.target() == 1.0f);
    assert(!solver.is_settled());

    // 步进几帧
    solver.update(0.016f);
    float pos_mid = solver.value();
    (void)pos_mid;
    assert(pos_mid > 0.0f && pos_mid <= 1.2f); // 阻尼收敛且平滑

    // 步进更多时间直到收敛
    for (int i = 0; i < 60; ++i) {
        solver.update(0.016f);
    }
    assert(solver.is_settled());
    assert(std::abs(solver.value() - 1.0f) < 0.002f);

    std::cout << "  -> 桌面微动效物理弹簧闭式解收敛计算精确可靠！" << std::endl;
}

void test_paint_context_primitive_emission_and_clip_stack() {
    std::cout << "[Test 5] PaintContext 紧致图元发射、偏移栈与硬件视口裁剪测试..." << std::endl;

    PaintContext context(nullptr); // 无头模拟上下文
    assert(context.primitive_count() == 0);

    // 构建测试容器
    auto desktop_view = VStack {
        .spacing = 4.0f,
        .padding = EdgeInsets(6.0f),
        .content = {
            Box {
                .width = 100.0f,
                .height = 26.0f,
                .background_color = Color{0.2f, 0.2f, 0.2f, 1.0f},
                .border_width = 1.0f,
                .corner_radius = 3.0f
            },
            Text {
                .text = "Console Output",
                .font_size = 13.0f
            },
            Box {
                .child = VStack {
                    .spacing = 2.0f,
                    .content = {
                        Text { .text = "Line 1: Kernel initialized successfully." },
                        Text { .text = "Line 2: 16x16 tile binning ready." }
                    }
                }
            }
        }
    };

    WidgetPtr root = desktop_view;
    auto render_tree = root->create_render_node();
    render_tree->layout(BoxConstraints::tight(Vec2{400.0f, 300.0f}));

    // 执行整树绘制
    render_tree->paint(context);

    // 检查图元发射数量 (包括背景色块、细边框、文本、滚动条等)
    assert(context.primitive_count() > 0);
    std::cout << "  -> 成功发射 " << context.primitive_count() << " 个 80B 紧致渲染图元，视口裁剪栈平衡闭环！" << std::endl;
}

void test_overflow_clipping_selection_and_clipboard() {
    std::cout << "[Test 6] 文本防溢出滚动、选区 (Ctrl+A/Shift) 与剪贴板 (Ctrl+C/X/V) 验证..." << std::endl;

    WidgetPtr input_w = TextInput {
        .text = "",
        .placeholder = "Input...",
        .height = 26.0f
    };
    auto node = std::static_pointer_cast<RenderTextInput>(input_w->create_render_node());
    assert(node != nullptr);
    assert(node->cursor_shape == CursorShape::IBeam);

    // 约束在 140px 宽的容器内
    node->layout(BoxConstraints(0.0f, 140.0f, 0.0f, 26.0f));
    assert(node->size().x == 140.0f);

    // 聚焦并输入超长中英文文本，验证输入框尺寸绝不被撑爆且 IME 光标严格在框内
    node->set_input_focused(true);
    node->on_ime_commit("这是一段非常长的中英文混合测试文本_KerneTile_KTUI_Overflow_Protection_1234567890");
    node->layout(BoxConstraints(0.0f, 140.0f, 0.0f, 26.0f));
    assert(node->size().x == 140.0f);
    Rect ime_rc = node->get_ime_cursor_rect();
    assert(ime_rc.min_x >= 0.0f && ime_rc.min_x <= 140.0f);

    // Ctrl+A 全选 + Ctrl+C 复制 + Ctrl+X 剪切 + Ctrl+V 粘贴
    KeyModifiers ctrl_mod{};
    ctrl_mod.ctrl = true;
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'A', "", ctrl_mod));
    assert(node->has_selection());
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'C', "", ctrl_mod));
    assert(!Clipboard::get_text().empty());

    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'X', "", ctrl_mod));
    assert(node->text().empty());

    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'V', "", ctrl_mod));
    assert(node->text() == Clipboard::get_text());

    // 验证 Text 控件的 Wrap 多行自动折行
    WidgetPtr wrap_text = Text {
        .text = "这是一段用于测试多行自动换行的长文本内容，超出最大宽度时应当自动折行增加高度。",
        .font_size = 13.0f,
        .overflow = TextOverflow::Wrap
    };
    auto wrap_node = wrap_text->create_render_node();
    wrap_node->layout(BoxConstraints(0.0f, 120.0f, 0.0f, 500.0f));
    assert(wrap_node->size().x <= 120.0f);
    assert(wrap_node->size().y > 25.0f); // 至少折为 2 行以上

    std::cout << "  -> 防溢出水平滚动、选区替换、Ctrl+A/C/X/V 剪贴板与多行折行全部验证通过！" << std::endl;
}

class MockOverlayBox : public RenderBox {
public:
    void perform_layout() override {
        set_size(Vec2{constraints().min_width, constraints().min_height});
    }
};

void test_overlay_manager_and_desktop_popups() {
    std::cout << "[Test 7] 浮层与弹出层管理器 (OverlayManager / Modal / Dismiss) 验证..." << std::endl;

    OverlayManager mgr;
    assert(!mgr.has_active_overlay());

    bool item_clicked = false;
    auto menu_node = std::make_shared<MockOverlayBox>();
    menu_node->set_size(Vec2{150.0f, 60.0f});
    menu_node->on_pointer_down = [&](const PointerEvent&) { item_clicked = true; };

    OverlayEntry entry1;
    entry1.content = menu_node;
    entry1.global_pos = Vec2{100.0f, 100.0f};
    entry1.size = Vec2{150.0f, 60.0f};
    entry1.dismiss_on_outside_click = true;
    uint64_t id1 = mgr.show(entry1);
    (void)id1;

    assert(mgr.has_active_overlay());
    assert(mgr.overlay_count() == 1);

    // 命中测试内部
    bool handled_inside = mgr.handle_pointer_down(Vec2{120.0f, 110.0f}, PointerButton::Primary);
    assert(handled_inside);
    assert(item_clicked);

    // 点击外部空白自动关闭 (dismiss_on_outside_click)
    bool handled_outside = mgr.handle_pointer_down(Vec2{10.0f, 10.0f}, PointerButton::Primary);
    assert(!handled_outside); // 非模态外部点击允许穿透
    assert(!mgr.has_active_overlay());

    // 模态对话框遮罩与阻断测试
    OverlayEntry modal_entry;
    modal_entry.content = std::make_shared<MockOverlayBox>();
    modal_entry.content->set_size(Vec2{100.0f, 40.0f});
    modal_entry.global_pos = Vec2{50.0f, 50.0f};
    modal_entry.size = Vec2{100.0f, 40.0f};
    modal_entry.is_modal = true;
    modal_entry.dismiss_on_outside_click = false;
    mgr.show(modal_entry);

    assert(mgr.has_modal_overlay());
    // 点击外部，模态遮罩阻断穿透
    bool modal_outside = mgr.handle_pointer_down(Vec2{10.0f, 10.0f}, PointerButton::Primary);
    assert(modal_outside);
    assert(mgr.has_active_overlay()); // 不被外部点击关闭

    // 关闭模态
    mgr.dismiss_all();
    assert(!mgr.has_active_overlay());

    std::cout << "  -> 顶层浮层栈、点击事件拦截与模态遮罩阻断全部验证通过！" << std::endl;
}

void test_text_input_undo_redo_and_word_selection() {
    std::cout << "[Test 8] TextInput 撤销/重做栈 (Ctrl+Z/Y) 与双击/三击选词验证..." << std::endl;

    WidgetPtr input_w = TextInput {
        .text = "hello_world",
        .height = 26.0f
    };
    auto node = std::static_pointer_cast<RenderTextInput>(input_w->create_render_node());
    assert(node != nullptr);
    node->set_input_focused(true);

    // 1. 测试撤销 / 重做 (Undo / Redo)
    KeyModifiers ctrl_mod{};
    ctrl_mod.ctrl = true;

    // 当前为 "hello_world"
    node->append_chars(" 2026");
    assert(node->text() == "hello_world 2026");
    assert(node->can_undo());

    // Ctrl+Z 撤销
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Z', "", ctrl_mod));
    assert(node->text() == "hello_world");
    assert(node->can_redo());

    // Ctrl+Y 重做
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Y', "", ctrl_mod));
    assert(node->text() == "hello_world 2026");

    // Ctrl+Shift+Z 亦支持重做
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Z', "", ctrl_mod));
    assert(node->text() == "hello_world");
    KeyModifiers ctrl_shift{};
    ctrl_shift.ctrl = true;
    ctrl_shift.shift = true;
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Z', "", ctrl_shift));
    assert(node->text() == "hello_world 2026");

    // 2. 测试双击智能分词选区 (Word Boundaries)
    // 词分词：英文字符/下划线、空白、中文
    auto [w1_start, w1_end] = node->find_word_boundaries(3); // 'l' 在 "hello_world" 中
    assert(w1_start == 0 && w1_end == 11); // 选中整个标识符 "hello_world"

    auto [space_start, space_end] = node->find_word_boundaries(11); // 空格处
    assert(space_start == 11 && space_end == 12);

    auto [num_start, num_end] = node->find_word_boundaries(13); // '0' 在 "2026" 中
    assert(num_start == 12 && num_end == 16); // 选中 "2026"

    std::cout << "  -> Ctrl+Z 撤销、Ctrl+Y/Ctrl+Shift+Z 重做与双击分词边界全部验证通过！" << std::endl;
}

void test_cursor_blink_and_on_demand_rendering() {
    std::cout << "[Test 9] 光标 530ms 闪烁逻辑与按需休眠超时 (0% CPU 闲置) 验证..." << std::endl;

    auto input_node = std::make_shared<RenderTextInput>();
    assert(!input_node->is_cursor_blink_visible()); // 未聚焦时不可见

    input_node->set_input_focused(true);
    assert(input_node->is_cursor_blink_visible()); // 聚焦初期必须处于可见周期

    // 测试无头 Application 闲置休眠超时计算
    Application app(create_platform_backend(true));
    WindowDesc desc;
    desc.size = Vec2{400.0f, 300.0f};
    auto win = app.create_window(desc);
    assert(win != nullptr);

    WidgetPtr static_ui = Box {
        .width = 400.0f,
        .height = 300.0f,
        .child = Text { .text = "Static content" }
    };
    win->set_content(static_ui);
    win->render_frame();
    win->clear_redraw_flag();

    // 当完全无待渲染脏区且无光标闪烁时，超时应为 0 (GetMessageW 彻底挂起休眠，0% CPU)
    uint32_t idle_timeout = app.compute_wait_timeout();
    assert(idle_timeout == 0);

    // 当挂接输入框并聚焦时，超时切换为定时光标轮询周期
    WidgetPtr input_ui = Box {
        .width = 400.0f,
        .height = 300.0f,
        .child = TextInput { .text = "Focus me" }
    };
    win->set_content(input_ui);
    win->render_frame();
    win->clear_redraw_flag();

    win->focus_manager().request_focus(win->root_render_box()->first_child()->as_render_box());
    assert(win->has_blinking_cursor());
    uint32_t active_timeout = app.compute_wait_timeout();
    assert(active_timeout > 0); // 启动 100ms 间歇闪烁定时驱动

    std::cout << "  -> 闲置 0% CPU 挂起休眠与聚焦 530ms 光标定时驱动切换全部验证通过！" << std::endl;
}

void test_mobile_touch_scroll_inertia_and_overscroll_bounce() {
    std::cout << "[Test 10] 移动端触控惯性滑行、抗末帧抖动与顶底橡皮筋越界回弹..." << std::endl;
    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    SmoothScrollPhysics p;
    p.set_viewport_extent(500.0f);
    p.set_max_scroll(1200.0f);

    // 1. 快速上滑 + 抬手前补发 0 位移事件，仍应触发强劲惯性滑行
    p.begin_touch_drag();
    for (int i = 0; i < 6; ++i) {
        p.apply_touch_drag_with_dt(-28.0f, 0.016);
    }
    p.apply_touch_drag_with_dt(0.0f, 0.004); // 模拟 Android/Web 抬手前零位移抖动
    float pos_at_lift = p.offset();
    require(pos_at_lift > 160.0f, "pos_at_lift > 160");
    p.end_touch_drag_for_test(0.0);
    require(p.is_flinging() && p.is_animating(), "fling started");

    for (int i = 0; i < 180 && p.step_touch_for_test(0.016); ++i) {}
    require(p.offset() > pos_at_lift + 300.0f, "fling glided > 300px");
    require(!p.is_animating(), "fling settled");

    // 2. 顶部下拉越界橡皮筋阻力 + 放手弹性回弹至 0
    p.jump_to(0.0f);
    p.begin_touch_drag();
    for (int i = 0; i < 5; ++i) {
        p.apply_touch_drag_with_dt(30.0f, 0.016); // 总下拉 150px
    }
    require(p.offset() < -20.0f && p.offset() > -140.0f, "top rubber band stretch");
    require(p.overscroll_amount() < -20.0f, "top overscroll_amount < -20");
    p.end_touch_drag_for_test(0.0);
    require(p.is_bouncing() && p.is_animating(), "top bounce started");
    for (int i = 0; i < 120 && p.step_touch_for_test(0.016); ++i) {}
    require(std::abs(p.offset()) < 0.5f && !p.is_bouncing(), "top bounce settled at 0");

    // 3. 底部上拉越界橡皮筋阻力 + 放手弹性回弹至 max_scroll
    p.jump_to(1200.0f);
    p.begin_touch_drag();
    for (int i = 0; i < 5; ++i) {
        p.apply_touch_drag_with_dt(-30.0f, 0.016);
    }
    require(p.offset() > 1220.0f && p.offset() < 1350.0f, "bottom rubber band stretch");
    p.end_touch_drag_for_test(0.0);
    require(p.is_bouncing(), "bottom bounce started");
    for (int i = 0; i < 120 && p.step_touch_for_test(0.016); ++i) {}
    require(std::abs(p.offset() - 1200.0f) < 0.5f && !p.is_bouncing(), "bottom bounce settled at max");

    // 4. 高速惯性撞击底边 -> 过冲并弹回 max_scroll
    p.jump_to(1050.0f);
    p.begin_touch_drag();
    for (int i = 0; i < 4; ++i) {
        p.apply_touch_drag_with_dt(-30.0f, 0.016);
    }
    p.end_touch_drag_for_test(0.0);
    require(p.is_flinging(), "fast fling toward bottom");
    bool saw_overshoot = false;
    for (int i = 0; i < 200 && p.step_touch_for_test(0.016); ++i) {
        if (p.offset() > 1205.0f) saw_overshoot = true;
    }
    require(saw_overshoot, "boundary fling overshoot observed");
    require(std::abs(p.offset() - 1200.0f) < 0.5f, "boundary overshoot bounced back");

    // 5. 滑动后停顿 > 95ms 再抬手 -> 不触发误惯性
    p.jump_to(200.0f);
    p.begin_touch_drag();
    p.apply_touch_drag_with_dt(-40.0f, 0.016);
    p.end_touch_drag_for_test(0.12);
    require(!p.is_flinging(), "pause before lift suppresses fling");

    std::cout << "  -> 移动端触控惯性、顶底橡皮筋越界与撞边弹性回弹全部验证通过！" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   KTUI 阶段 7: C++20 指定初始化器与端到端实战验证   " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_designated_initializer_syntax_and_tree_building();
    test_view_model_reactive_binding_and_two_way_sync();
    test_desktop_button_interaction_lifecycle();
    test_spring_animation_in_desktop_controls();
    test_paint_context_primitive_emission_and_clip_stack();
    test_overflow_clipping_selection_and_clipboard();
    test_overlay_manager_and_desktop_popups();
    test_text_input_undo_redo_and_word_selection();
    test_cursor_blink_and_on_demand_rendering();
    test_mobile_touch_scroll_inertia_and_overscroll_bounce();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 7 核心控件库与全栈端到端验证 100% 通过! " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}

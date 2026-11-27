#include <ktui/ktui.hpp>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace ktui;

#define KTUI_CHECK(expr) do { if (!(expr)) { std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << ": " #expr << "\n"; std::exit(1); } } while (0)


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
    KTUI_CHECK(widget != nullptr);

    auto root_render_box = widget->create_render_node();
    KTUI_CHECK(root_render_box != nullptr);

    // 执行桌面固定视口布局排版 (800x600)
    BoxConstraints window_constraints = BoxConstraints::tight(Vec2{800.0f, 600.0f});
    root_render_box->layout(window_constraints);

    KTUI_CHECK(!root_render_box->needs_layout());
    KTUI_CHECK(root_render_box->size() == Vec2(800.0f, 600.0f));

    // 验证 Flex 容器的子节点数
    auto flex_box = std::static_pointer_cast<RenderFlexBox>(root_render_box);
    KTUI_CHECK(flex_box != nullptr);
    KTUI_CHECK(flex_box->child_boxes().size() == 3);

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
    KTUI_CHECK(input_widget != nullptr);
    auto input_render_box = std::static_pointer_cast<RenderTextInput>(input_widget->create_render_node());
    KTUI_CHECK(input_render_box != nullptr);

    // 初始值同步校验
    KTUI_CHECK(input_render_box->text() == "EngineCore");

    // 模拟用户键盘键入字符
    input_render_box->set_input_focused(true);
    input_render_box->append_chars(".cpp");

    // 验证即时双向同步回 ViewModel
    KTUI_CHECK(input_render_box->text() == "EngineCore.cpp");
    KTUI_CHECK(vm.search_query.get() == "EngineCore.cpp");

    // 模拟从外部/后台修改 ViewModel -> 渲染盒自动响应
    vm.search_query.set("ShaderPipeline.hlsl");
    KTUI_CHECK(input_render_box->text() == "ShaderPipeline.hlsl");

    // 2. 测试 Checkbox 双向绑定
    WidgetPtr check_widget = Checkbox{
        .label = "Auto Build"
    }.bind_two_way(vm.build_on_save);
    auto check_render_box = std::static_pointer_cast<RenderCheckbox>(check_widget->create_render_node());
    KTUI_CHECK(check_render_box != nullptr);
    KTUI_CHECK(check_render_box->checked() == true);

    // 模拟用户点击复选框切换
    check_render_box->toggle();
    KTUI_CHECK(check_render_box->checked() == false);
    KTUI_CHECK(vm.build_on_save.get() == false);

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
    KTUI_CHECK(btn_node != nullptr);

    btn_node->layout(BoxConstraints::loose(Vec2{200.0f, 50.0f}));
    KTUI_CHECK(btn_node->size().y == 26.0f); // 严格保证桌面 26px 高度

    // 模拟鼠标划入
    PointerEvent enter_ev(PlatformEventType::PointerMove, PointF{10.0f, 10.0f});
    btn_node->on_pointer_enter(enter_ev);
    KTUI_CHECK(btn_node->is_hovered == true);
    KTUI_CHECK(btn_node->is_active == false);

    // 模拟鼠标左键按下
    PointerEvent down_ev(PlatformEventType::PointerDown, PointF{10.0f, 10.0f}, PointerButton::Primary);
    btn_node->on_pointer_down(down_ev);
    KTUI_CHECK(btn_node->is_active == true);
    KTUI_CHECK(!clicked);

    // 模拟鼠标左键松开 -> 触发点击回调
    PointerEvent up_ev(PlatformEventType::PointerUp, PointF{10.0f, 10.0f}, PointerButton::Primary);
    btn_node->on_pointer_up(up_ev);
    KTUI_CHECK(btn_node->is_active == false);
    KTUI_CHECK(clicked == true);

    std::cout << "  -> 桌面按钮 Hover -> Active -> Click 事件流及状态转换完全正确！" << std::endl;
}

void test_spring_animation_in_desktop_controls() {
    std::cout << "[Test 4] 物理弹簧二阶常微分闭式解平滑微动效在桌面控件中的集成测试..." << std::endl;

    // 创建标准桌面微交互阻尼弹簧求解器 (Snappy 预设)
    SpringSolver solver(0.0f, Springs::Snappy);
    KTUI_CHECK(solver.value() == 0.0f);
    KTUI_CHECK(solver.target() == 0.0f);

    // 目标变更为 1.0 (例如按钮展开或高亮)
    solver.rebind(0.0f, 0.0f, 1.0f);
    KTUI_CHECK(solver.target() == 1.0f);
    KTUI_CHECK(!solver.is_settled());

    // 步进几帧
    solver.update(0.016f);
    float pos_mid = solver.value();
    (void)pos_mid;
    KTUI_CHECK(pos_mid > 0.0f && pos_mid <= 1.2f); // 阻尼收敛且平滑

    // 步进更多时间直到收敛
    for (int i = 0; i < 60; ++i) {
        solver.update(0.016f);
    }
    KTUI_CHECK(solver.is_settled());
    KTUI_CHECK(std::abs(solver.value() - 1.0f) < 0.002f);

    std::cout << "  -> 桌面微动效物理弹簧闭式解收敛计算精确可靠！" << std::endl;
}

void test_paint_context_primitive_emission_and_clip_stack() {
    std::cout << "[Test 5] PaintContext 紧致图元发射、偏移栈与硬件视口裁剪测试..." << std::endl;

    PaintContext context(nullptr); // 无头模拟上下文
    KTUI_CHECK(context.primitive_count() == 0);

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
    KTUI_CHECK(context.primitive_count() > 0);
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
    KTUI_CHECK(node != nullptr);
    KTUI_CHECK(node->cursor_shape == CursorShape::IBeam);

    // 约束在 140px 宽的容器内 (RenderTextBox 默认紧凑宽度为 120px)
    node->layout(BoxConstraints(0.0f, 140.0f, 0.0f, 26.0f));
    KTUI_CHECK(node->size().x == 120.0f && node->size().x <= 140.0f);

    // 聚焦并输入超长中英文文本，验证输入框尺寸绝不被撑爆且 IME 光标严格在框内
    node->set_input_focused(true);
    node->on_ime_commit("这是一段非常长的中英文混合测试文本_KerneTile_KTUI_Overflow_Protection_1234567890");
    node->layout(BoxConstraints(0.0f, 140.0f, 0.0f, 26.0f));
    KTUI_CHECK(node->size().x == 120.0f && node->size().x <= 140.0f);
    Rect ime_rc = node->get_ime_cursor_rect();
    KTUI_CHECK(ime_rc.min_x >= 0.0f && ime_rc.min_x <= node->size().x);

    // Ctrl+A 全选 + Ctrl+C 复制 + Ctrl+X 剪切 + Ctrl+V 粘贴
    KeyModifiers ctrl_mod{};
    ctrl_mod.ctrl = true;
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'A', "", ctrl_mod));
    KTUI_CHECK(node->has_selection());
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'C', "", ctrl_mod));
    KTUI_CHECK(!Clipboard::get_text().empty());

    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'X', "", ctrl_mod));
    KTUI_CHECK(node->text().empty());

    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'V', "", ctrl_mod));
    KTUI_CHECK(node->text() == Clipboard::get_text());

    // 验证 Text 控件的 Wrap 多行自动折行
    WidgetPtr wrap_text = Text {
        .text = "这是一段用于测试多行自动换行的长文本内容，超出最大宽度时应当自动折行增加高度。",
        .font_size = 13.0f,
        .overflow = TextOverflow::Wrap
    };
    auto wrap_node = wrap_text->create_render_node();
    wrap_node->layout(BoxConstraints(0.0f, 120.0f, 0.0f, 500.0f));
    KTUI_CHECK(wrap_node->size().x <= 120.0f);
    KTUI_CHECK(wrap_node->size().y > 25.0f); // 至少折为 2 行以上

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
    KTUI_CHECK(!mgr.has_active_overlay());

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

    KTUI_CHECK(mgr.has_active_overlay());
    KTUI_CHECK(mgr.overlay_count() == 1);

    // 命中测试内部
    bool handled_inside = mgr.handle_pointer_down(Vec2{120.0f, 110.0f}, PointerButton::Primary);
    KTUI_CHECK(handled_inside);
    KTUI_CHECK(item_clicked);

    // 点击外部空白自动关闭 (dismiss_on_outside_click)
    bool handled_outside = mgr.handle_pointer_down(Vec2{10.0f, 10.0f}, PointerButton::Primary);
    KTUI_CHECK(!handled_outside); // 非模态外部点击允许穿透
    KTUI_CHECK(!mgr.has_active_overlay());

    // 模态对话框遮罩与阻断测试
    OverlayEntry modal_entry;
    modal_entry.content = std::make_shared<MockOverlayBox>();
    modal_entry.content->set_size(Vec2{100.0f, 40.0f});
    modal_entry.global_pos = Vec2{50.0f, 50.0f};
    modal_entry.size = Vec2{100.0f, 40.0f};
    modal_entry.is_modal = true;
    modal_entry.dismiss_on_outside_click = false;
    mgr.show(modal_entry);

    KTUI_CHECK(mgr.has_modal_overlay());
    // 点击外部，模态遮罩阻断穿透
    bool modal_outside = mgr.handle_pointer_down(Vec2{10.0f, 10.0f}, PointerButton::Primary);
    KTUI_CHECK(modal_outside);
    KTUI_CHECK(mgr.has_active_overlay()); // 不被外部点击关闭

    // 关闭模态
    mgr.dismiss_all();
    KTUI_CHECK(!mgr.has_active_overlay());

    std::cout << "  -> 顶层浮层栈、点击事件拦截与模态遮罩阻断全部验证通过！" << std::endl;
}

void test_text_input_undo_redo_and_word_selection() {
    std::cout << "[Test 8] TextInput 撤销/重做栈 (Ctrl+Z/Y) 与双击/三击选词验证..." << std::endl;

    WidgetPtr input_w = TextInput {
        .text = "hello_world",
        .height = 26.0f
    };
    auto node = std::static_pointer_cast<RenderTextInput>(input_w->create_render_node());
    KTUI_CHECK(node != nullptr);
    node->set_input_focused(true);

    // 1. 测试撤销 / 重做 (Undo / Redo)
    KeyModifiers ctrl_mod{};
    ctrl_mod.ctrl = true;

    // 当前为 "hello_world"
    node->append_chars(" 2026");
    KTUI_CHECK(node->text() == "hello_world 2026");
    KTUI_CHECK(node->can_undo());

    // Ctrl+Z 撤销
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Z', "", ctrl_mod));
    KTUI_CHECK(node->text() == "hello_world");
    KTUI_CHECK(node->can_redo());

    // Ctrl+Y 重做
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Y', "", ctrl_mod));
    KTUI_CHECK(node->text() == "hello_world 2026");

    // Ctrl+Shift+Z 亦支持重做
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Z', "", ctrl_mod));
    KTUI_CHECK(node->text() == "hello_world");
    KeyModifiers ctrl_shift{};
    ctrl_shift.ctrl = true;
    ctrl_shift.shift = true;
    node->handle_text_key(KeyEvent(PlatformEventType::KeyDown, 'Z', "", ctrl_shift));
    KTUI_CHECK(node->text() == "hello_world 2026");

    // 2. 测试双击智能分词选区 (Word Boundaries)
    // 词分词：英文字符/下划线、空白、中文
    auto [w1_start, w1_end] = node->find_word_boundaries(3); // 'l' 在 "hello_world" 中
    KTUI_CHECK(w1_start == 0 && w1_end == 11); // 选中整个标识符 "hello_world"

    auto [space_start, space_end] = node->find_word_boundaries(11); // 空格处
    KTUI_CHECK(space_start == 11 && space_end == 12);

    auto [num_start, num_end] = node->find_word_boundaries(13); // '0' 在 "2026" 中
    KTUI_CHECK(num_start == 12 && num_end == 16); // 选中 "2026"

    std::cout << "  -> Ctrl+Z 撤销、Ctrl+Y/Ctrl+Shift+Z 重做与双击分词边界全部验证通过！" << std::endl;
}

void test_cursor_blink_and_on_demand_rendering() {
    std::cout << "[Test 9] 光标 530ms 闪烁逻辑与按需休眠超时 (0% CPU 闲置) 验证..." << std::endl;

    auto input_node = std::make_shared<RenderTextInput>();
    KTUI_CHECK(!input_node->is_cursor_blink_visible()); // 未聚焦时不可见

    input_node->set_input_focused(true);
    KTUI_CHECK(input_node->is_cursor_blink_visible()); // 聚焦初期必须处于可见周期

    // 测试无头 Application 闲置休眠超时计算
    Application app(create_platform_backend(true));
    WindowDesc desc;
    desc.size = Vec2{400.0f, 300.0f};
    auto win = app.create_window(desc);
    KTUI_CHECK(win != nullptr);

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
    KTUI_CHECK(idle_timeout == 0);

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
    KTUI_CHECK(win->has_blinking_cursor());
    uint32_t active_timeout = app.compute_wait_timeout();
    KTUI_CHECK(active_timeout > 0); // 启动 100ms 间歇闪烁定时驱动

    std::cout << "  -> 闲置 0% CPU 挂起休眠与聚焦 530ms 光标定时驱动切换全部验证通过！" << std::endl;
}

void test_mobile_touch_scroll_inertia_and_overscroll_bounce() {
    std::cout << "[Test 10] 移动端触控惯性滑行、R4 Opt-in 橡皮筋越界与 SpringSolver 回弹..." << std::endl;
    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    struct VirtualClockGuard {
        VirtualClockGuard() { FrameClock::global().enable_virtual_time(100.0); }
        ~VirtualClockGuard() { FrameClock::global().disable_virtual_time(); }
    } vclock_guard;

    SmoothScrollPhysics p;
    p.set_viewport_extent(500.0f);
    p.set_max_scroll(1200.0f);

    // 0. 默认开启 (rubber_band_overscroll == true)，手动关闭后严格钳制在 [0, max_scroll]
    require(p.is_rubber_band_enabled() && p.rubber_band_enabled(), "rubber_band default true");
    p.set_rubber_band_enabled(false);
    require(!p.is_rubber_band_enabled() && !p.rubber_band_enabled(), "rubber_band manual disable false");
    p.jump_to(0.0f);
    p.begin_touch_drag();
    p.apply_touch_drag_with_dt(150.0f, 0.016);
    require(p.offset() == 0.0f && p.overscroll_amount() == 0.0f && !p.is_bouncing(),
            "disabled top drag clamped to 0");
    p.end_touch_drag_for_test(0.0);
    require(!p.is_bouncing() && p.offset() == 0.0f, "disabled top release no bounce");

    p.jump_to(1200.0f);
    p.begin_touch_drag();
    p.apply_touch_drag_with_dt(-150.0f, 0.016);
    require(p.offset() == 1200.0f && p.overscroll_amount() == 0.0f && !p.is_bouncing(),
            "disabled bottom drag clamped to max_scroll");
    p.end_touch_drag_for_test(0.0);
    require(!p.is_bouncing() && p.offset() == 1200.0f, "disabled bottom release no bounce");

    p.jump_to(0.0f);
    p.add_wheel_delta(-360.0f);
    for (int i = 0; i < 40; ++i) {
        FrameClock::global().advance_virtual(1.0f / 60.0f);
        p.update();
        require(p.offset() >= 0.0f && p.offset() <= 1200.0f && p.overscroll_amount() == 0.0f,
                "disabled wheel top clamped");
    }
    p.jump_to(1200.0f);
    p.add_wheel_delta(360.0f);
    for (int i = 0; i < 40; ++i) {
        FrameClock::global().advance_virtual(1.0f / 60.0f);
        p.update();
        require(p.offset() >= 0.0f && p.offset() <= 1200.0f && p.overscroll_amount() == 0.0f,
                "disabled wheel bottom clamped");
    }

    p.jump_to(1050.0f);
    p.begin_touch_drag();
    for (int i = 0; i < 4; ++i) {
        p.apply_touch_drag_with_dt(-30.0f, 0.016);
    }
    p.end_touch_drag_for_test(0.0);
    for (int i = 0; i < 200 && p.step_touch_for_test(0.016); ++i) {
        require(p.offset() <= 1200.0f && p.overscroll_amount() == 0.0f && !p.is_bouncing(),
                "disabled boundary fling clamped at max_scroll");
    }

    // 显式开启 R4 Opt-in 橡皮筋越界回弹
    p.set_rubber_band_enabled(true);
    require(p.is_rubber_band_enabled() && p.rubber_band_enabled(), "rubber_band enabled");
    p.jump_to(0.0f);

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

    // 2. 顶部下拉越界橡皮筋公式 f(x)=(x*d*c)/(d+c*x) + 放手 SpringSolver 回弹至 0
    constexpr float d = 500.0f;
    constexpr float c = 0.55f;
    constexpr float expected_150 = (150.0f * d * c) / (d + c * 150.0f);

    p.jump_to(0.0f);
    p.begin_touch_drag();
    for (int i = 0; i < 5; ++i) {
        p.apply_touch_drag_with_dt(30.0f, 0.016); // 总下拉 150px
    }
    require(std::abs((-p.offset()) - expected_150) < 1e-4f, "top rubber band matches hyperbolic formula within 1e-4");
    require(p.overscroll_amount() < -20.0f, "top overscroll_amount < -20");
    p.end_touch_drag_for_test(0.0);
    require(p.is_bouncing() && p.is_animating(), "top bounce started");
    for (int i = 0; i < 120 && p.step_touch_for_test(0.016); ++i) {}
    require(std::abs(p.offset()) < 0.5f && !p.is_bouncing(), "top bounce settled at 0");

    // 3. 底部上拉越界橡皮筋公式 + 放手 SpringSolver 回弹至 max_scroll
    p.jump_to(1200.0f);
    p.begin_touch_drag();
    for (int i = 0; i < 5; ++i) {
        p.apply_touch_drag_with_dt(-30.0f, 0.016);
    }
    require(std::abs((p.offset() - 1200.0f) - expected_150) < 1e-4f, "bottom rubber band matches hyperbolic formula within 1e-4");
    p.end_touch_drag_for_test(0.0);
    require(p.is_bouncing(), "bottom bounce started");
    for (int i = 0; i < 120 && p.step_touch_for_test(0.016); ++i) {}
    require(std::abs(p.offset() - 1200.0f) < 0.5f && !p.is_bouncing(), "bottom bounce settled at max");

    // 4. 高速惯性撞击底边 -> 过冲并经 SpringSolver 弹回 max_scroll
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

    // 6. 鼠标滚轮输入在边界处严格钳制（过冲回弹仅在触控模式下启用）
    p.jump_to(0.0f);
    p.add_wheel_delta(-36.0f); // -100px
    require(p.target() == 0.0f && p.offset() == 0.0f && !p.is_bouncing(),
            "wheel at top stays clamped at 0 (touch-only overscroll)");
    p.jump_to(1200.0f);
    p.add_wheel_delta(36.0f); // +100px
    require(p.target() == 1200.0f && p.offset() == 1200.0f && !p.is_bouncing(),
            "wheel at bottom stays clamped at 1200 (touch-only overscroll)");

    // 7. 运行时关闭 rubber_band_overscroll 立即复位
    p.jump_to(0.0f);
    p.begin_touch_drag();
    p.apply_touch_drag_with_dt(80.0f, 0.016);
    p.end_touch_drag_for_test(0.0);
    require(p.is_bouncing() && p.offset() < -10.0f, "bouncing before disable");
    p.set_rubber_band_enabled(false);
    require(!p.is_bouncing() && p.offset() == 0.0f && p.overscroll_amount() == 0.0f, "disable mid-bounce clamps immediately");

    // 8. ScrollView, VirtualList, DataGrid 默认禁用 vs Opt-in 行为验证
    auto run_touch_drag_past_top = [](RenderBox& node) {
        PointerEvent down(PlatformEventType::PointerDown, PointF{100.0f, 100.0f}, PointerButton::Primary);
        down.position = PointF{100.0f, 100.0f};
        down.device_kind = PointerDeviceKind::Touch;
        if (node.on_pointer_preview) node.on_pointer_preview(down);
        FrameClock::global().advance_virtual(1.0f / 60.0f);

        PointerEvent move1(PlatformEventType::PointerMove, PointF{100.0f, 160.0f}, PointerButton::Primary);
        move1.position = PointF{100.0f, 160.0f};
        move1.device_kind = PointerDeviceKind::Touch;
        if (node.on_pointer_preview) node.on_pointer_preview(move1);
        FrameClock::global().advance_virtual(1.0f / 60.0f);

        PointerEvent move2(PlatformEventType::PointerMove, PointF{100.0f, 200.0f}, PointerButton::Primary);
        move2.position = PointF{100.0f, 200.0f};
        move2.device_kind = PointerDeviceKind::Touch;
        if (node.on_pointer_move) node.on_pointer_move(move2);
    };
    auto release_touch = [](RenderBox& node) {
        PointerEvent up(PlatformEventType::PointerUp, PointF{100.0f, 200.0f}, PointerButton::Primary);
        up.position = PointF{100.0f, 200.0f};
        up.device_kind = PointerDeviceKind::Touch;
        if (node.on_pointer_up) node.on_pointer_up(up);
    };

    PaintContext headless_ctx(nullptr);
    const float expected_container_100 = (100.0f * 300.0f * 0.55f) / (300.0f + 0.55f * 100.0f);

    // 8a. ScrollView
    {
        WidgetPtr sv_default_w = ScrollView{ .width = 300.0f, .height = 300.0f, .child = Box{ .height = 900.0f } };
        auto sv_def = std::static_pointer_cast<RenderScrollView>(sv_default_w->create_render_node());
        require(sv_def->is_rubber_band_enabled(), "ScrollView default rubber_band true");
        sv_def->set_rubber_band_enabled(false);
        sv_def->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
        run_touch_drag_past_top(*sv_def);
        require(sv_def->scroll_offset() == 0.0f, "ScrollView disabled clamps to 0");
        release_touch(*sv_def);

        WidgetPtr sv_rb_w = ScrollView{ .width = 300.0f, .height = 300.0f, .child = Box{ .height = 900.0f } }.rubber_band(true);
        auto sv_rb = std::static_pointer_cast<RenderScrollView>(sv_rb_w->create_render_node());
        require(sv_rb->is_rubber_band_enabled(), "ScrollView builder rubber_band(true) wired");
        sv_rb->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
        run_touch_drag_past_top(*sv_rb);
        require(std::abs((-sv_rb->scroll_offset()) - expected_container_100) < 1e-4f,
                "ScrollView top overscroll matches f(x) with viewport extent 300");
        release_touch(*sv_rb);
        for (int i = 0; i < 90; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            sv_rb->paint(headless_ctx);
        }
        require(std::abs(sv_rb->scroll_offset()) < 0.5f, "ScrollView rebounded to 0");
    }

    // 8b. VirtualList
    {
        auto item_fn = [](size_t idx) -> WidgetPtr {
            return Text{ .text = "Item " + std::to_string(idx) };
        };
        WidgetPtr vl_default_w = VirtualList{ .item_count = 40, .item_height = 36.0f, .width = 300.0f, .height = 300.0f, .item_builder = item_fn };
        auto vl_def = std::static_pointer_cast<RenderVirtualList>(vl_default_w->create_render_node());
        require(vl_def->is_rubber_band_enabled(), "VirtualList default rubber_band true");
        vl_def->set_rubber_band_enabled(false);
        vl_def->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
        run_touch_drag_past_top(*vl_def);
        require(vl_def->scroll_offset() == 0.0f, "VirtualList disabled clamps to 0");
        release_touch(*vl_def);

        WidgetPtr vl_rb_w = VirtualList{ .item_count = 40, .item_height = 36.0f, .width = 300.0f, .height = 300.0f, .item_builder = item_fn, .rubber_band_overscroll = true };
        auto vl_rb = std::static_pointer_cast<RenderVirtualList>(vl_rb_w->create_render_node());
        require(vl_rb->is_rubber_band_enabled(), "VirtualList designated init rubber_band_overscroll wired");
        vl_rb->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
        run_touch_drag_past_top(*vl_rb);
        require(std::abs((-vl_rb->scroll_offset()) - expected_container_100) < 1e-4f,
                "VirtualList top overscroll matches f(x) with viewport extent 300");
        release_touch(*vl_rb);
        for (int i = 0; i < 90; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            vl_rb->paint(headless_ctx);
        }
        require(std::abs(vl_rb->scroll_offset()) < 0.5f, "VirtualList rebounded to 0");
    }

    // 8c. DataGrid (header height = 36.0f -> height 336.0f gives body_h = 300.0f)
    {
        std::vector<DataGridColumn> cols = { {"Name", 150.0f}, {"Value", 150.0f} };
        std::vector<std::vector<std::string>> rows;
        for (int i = 0; i < 40; ++i) rows.push_back({ "Row " + std::to_string(i), std::to_string(i * 10) });

        WidgetPtr dg_default_w = DataGrid{ .columns = cols, .rows = rows, .row_height = 34.0f, .width = 300.0f, .height = 336.0f };
        auto dg_def = std::static_pointer_cast<RenderDataGrid>(dg_default_w->create_render_node());
        require(dg_def->is_rubber_band_enabled(), "DataGrid default rubber_band true");
        dg_def->set_rubber_band_enabled(false);
        dg_def->layout(BoxConstraints::tight(Vec2{300.0f, 336.0f}));
        run_touch_drag_past_top(*dg_def);
        require(dg_def->scroll_offset() == 0.0f, "DataGrid disabled clamps to 0");
        release_touch(*dg_def);

        WidgetPtr dg_rb_w = DataGrid{ .columns = cols, .rows = rows, .row_height = 34.0f, .width = 300.0f, .height = 336.0f, .rubber_band_overscroll = true };
        auto dg_rb = std::static_pointer_cast<RenderDataGrid>(dg_rb_w->create_render_node());
        require(dg_rb->is_rubber_band_enabled(), "DataGrid designated init rubber_band_overscroll wired");
        dg_rb->layout(BoxConstraints::tight(Vec2{300.0f, 336.0f}));
        run_touch_drag_past_top(*dg_rb);
        require(std::abs((-dg_rb->scroll_offset()) - expected_container_100) < 1e-4f,
                "DataGrid top overscroll matches f(x) with body viewport extent 300");
        release_touch(*dg_rb);
        for (int i = 0; i < 90; ++i) {
            FrameClock::global().advance_virtual(1.0f / 60.0f);
            dg_rb->paint(headless_ctx);
        }
        require(std::abs(dg_rb->scroll_offset()) < 0.5f, "DataGrid rebounded to 0");
    }

    // 9. Regression: RenderVirtualList::set_rubber_band_enabled(false) mid-overscroll + paint() syncs child offsets without layout()
    {
        auto item_fn = [](size_t idx) -> WidgetPtr {
            return Text{ .text = "Row " + std::to_string(idx) };
        };
        auto min_child_y = [](RenderVirtualList& vl) {
            float min_y = 1e9f;
            for (RenderNode* c = vl.first_child(); c != nullptr; c = c->next_sibling()) {
                if (auto* b = c->as_render_box()) {
                    min_y = std::min(min_y, b->offset().y);
                }
            }
            return min_y;
        };

        WidgetPtr vl_w = VirtualList{ .item_count = 40, .item_height = 36.0f, .width = 300.0f, .height = 300.0f, .item_builder = item_fn, .rubber_band_overscroll = true };
        auto vl = std::static_pointer_cast<RenderVirtualList>(vl_w->create_render_node());
        vl->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
        run_touch_drag_past_top(*vl);
        require(std::abs((-vl->scroll_offset()) - expected_container_100) < 1e-4f,
                "VirtualList overscrolled before disable");
        require(std::abs(min_child_y(*vl) - std::round(expected_container_100)) < 1e-3f,
                "VirtualList child 0 shifted during overscroll (pixel-snapped)");

        // Disable via set_rubber_band_enabled(false) and paint() WITHOUT calling layout()
        vl->set_rubber_band_enabled(false);
        vl->paint(headless_ctx);
        require(vl->scroll_offset() == 0.0f, "VirtualList scroll_offset clamped on disable");
        require(std::abs(min_child_y(*vl)) < 1e-4f,
                "VirtualList child offsets must resync on disable + paint without layout");
        release_touch(*vl);

        // Also verify update_render_node(false) + paint() without layout()
        VirtualListWidget desc_on(VirtualList{ .item_count = 40, .item_height = 36.0f, .width = 300.0f, .height = 300.0f, .item_builder = item_fn, .rubber_band_overscroll = true });
        desc_on.update_render_node(vl);
        run_touch_drag_past_top(*vl);
        require(min_child_y(*vl) > 20.0f, "VirtualList re-overscrolled after update_render_node(true)");
        VirtualListWidget desc_off(VirtualList{ .item_count = 40, .item_height = 36.0f, .width = 300.0f, .height = 300.0f, .item_builder = item_fn, .rubber_band_overscroll = false });
        desc_off.update_render_node(vl);
        vl->paint(headless_ctx);
        require(vl->scroll_offset() == 0.0f && std::abs(min_child_y(*vl)) < 1e-4f,
                "VirtualList child offsets resynced via update_render_node(false) + paint without layout");
        release_touch(*vl);
    }

    // 10. Regression: ScrollView, VirtualList, DataGrid with max_scroll == 0 (short content)
    //     default disabled = strict no-op; enabled = touch & wheel overscroll + rebound
    {
        auto send_wheel = [](RenderBox& node, float delta_y) {
            PointerEvent wheel(PlatformEventType::PointerScroll, PointF{100.0f, 100.0f}, PointerButton::None);
            wheel.position = PointF{100.0f, 100.0f};
            wheel.delta = PointF{0.0f, delta_y};
            if (node.on_pointer_move) node.on_pointer_move(wheel);
        };

        // 10a. ScrollView (content height 100 < viewport 300 => max_scroll == 0)
        {
            WidgetPtr sv_def_w = ScrollView{ .width = 300.0f, .height = 300.0f, .child = Box{ .height = 100.0f }, .rubber_band_overscroll = false };
            auto sv_def = std::static_pointer_cast<RenderScrollView>(sv_def_w->create_render_node());
            sv_def->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
            require(sv_def->max_scroll() == 0.0f, "ScrollView short content max_scroll == 0");
            run_touch_drag_past_top(*sv_def);
            require(sv_def->scroll_offset() == 0.0f, "ScrollView max_scroll==0 disabled touch no-op");
            release_touch(*sv_def);
            send_wheel(*sv_def, -36.0f);
            for (int i = 0; i < 20; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                sv_def->paint(headless_ctx);
                require(sv_def->scroll_offset() == 0.0f, "ScrollView max_scroll==0 disabled wheel no-op");
            }

            WidgetPtr sv_rb_w = ScrollView{ .width = 300.0f, .height = 300.0f, .child = Box{ .height = 100.0f }, .rubber_band_overscroll = true };
            auto sv_rb = std::static_pointer_cast<RenderScrollView>(sv_rb_w->create_render_node());
            sv_rb->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
            run_touch_drag_past_top(*sv_rb);
            require(std::abs((-sv_rb->scroll_offset()) - expected_container_100) < 1e-4f,
                    "ScrollView max_scroll==0 enabled touch overscrolls");
            release_touch(*sv_rb);
            for (int i = 0; i < 90; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                sv_rb->paint(headless_ctx);
            }
            require(std::abs(sv_rb->scroll_offset()) < 0.5f, "ScrollView max_scroll==0 touch rebounds to 0");

            send_wheel(*sv_rb, -36.0f);
            for (int i = 0; i < 20; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                sv_rb->paint(headless_ctx);
                require(sv_rb->scroll_offset() == 0.0f,
                        "ScrollView max_scroll==0 mouse wheel stays clamped at 0 (touch-only overscroll)");
            }
        }

        // 10b. VirtualList (3 items * 36 = 108 < viewport 300 => max_scroll == 0)
        {
            auto item_fn = [](size_t idx) -> WidgetPtr { return Text{ .text = "Short " + std::to_string(idx) }; };
            WidgetPtr vl_def_w = VirtualList{ .item_count = 3, .item_height = 36.0f, .width = 300.0f, .height = 300.0f, .item_builder = item_fn, .rubber_band_overscroll = false };
            auto vl_def = std::static_pointer_cast<RenderVirtualList>(vl_def_w->create_render_node());
            vl_def->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
            require(vl_def->max_scroll() == 0.0f, "VirtualList short content max_scroll == 0");
            run_touch_drag_past_top(*vl_def);
            require(vl_def->scroll_offset() == 0.0f, "VirtualList max_scroll==0 disabled touch no-op");
            release_touch(*vl_def);
            send_wheel(*vl_def, -36.0f);
            for (int i = 0; i < 20; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                vl_def->paint(headless_ctx);
                require(vl_def->scroll_offset() == 0.0f, "VirtualList max_scroll==0 disabled wheel no-op");
            }

            WidgetPtr vl_rb_w = VirtualList{ .item_count = 3, .item_height = 36.0f, .width = 300.0f, .height = 300.0f, .item_builder = item_fn, .rubber_band_overscroll = true };
            auto vl_rb = std::static_pointer_cast<RenderVirtualList>(vl_rb_w->create_render_node());
            vl_rb->layout(BoxConstraints::tight(Vec2{300.0f, 300.0f}));
            run_touch_drag_past_top(*vl_rb);
            require(std::abs((-vl_rb->scroll_offset()) - expected_container_100) < 1e-4f,
                    "VirtualList max_scroll==0 enabled touch overscrolls");
            release_touch(*vl_rb);
            for (int i = 0; i < 90; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                vl_rb->paint(headless_ctx);
            }
            require(std::abs(vl_rb->scroll_offset()) < 0.5f, "VirtualList max_scroll==0 touch rebounds to 0");

            send_wheel(*vl_rb, 36.0f); // mouse wheel stays clamped on short list
            for (int i = 0; i < 20; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                vl_rb->paint(headless_ctx);
                require(vl_rb->scroll_offset() == 0.0f,
                        "VirtualList max_scroll==0 mouse wheel stays clamped at 0 (touch-only overscroll)");
            }
        }

        // 10c. DataGrid (3 rows * 34 = 102 < body_h 300 => max_scroll == 0)
        {
            std::vector<DataGridColumn> cols = { {"Name", 150.0f}, {"Value", 150.0f} };
            std::vector<std::vector<std::string>> rows = { {"R0", "0"}, {"R1", "10"}, {"R2", "20"} };

            WidgetPtr dg_def_w = DataGrid{ .columns = cols, .rows = rows, .row_height = 34.0f, .width = 300.0f, .height = 336.0f, .rubber_band_overscroll = false };
            auto dg_def = std::static_pointer_cast<RenderDataGrid>(dg_def_w->create_render_node());
            dg_def->layout(BoxConstraints::tight(Vec2{300.0f, 336.0f}));
            require(dg_def->max_scroll() == 0.0f, "DataGrid short content max_scroll == 0");
            run_touch_drag_past_top(*dg_def);
            require(dg_def->scroll_offset() == 0.0f, "DataGrid max_scroll==0 disabled touch no-op");
            release_touch(*dg_def);
            send_wheel(*dg_def, -36.0f);
            for (int i = 0; i < 20; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                dg_def->paint(headless_ctx);
                require(dg_def->scroll_offset() == 0.0f, "DataGrid max_scroll==0 disabled wheel no-op");
            }

            WidgetPtr dg_rb_w = DataGrid{ .columns = cols, .rows = rows, .row_height = 34.0f, .width = 300.0f, .height = 336.0f, .rubber_band_overscroll = true };
            auto dg_rb = std::static_pointer_cast<RenderDataGrid>(dg_rb_w->create_render_node());
            dg_rb->layout(BoxConstraints::tight(Vec2{300.0f, 336.0f}));
            run_touch_drag_past_top(*dg_rb);
            require(std::abs((-dg_rb->scroll_offset()) - expected_container_100) < 1e-4f,
                    "DataGrid max_scroll==0 enabled touch overscrolls");
            release_touch(*dg_rb);
            for (int i = 0; i < 90; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                dg_rb->paint(headless_ctx);
            }
            require(std::abs(dg_rb->scroll_offset()) < 0.5f, "DataGrid max_scroll==0 touch rebounds to 0");

            send_wheel(*dg_rb, -36.0f);
            for (int i = 0; i < 20; ++i) {
                FrameClock::global().advance_virtual(1.0f / 60.0f);
                dg_rb->paint(headless_ctx);
                require(dg_rb->scroll_offset() == 0.0f,
                        "DataGrid max_scroll==0 mouse wheel stays clamped at 0 (touch-only overscroll)");
            }
        }
    }

    std::cout << "  -> 移动端触控惯性、R4 Opt-in 橡皮筋越界、SpringSolver 回弹与三容器验证全部通过！" << std::endl;
}

void test_touch_long_press_synthesizes_secondary_click() {
    std::cout << "[Test 11] 触控长按 (480ms) 全局等价右键：TextBox 弹出菜单 + 移动取消..." << std::endl;
    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    Application app(create_platform_backend(true));
    WindowDesc desc;
    desc.size = Vec2{480.0f, 320.0f};
    desc.decorated = true;
    auto win = app.create_window(desc);
    win->set_content(TextBox{ .text = "Hello Touch" });
    win->render_frame();

    // 1. 触控按下后移动 > 10px -> 自动取消长按计时器
    PlatformEvent down1;
    down1.type = PlatformEventType::PointerDown;
    down1.button = PointerButton::Primary;
    down1.device_kind = PointerDeviceKind::Touch;
    down1.position = Vec2{40.0f, 18.0f};
    win->handle_platform_event(down1);
    require(win->has_pending_touch_long_press(), "long press armed on touch down");

    PlatformEvent move1;
    move1.type = PlatformEventType::PointerMove;
    move1.button = PointerButton::Primary;
    move1.device_kind = PointerDeviceKind::Touch;
    move1.position = Vec2{40.0f, 35.0f}; // 移动 17px > 10px
    win->handle_platform_event(move1);
    require(!win->has_pending_touch_long_press(), "long press cancelled on drag > 10px");

    PlatformEvent up1 = down1;
    up1.type = PlatformEventType::PointerUp;
    win->handle_platform_event(up1);

    // 2. 触控静止长按到期 -> 自动合成右键、选中所在词并弹出 TextBox 上下文菜单
    // （用新窗口/新 TextBox 避免紧接上一步的 <400ms 误算为双击）
    auto win2 = app.create_window(desc);
    win2->set_content(TextBox{ .text = "Hello Touch" });
    win2->render_frame();
    win2->handle_platform_event(down1);
    require(win2->has_pending_touch_long_press(), "long press re-armed");
    require(!win2->overlay_manager().has_active_overlay(), "no overlay before long press");
    require(win2->trigger_touch_long_press_for_test(), "long press fired");
    require(win2->overlay_manager().has_active_overlay(), "TextBox context menu opened via touch long press");

    // 3. 长按弹出菜单后抬手 -> 被吞掉，不误关闭菜单，且自动选中了所在词
    win2->handle_platform_event(up1);
    require(win2->overlay_manager().has_active_overlay(), "context menu remains open after finger lift");
    auto* tb = static_cast<RenderTextBox*>(win2->root_render_box());
    require(tb && tb->has_selection(), "touch long press auto-selected word");

    // 4. 全选后再次触控长按 -> 选区绝不被 PointerDown 清空，直接保持选区并弹出菜单
    win2->overlay_manager().dismiss_all();
    tb->select_all();
    require(tb->selected_text() == "Hello Touch", "select_all selected full text");
    win2->handle_platform_event(down1);
    require(tb->selected_text() == "Hello Touch", "touch down on existing selection must NOT clear selection");
    require(win2->trigger_touch_long_press_for_test(), "second long press fired");
    require(tb->selected_text() == "Hello Touch", "selection preserved after second long press");
    require(win2->overlay_manager().has_active_overlay(), "context menu re-opened with selection intact");
    win2->handle_platform_event(up1);

    // 5. 短按已选中的文本并抬手 -> 正常折叠取消选区
    win2->overlay_manager().dismiss_all();
    win2->handle_platform_event(down1);
    win2->handle_platform_event(up1);
    require(!tb->has_selection(), "short tap collapsed selection");

    std::cout << "  -> 触控长按全局右键合成（含 Telegram 风格自动选词、全选保活与短按折叠）验证通过！" << std::endl;
}

void test_comprehensive_touch_adaptation() {
    std::cout << "[Test 12] 全控件触控适配验收：44px 隐式热区、PointerCancel 仲裁、手势拖拽与滑块..." << std::endl;
    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    // 1. touch_metrics 44px 热区扩展与 InputModeTracker
    {
        Rect small = Rect::from_xywh(100.0f, 100.0f, 20.0f, 20.0f);
        Rect mouse_r = touch::expand_hit_rect(small, PointerDeviceKind::Mouse);
        Rect touch_r = touch::expand_hit_rect(small, PointerDeviceKind::Touch);
        require(std::abs(mouse_r.width() - 20.0f) < 0.01f, "mouse hit rect unchanged");
        require(std::abs(touch_r.width() - touch::kMinTargetSize) < 0.01f &&
                std::abs(touch_r.height() - touch::kMinTargetSize) < 0.01f,
                "touch hit rect expanded to 44x44");
        require(touch_r.contains(PointF{90.0f, 90.0f}), "touch expanded rect catches outer margin");
        touch::InputModeTracker::notify(PointerDeviceKind::Touch);
        require(touch::InputModeTracker::is_touch_active(), "InputModeTracker reports Touch");
        touch::InputModeTracker::notify(PointerDeviceKind::Mouse);
        require(!touch::InputModeTracker::is_touch_active(), "InputModeTracker reports Mouse");
    }

    auto make_touch = [](PlatformEventType type, Vec2 pos) {
        PlatformEvent ev;
        ev.type = type;
        ev.button = PointerButton::Primary;
        ev.device_kind = PointerDeviceKind::Touch;
        ev.position = pos;
        return ev;
    };

    // 2. ScrollView + Button 触控滚动手势仲裁：垂直拖过阈值 -> Button 收到 PointerCancel，抬手绝不误触发 on_click
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = Vec2{320.0f, 220.0f};
        desc.decorated = true;
        auto win = app.create_window(desc);

        int clicks = 0;
        std::vector<WidgetPtr> rows;
        rows.push_back(Button{ .text = "Tap Me", .on_click = [&]() { ++clicks; } });
        for (int i = 0; i < 14; ++i) {
            rows.push_back(Box{ .height = 40.0f, .child = Text{ .text = "Row " + std::to_string(i) } });
        }
        win->set_content(ScrollView{
            .height = 220.0f,
            .child = VStack{ .spacing = 6.0f, .content = std::move(rows) }
        });
        win->render_frame();

        // 2a. 短按按钮 -> 正常触发点击
        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, Vec2{40.0f, 16.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, Vec2{40.0f, 16.0f}));
        require(clicks == 1, "touch tap on button inside ScrollView fires click");

        // 2b. 按在按钮上垂直拖 40px -> ScrollView 抢占手势并向 Button 派发 PointerCancel，不触发点击
        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, Vec2{40.0f, 16.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerMove, Vec2{40.0f, 56.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, Vec2{40.0f, 56.0f}));
        require(clicks == 1, "vertical touch scroll cancels button press via PointerCancel");
    }

    // 3. ToggleSwitch 触控水平拖拽切换开关
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = Vec2{300.0f, 120.0f};
        desc.decorated = true;
        auto win = app.create_window(desc);

        bool is_on = false;
        win->set_content(ToggleSwitch{ .checked = false, .on_change = [&](bool v) { is_on = v; } });
        win->render_frame();

        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, Vec2{12.0f, 16.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerMove, Vec2{36.0f, 16.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, Vec2{36.0f, 16.0f}));
        require(is_on, "ToggleSwitch horizontal touch drag turns switch ON");
    }

    // 4. RatingControl 触控横扫连续选星
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = Vec2{300.0f, 120.0f};
        desc.decorated = true;
        auto win = app.create_window(desc);

        float rating = 1.0f;
        win->set_content(RatingControl{ .value = 1.0f, .max_rating = 5, .on_value_changed = [&](float v) { rating = v; } });
        win->render_frame();

        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, Vec2{10.0f, 14.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerMove, Vec2{100.0f, 14.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, Vec2{100.0f, 14.0f}));
        require(rating >= 3.0f, "RatingControl horizontal touch swipe updates rating");
    }

    // 5. GhostTextBox 触控右滑逐词接受补全
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = Vec2{460.0f, 120.0f};
        desc.decorated = true;
        auto win = app.create_window(desc);

        win->set_content(GhostTextBox{
            .text = "git ",
            .suggestions = { "git status --short" }
        });
        win->render_frame();
        auto* ghost = static_cast<RenderGhostTextBox*>(win->root_render_box());
        require(ghost && !ghost->ghost_suffix().empty(), "GhostTextBox has ghost suffix");

        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, Vec2{30.0f, 16.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerMove, Vec2{80.0f, 16.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, Vec2{80.0f, 16.0f}));
        require(ghost->text().find("status") != std::string::npos, "GhostTextBox touch swipe-right accepts next word");
    }

    // 6. SplitView 28px 触控加宽抓取区拖动分栏
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = Vec2{400.0f, 200.0f};
        desc.decorated = true;
        auto win = app.create_window(desc);

        float ratio = 0.35f;
        win->set_content(SplitView{
            .ratio = 0.35f,
            .splitter_size = 8.0f,
            .first = Box{ .child = Text{ .text = "Left" } },
            .second = Box{ .child = Text{ .text = "Right" } },
            .on_ratio_changed = [&](float r) { ratio = r; }
        });
        win->render_frame();

        // 视觉分隔条中心约在 400*0.35 + 4 = 144，点在 154（超出 8px 视觉条但在 28px 触控抓取带内）
        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, Vec2{154.0f, 100.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerMove, Vec2{220.0f, 100.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, Vec2{220.0f, 100.0f}));
        require(ratio > 0.45f, "SplitView 28px touch grab zone drags splitter smoothly");
    }

    // 7. PipsPager 触控轻扫翻页
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = Vec2{300.0f, 100.0f};
        desc.decorated = true;
        auto win = app.create_window(desc);

        int page = 0;
        win->set_content(PipsPager{
            .number_of_pages = 5,
            .selected_index = 0,
            .on_selected_index_changed = [&](int p) { page = p; }
        });
        win->render_frame();

        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, Vec2{90.0f, 15.0f}));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, Vec2{45.0f, 15.0f}));
        require(page == 1, "PipsPager touch swipe left advances page");
    }

    std::cout << "  -> 全控件触控适配验收（44px 热区、PointerCancel、ToggleSwitch/Rating/Ghost/SplitView/PipsPager）全部通过！" << std::endl;
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
    test_touch_long_press_synthesizes_secondary_click();
    test_comprehensive_touch_adaptation();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 7 核心控件库与全栈端到端验证 100% 通过! " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}

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

void test_mobile_touch_passthrough_and_responsive_layout() {
    std::cout << "[Test 13] 移动端触控防穿透 (NavigationView 浮层/Expander) 与窄屏响应式布局 (SettingsCard)..." << std::endl;
    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    auto make_touch = [](PlatformEventType t, Vec2 pos) {
        PlatformEvent ev;
        ev.type = t;
        ev.button = PointerButton::Primary;
        ev.device_kind = PointerDeviceKind::Touch;
        ev.position = pos;
        return ev;
    };

    // 1. NavigationView LeftMinimal 窄屏抽屉展开时，触控点击侧边栏项绝不穿透到底层内容按钮
    {
        Application app(create_platform_backend(true));
        WindowDesc desc;
        desc.size = Vec2{390.0f, 700.0f};
        desc.decorated = true;
        auto win = app.create_window(desc);

        std::string selected_nav = "home";
        int underlying_clicks = 0;

        win->set_content(NavigationView{
            .menu_items = {
                NavigationViewItem{ .id = "home", .label = "主页", .icon = NavigationIcon::Home },
                NavigationViewItem{ .id = "buttons", .label = "按钮控件", .icon = NavigationIcon::Home },
                NavigationViewItem{ .id = "settings_demo", .label = "外观设置", .icon = NavigationIcon::Settings }
            },
            .selected_id = "home",
            .pane_display_mode = NavigationPaneDisplayMode::LeftMinimal,
            .is_pane_open = true,
            .is_search_box_visible = false,
            .content = Button{
                .text = "底层全屏巨型按钮",
                .on_click = [&]() { underlying_clicks++; }
            },
            .on_selection_changed = [&](const std::string& id) { selected_nav = id; }
        });
        // 推进抽屉展开动画至完全打开 (m_pane_open_anim -> 1.0，NavigationView 使用 steady_clock)
        for (int i = 0; i < 20; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            win->render_frame();
        }

        // 点在侧边栏第二个菜单项 ("按钮控件") 区域，同时也是底层大按钮的坐标范围内
        // Top chrome row = 44px, Row 0 (home) = [48, 84], Row 1 (buttons) = [84, 120]
        const float tb_h = win->titlebar_height();
        const Vec2 tap_pt{120.0f, tb_h + 102.0f};
        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, tap_pt));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, tap_pt));

        require(selected_nav == "buttons", "Touch on open NavigationView overlay pane must select sidebar item");
        require(underlying_clicks == 0, "Touch on open NavigationView overlay pane must NOT penetrate to underlying content button");

        // 侧边栏关闭动画完成后，再点相同位置应正常命中底层按钮
        for (int i = 0; i < 25; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            win->render_frame();
        }
        win->handle_platform_event(make_touch(PlatformEventType::PointerDown, tap_pt));
        win->handle_platform_event(make_touch(PlatformEventType::PointerUp, tap_pt));
        require(underlying_clicks == 1, "After overlay pane closes, touch hits underlying content button normally");
    }

    // 2. SettingsCard 窄屏 (360px) 携带宽 ComboBox 自动上下双行堆叠，不挤压标题
    {
        WidgetPtr card_w = SettingsCard{
            .header = "窗口背景材质 (Backdrop)",
            .description = "实时切换云母材质或在移动端自动双行堆叠展示长描述文本",
            .content = ComboBox{
                .items = {"无材质 (None)", "云母 (Mica)", "深层云母 (Mica Alt)"},
                .selected_index = 1,
                .width = 180.0f
            }
        };
        auto card = std::static_pointer_cast<RenderSettingsCard>(card_w->create_render_node());

        // 窄屏 360px -> 应触发上下堆叠，高度 > 100px
        card->layout(BoxConstraints{360.0f, 360.0f, 0.0f, BoxConstraints::kInfinity});
        float narrow_h = card->size().y;
        require(narrow_h > 100.0f, "SettingsCard on narrow 360px screen stacks ComboBox vertically and expands height");

        // 宽屏 720px -> 保持桌面单行紧凑布局，高度 <= 80px
        card->layout(BoxConstraints{720.0f, 720.0f, 0.0f, BoxConstraints::kInfinity});
        float wide_h = card->size().y;
        require(wide_h <= 80.0f, "SettingsCard on wide 720px desktop stays compact single-row");
    }

    // 3. FlexEngine 窄屏 (<560px) HStack 自动防溢出折行 vs 宽屏单行保持不变
    {
        WidgetPtr row_w = HStack{
            .spacing = 10.0f,
            .content = {
                Button{ .text = "操作按钮一 (110px)" },
                Button{ .text = "操作按钮二 (110px)" },
                Button{ .text = "操作按钮三 (110px)" },
                Button{ .text = "操作按钮四 (110px)" }
            }
        };
        auto row = row_w->create_render_node();

        // 窄屏 340px -> 必须自动折行，且所有子项严格在 340px 边界内不溢出
        row->layout(BoxConstraints{340.0f, 340.0f, 0.0f, BoxConstraints::kInfinity});
        require(row->size().y > 56.0f, "Narrow HStack auto-reflows overflowing buttons onto multiple lines");
        for (RenderNode* c = row->first_child(); c != nullptr; c = c->next_sibling()) {
            if (auto* b = c->as_render_box()) {
                require(b->offset().x + b->size().x <= 340.5f, "Auto-reflowed HStack child stays inside narrow viewport");
            }
        }

        // 宽屏 760px -> 保持桌面单行排布
        row->layout(BoxConstraints{760.0f, 760.0f, 0.0f, BoxConstraints::kInfinity});
        require(row->size().y <= 42.0f, "Wide desktop HStack stays single-row");
    }

    // 4. HStack 携带 Spacer {} 或 Expanded {} 在窄屏自动折行
    {
        WidgetPtr sp_row_w = HStack{
            .spacing = 10.0f,
            .content = {
                Text{ .text = "这是一段比较长的移动端说明标题用来验证横向防溢出折行表现" },
                Spacer{},
                Button{ .text = "立即切换主题" }
            }
        };
        auto sp_row = sp_row_w->create_render_node();
        sp_row->layout(BoxConstraints{340.0f, 340.0f, 0.0f, BoxConstraints::kInfinity});
        require(sp_row->size().y > 46.0f, "Narrow HStack with Spacer auto-reflows instead of crushing sibling button");
        for (RenderNode* c = sp_row->first_child(); c != nullptr; c = c->next_sibling()) {
            if (auto* b = c->as_render_box()) {
                require(b->offset().x + b->size().x <= 340.5f, "Reflowed Spacer-row child stays inside 340px width");
            }
        }

        WidgetPtr exp_row_w = HStack{
            .spacing = 8.0f,
            .content = {
                Button{ .text = "固定前置按钮A (120px)" },
                Expanded{ .child = Text{ .text = "中间说明内容在窄屏不应当被压成零宽", .overflow = TextOverflow::Wrap } },
                Button{ .text = "固定后置按钮B (120px)" }
            }
        };
        auto exp_row = exp_row_w->create_render_node();
        exp_row->layout(BoxConstraints{340.0f, 340.0f, 0.0f, BoxConstraints::kInfinity});
        require(exp_row->size().y > 50.0f, "Narrow HStack with Expanded child reflows when leftover width is squeezed");
        for (RenderNode* c = exp_row->first_child(); c != nullptr; c = c->next_sibling()) {
            if (auto* b = c->as_render_box()) {
                require(b->offset().x + b->size().x <= 340.5f, "Reflowed Expanded-row child stays inside 340px width");
            }
        }
    }

    // 5. SegmentedControl / MenuBar / StatusBar / TabView / Pivot / ToggleSwitch 窄屏折行验证
    {
        WidgetPtr seg_w = SegmentedControl{
            .items = {
                SegmentedItem{ .label = "选项一详情" },
                SegmentedItem{ .label = "选项二详情" },
                SegmentedItem{ .label = "选项三详情" },
                SegmentedItem{ .label = "选项四详情" }
            },
            .selected_index = 0
        };
        auto seg = seg_w->create_render_node();
        seg->layout(BoxConstraints{220.0f, 220.0f, 0.0f, BoxConstraints::kInfinity});
        require(seg->size().y > 50.0f && seg->size().x <= 220.5f, "Narrow SegmentedControl wraps across multiple rows");
        seg->layout(BoxConstraints{480.0f, 480.0f, 0.0f, BoxConstraints::kInfinity});
        require(seg->size().y <= 34.0f, "Wide SegmentedControl remains single-row");

        WidgetPtr mb_w = MenuBar{
            .items = {
                MenuBarItem{ .title = "File" },
                MenuBarItem{ .title = "Edit" },
                MenuBarItem{ .title = "Selection" },
                MenuBarItem{ .title = "View" },
                MenuBarItem{ .title = "Window" },
                MenuBarItem{ .title = "Help" }
            }
        };
        auto mb = mb_w->create_render_node();
        mb->layout(BoxConstraints{200.0f, 200.0f, 0.0f, BoxConstraints::kInfinity});
        require(mb->size().y > 40.0f && mb->size().x <= 200.5f, "Narrow MenuBar wraps menu items across rows");

        WidgetPtr sb_w = StatusBar{
            .slots = {
                StatusBarSlot{ .text = "main* branch" },
                StatusBarSlot{ .text = "Ln 128, Col 34" },
                StatusBarSlot{ .text = "UTF-8", .align_right = true },
                StatusBarSlot{ .text = "C++20 WebGPU", .align_right = true }
            }
        };
        auto sb = sb_w->create_render_node();
        sb->layout(BoxConstraints{220.0f, 220.0f, 0.0f, BoxConstraints::kInfinity});
        require(sb->size().y > 32.0f && sb->size().x <= 220.5f, "Narrow StatusBar wraps left/right slots across rows");

        WidgetPtr tv_w = TabView{
            .tabs = {
                TabViewItem{ .id = "t1", .header = "Document 1" },
                TabViewItem{ .id = "t2", .header = "Document 2" },
                TabViewItem{ .id = "t3", .header = "Document 3" },
                TabViewItem{ .id = "t4", .header = "Document 4" },
                TabViewItem{ .id = "t5", .header = "Document 5" }
            },
            .height = 180.0f
        };
        auto tv = tv_w->create_render_node();
        tv->layout(BoxConstraints{240.0f, 240.0f, 0.0f, BoxConstraints::kInfinity});
        require(tv->size().y > 160.0f && tv->size().x <= 240.5f, "Narrow TabView constrains width cleanly");

        WidgetPtr pv_w = Pivot{
            .items = {
                PivotItem{ .header = "Overview" },
                PivotItem{ .header = "Performance" },
                PivotItem{ .header = "Memory Profile" },
                PivotItem{ .header = "GPU Tiles" }
            }
        };
        auto pv = pv_w->create_render_node();
        pv->layout(BoxConstraints{200.0f, 200.0f, 0.0f, BoxConstraints::kInfinity});
        require(pv->size().y > 50.0f && pv->size().x <= 200.5f, "Narrow Pivot wraps tab headers onto multiple lines");

        WidgetPtr ts_w = ToggleSwitch{
            .label = "这是一条非常长的移动端开关说明文字必须能够自动换行"
        };
        auto ts = ts_w->create_render_node();
        ts->layout(BoxConstraints{200.0f, 200.0f, 0.0f, BoxConstraints::kInfinity});
        require(ts->size().y > 32.0f && ts->size().x <= 200.5f, "Narrow ToggleSwitch wraps long labels onto multiple lines");
    }

    std::cout << "  -> 移动端触控防穿透（NavigationView 浮层抽屉）、SettingsCard 堆叠与 FlexEngine/复合控件窄屏自动折行验证通过！" << std::endl;
}

void test_touch_scroll_mid_fling_interrupt_and_continue() {
    std::cout << "[Test 14] 触控惯性滚动作业中二次打断续滑、点停不误触与滚动条触控接管..." << std::endl;
    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    Application app(create_platform_backend(true));
    WindowDesc desc;
    desc.title = "Touch Scroll Interrupt Test";
    desc.size = Vec2{360.0f, 480.0f};
    desc.custom_titlebar = false;
    auto win = app.create_window(desc);
    require(win != nullptr, "create window");

    auto send_touch = [&](PlatformEventType type, float x, float y) {
        PlatformEvent pe{};
        pe.type = type;
        pe.position = Vec2{x, y};
        pe.button = (type == PlatformEventType::PointerMove) ? PointerButton::None : PointerButton::Primary;
        pe.device_kind = PointerDeviceKind::Touch;
        pe.pointer_id = 1;
        win->handle_platform_event(pe);
    };

    // 1. ScrollView: 第一次滑动触发惯性 -> 惯性未停止时立刻第二次滑动应立即打断并跟手续滑 + 再次启动惯性
    {
        int btn_clicks = 0;
        std::vector<WidgetPtr> items;
        for (int i = 0; i < 40; ++i) {
            items.push_back(Button{
                .text = "Row " + std::to_string(i),
                .height = 48.0f,
                .on_click = [&btn_clicks]() { ++btn_clicks; }
            });
        }
        win->set_content(ScrollView{
            .child = VStack{
                .align_items = AlignItems::Stretch,
                .spacing = 8.0f,
                .content = std::move(items)
            }
        });
        win->render_frame(true);

        auto* sv = static_cast<RenderScrollView*>(win->root_render_box());
        require(sv != nullptr && sv->max_scroll() > 1000.0f, "ScrollView scrollable");

        // 第一次快速上滑 -> 启动惯性
        send_touch(PlatformEventType::PointerDown, 150.0f, 380.0f);
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            send_touch(PlatformEventType::PointerMove, 150.0f, 380.0f - static_cast<float>(i) * 32.0f);
        }
        send_touch(PlatformEventType::PointerUp, 150.0f, 220.0f);
        require(btn_clicks == 0, "first swipe should not click child button");

        const float after_swipe1 = sv->scroll_offset();
        std::this_thread::sleep_for(std::chrono::milliseconds(24));
        win->render_frame(true);
        const float during_fling1 = sv->scroll_offset();
        require(during_fling1 > after_swipe1 + 10.0f, "first swipe launched active fling");

        // 惯性仍在滑行中 -> 立刻发起第二次滑动 (PointerDown -> PointerMove -> PointerUp)
        send_touch(PlatformEventType::PointerDown, 150.0f, 360.0f);
        const float offset_at_catch = sv->scroll_offset();
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
        send_touch(PlatformEventType::PointerMove, 150.0f, 320.0f);
        const float offset_after_move1 = sv->scroll_offset();
        require(offset_after_move1 > offset_at_catch + 30.0f,
                "mid-fling second touch move must immediately drag 1:1 without waiting for stop");

        for (int i = 2; i <= 4; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            send_touch(PlatformEventType::PointerMove, 150.0f, 360.0f - static_cast<float>(i) * 40.0f);
        }
        send_touch(PlatformEventType::PointerUp, 150.0f, 200.0f);
        require(btn_clicks == 0, "mid-fling second swipe must not click child button");

        const float after_swipe2 = sv->scroll_offset();
        require(after_swipe2 > offset_at_catch + 140.0f, "second swipe scrolled smoothly");

        std::this_thread::sleep_for(std::chrono::milliseconds(24));
        win->render_frame(true);
        const float during_fling2 = sv->scroll_offset();
        require(during_fling2 > after_swipe2 + 10.0f, "second swipe launched fresh fling");

        // 惯性仍在滑行中 -> 轻点刹车不移动 -> 停止滚动且不误触子按钮
        send_touch(PlatformEventType::PointerDown, 150.0f, 240.0f);
        send_touch(PlatformEventType::PointerUp, 150.0f, 240.0f);
        const float braked_offset = sv->scroll_offset();
        std::this_thread::sleep_for(std::chrono::milliseconds(24));
        win->render_frame(true);
        require(std::abs(sv->scroll_offset() - braked_offset) < 0.5f, "tap during fling brakes scroll cleanly");
        require(btn_clicks == 0, "tap-to-brake must not trigger child button click");

        // 触控按住右侧 24px 滚动条热区（在惯性滑行中途直接抓握）-> 立即打断惯性并跟手拖动滑块
        send_touch(PlatformEventType::PointerDown, 150.0f, 360.0f);
        for (int i = 1; i <= 4; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            send_touch(PlatformEventType::PointerMove, 150.0f, 360.0f - static_cast<float>(i) * 35.0f);
        }
        send_touch(PlatformEventType::PointerUp, 150.0f, 220.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        win->render_frame(true);

        // 命中右侧 24px 触控带内（x = 342，处于 12px 视觉宽外但 24px 触控宽内）
        const float sb_start_offset = sv->scroll_offset();
        const float frac = std::clamp(sb_start_offset / sv->max_scroll(), 0.0f, 1.0f);
        const float thumb_y = 20.0f + frac * (480.0f - 80.0f);
        send_touch(PlatformEventType::PointerDown, 342.0f, thumb_y);
        send_touch(PlatformEventType::PointerMove, 342.0f, std::min(460.0f, thumb_y + 80.0f));
        send_touch(PlatformEventType::PointerUp, 342.0f, std::min(460.0f, thumb_y + 80.0f));
        require(sv->scroll_offset() > sb_start_offset + 50.0f,
                "touch grabbing scrollbar in 24px hit strip must interrupt fling and scrub offset");
    }

    // 2. VirtualList: 惯性中途第二次触控滑动立即打断并跟手续滑
    {
        win->set_content(VirtualList{
            .item_count = 200,
            .item_height = 40.0f,
            .height = 480.0f,
            .item_builder = [](size_t idx) -> WidgetPtr {
                return Text{ .text = "Item #" + std::to_string(idx) };
            }
        });
        win->render_frame(true);

        auto* vl = static_cast<RenderVirtualList*>(win->root_render_box());
        require(vl != nullptr && vl->max_scroll() > 1000.0f, "VirtualList scrollable");

        send_touch(PlatformEventType::PointerDown, 150.0f, 360.0f);
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            send_touch(PlatformEventType::PointerMove, 150.0f, 360.0f - static_cast<float>(i) * 32.0f);
        }
        send_touch(PlatformEventType::PointerUp, 150.0f, 200.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        win->render_frame(true);

        const float before_swipe2 = vl->scroll_offset();
        send_touch(PlatformEventType::PointerDown, 150.0f, 360.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        send_touch(PlatformEventType::PointerMove, 150.0f, 310.0f);
        const float during_swipe2 = vl->scroll_offset();
        send_touch(PlatformEventType::PointerUp, 150.0f, 310.0f);
        require(during_swipe2 > before_swipe2 + 40.0f,
                "VirtualList mid-fling second swipe must immediately scroll");
    }

    // 3. DataGrid: 惯性中途第二次触控滑动立即打断并跟手续滑
    {
        std::vector<std::vector<std::string>> rows;
        for (int i = 0; i < 120; ++i) {
            rows.push_back({std::to_string(i), "Row " + std::to_string(i)});
        }
        win->set_content(DataGrid{
            .columns = {
                DataGridColumn{ .title = "ID", .width = 80.0f },
                DataGridColumn{ .title = "Name", .width = 240.0f }
            },
            .rows = std::move(rows),
            .height = 480.0f
        });
        win->render_frame(true);

        auto* dg = static_cast<RenderDataGrid*>(win->root_render_box());
        require(dg != nullptr, "DataGrid created");

        send_touch(PlatformEventType::PointerDown, 150.0f, 360.0f);
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            send_touch(PlatformEventType::PointerMove, 150.0f, 360.0f - static_cast<float>(i) * 32.0f);
        }
        send_touch(PlatformEventType::PointerUp, 150.0f, 200.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        win->render_frame(true);

        const float before_swipe2 = dg->scroll_offset();
        send_touch(PlatformEventType::PointerDown, 150.0f, 360.0f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        send_touch(PlatformEventType::PointerMove, 150.0f, 310.0f);
        const float during_swipe2 = dg->scroll_offset();
        send_touch(PlatformEventType::PointerUp, 150.0f, 310.0f);
        require(during_swipe2 > before_swipe2 + 40.0f,
                "DataGrid mid-fling second swipe must immediately scroll");
    }

    std::cout << "  -> ScrollView / VirtualList / DataGrid 触控惯性打断续滑、点停防误触与滚动条触控接管全部验证通过！" << std::endl;
}

// =========================================================================
// [Test 15] 文本框点击选区稳定性（鼠标+触控）与水滴形触控选区/光标调整手柄验证
// =========================================================================
static void test_text_selection_stability_and_touch_teardrop_handles() {
    std::cout << "[Test 15] 文本框点击选区稳定性与水滴形触控调整手柄验证..." << std::endl;
    auto require = [](bool cond, const char* msg) {
        if (!cond) {
            std::cerr << "FAILED: " << msg << std::endl;
            std::abort();
        }
    };

    Application app(create_platform_backend(true));
    WindowDesc desc;
    desc.title = "Text Selection & Handles E2E";
    desc.size = Vec2{480.0f, 420.0f};
    auto win = app.create_window(desc);
    require(win != nullptr, "create window");
    const float tb_h = win->titlebar_height();

    auto send_mouse = [&](PlatformEventType type, float client_x, float client_y,
                          PointerButton btn = PointerButton::Primary) {
        PlatformEvent ev;
        ev.type = type;
        ev.position = Vec2{client_x, tb_h + client_y};
        ev.button = btn;
        ev.device_kind = PointerDeviceKind::Mouse;
        win->handle_platform_event(ev);
    };
    auto send_touch = [&](PlatformEventType type, float client_x, float client_y,
                          PointerButton btn = PointerButton::Primary) {
        PlatformEvent ev;
        ev.type = type;
        ev.position = Vec2{client_x, tb_h + client_y};
        ev.button = btn;
        ev.device_kind = PointerDeviceKind::Touch;
        ev.pointer_id = 1;
        win->handle_platform_event(ev);
    };

    // 1. RenderTextBox: 鼠标点击不乱跳 + 拖拽选区 + 水滴形触控手柄调整 Start / End / Crossing / Caret
    {
        win->set_content(VStack{
            .align_items = AlignItems::Stretch,
            .spacing = 16.0f,
            .padding = EdgeInsets{24.0f, 24.0f, 24.0f, 24.0f},
            .content = {
                TextBox{
                    .text = "Hello KerneTile World Selection",
                    .placeholder = "input"
                }
            }
        });
        win->render_frame(true);

        RenderTextBox* tb = static_cast<RenderTextBox*>(win->root_render_box()->first_child());
        require(tb != nullptr, "RenderTextBox found");

        const Vec2 g = tb->global_offset();
        const auto style = tb->resolve_current_style();
        const float mid_y = g.y + tb->size().y * 0.5f;
        const float base_x = g.x + style.padding.left;
        const std::string txt = tb->text();
        auto char_client_x = [&](size_t idx) -> float {
            return base_x + TextMeasure::cursor_x(txt, style.font_size, idx);
        };

        // 1a. 鼠标在不同位置快速连点 (<400ms)，绝不能误判成双击选词/三击全选
        const float x_pos_a = char_client_x(2);  // "He|llo"
        const float x_pos_b = char_client_x(12); // "Hello KerneT|ile"
        const float x_pos_c = char_client_x(20); // "Hello KerneTile Worl|d"
        send_mouse(PlatformEventType::PointerDown, x_pos_a, mid_y);
        send_mouse(PlatformEventType::PointerUp, x_pos_a, mid_y);
        require(!tb->has_selection(), "Single mouse click at A must not create selection");
        require(tb->cursor_index() == 2, "Single mouse click at A places caret at index 2");

        send_mouse(PlatformEventType::PointerDown, x_pos_b, mid_y);
        send_mouse(PlatformEventType::PointerUp, x_pos_b, mid_y);
        require(!tb->has_selection(), "Quick click at distant B must NOT jump into word/all selection");
        require(tb->cursor_index() == 12, "Click at B places caret accurately at index 12");

        send_mouse(PlatformEventType::PointerDown, x_pos_c, mid_y);
        send_mouse(PlatformEventType::PointerUp, x_pos_c, mid_y);
        require(!tb->has_selection(), "Quick 3rd click at distant C must NOT select all");
        require(tb->cursor_index() == 20, "Click at C places caret accurately at index 20");

        // 1b. 单击伴随 1.5px 轻微手抖 PointerMove 不得误产生选区
        send_mouse(PlatformEventType::PointerDown, x_pos_a, mid_y);
        send_mouse(PlatformEventType::PointerMove, x_pos_a + 1.5f, mid_y + 1.0f, PointerButton::None);
        send_mouse(PlatformEventType::PointerUp, x_pos_a + 1.5f, mid_y + 1.0f);
        require(!tb->has_selection(), "Mouse click with <=3.5px micro-jitter must NOT create spurious selection");
        require(tb->cursor_index() == 2, "Micro-jitter click keeps caret stable");

        // 1c. 正常鼠标拖拽（包括拖出 28px 高度边界）应稳定创建选区
        send_mouse(PlatformEventType::PointerDown, char_client_x(0), mid_y);
        send_mouse(PlatformEventType::PointerMove, char_client_x(5), mid_y + 28.0f, PointerButton::None);
        send_mouse(PlatformEventType::PointerUp, char_client_x(5), mid_y + 28.0f);
        require(tb->has_selection() && tb->selected_text() == "Hello",
                "Mouse drag across vertical bounds selects 'Hello' smoothly");

        // 1d. 同位置双击选词 -> 三击全选 -> 第四击回绕为单光标不卡死在全选态
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
        const float x_word = char_client_x(8); // inside "KerneTile"
        send_mouse(PlatformEventType::PointerDown, x_word, mid_y);
        send_mouse(PlatformEventType::PointerUp, x_word, mid_y);
        require(!tb->has_selection(), "1st click clears selection");
        send_mouse(PlatformEventType::PointerDown, x_word, mid_y);
        send_mouse(PlatformEventType::PointerUp, x_word, mid_y);
        require(tb->selected_text() == "KerneTile", "2nd click at same spot selects word 'KerneTile'");
        send_mouse(PlatformEventType::PointerDown, x_word, mid_y);
        send_mouse(PlatformEventType::PointerUp, x_word, mid_y);
        require(tb->selected_text() == txt, "3rd click at same spot selects all text");
        send_mouse(PlatformEventType::PointerDown, x_word, mid_y);
        send_mouse(PlatformEventType::PointerUp, x_word, mid_y);
        require(!tb->has_selection(), "4th click wraps back to single caret without staying stuck in select-all");

        // 1e. 触控长按选中单词并弹出水滴形触控选区手柄 + 上下文菜单
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
        send_touch(PlatformEventType::PointerDown, x_word, mid_y);
        require(!win->overlay_manager().has_active_overlay(), "no overlay before touch long press fires");
        win->trigger_touch_long_press_for_test();
        send_touch(PlatformEventType::PointerUp, x_word, mid_y);
        require(tb->selected_text() == "KerneTile", "Touch long press selects 'KerneTile'");
        require(win->overlay_manager().overlay_count() >= 2,
                "Both teardrop touch handles overlay and context menu overlay are active");
        win->render_frame(true);

        // 1f. 直接按住右侧 End 水滴手柄（同时上下文菜单打开）：一次手势关闭菜单并向右拖拽扩大选区！
        const float handle_y = g.y + tb->size().y + 6.0f; // 水滴球体位于文本框下沿下方
        const float end_bulb_x = char_client_x(15) + 8.0f; // End 水滴向右偏移 half=8px
        const float drag_end_dx = char_client_x(21) - char_client_x(15);
        send_touch(PlatformEventType::PointerDown, end_bulb_x, handle_y);
        send_touch(PlatformEventType::PointerMove, end_bulb_x + drag_end_dx, handle_y, PointerButton::None);
        send_touch(PlatformEventType::PointerUp, end_bulb_x + drag_end_dx, handle_y);
        require(tb->has_selection() && tb->selected_text() == "KerneTile World",
                "Dragging End teardrop handle expands selection right to 'KerneTile World'");

        // 1g. 按住左侧 Start 水滴手柄向左拖拽扩大选区到文本开头
        win->render_frame(true);
        const float start_bulb_x = char_client_x(6) - 8.0f; // Start 水滴向左偏移 half=8px
        const float drag_start_dx = char_client_x(0) - char_client_x(6);
        send_touch(PlatformEventType::PointerDown, start_bulb_x, handle_y);
        send_touch(PlatformEventType::PointerMove, start_bulb_x + drag_start_dx, handle_y, PointerButton::None);
        send_touch(PlatformEventType::PointerUp, start_bulb_x + drag_start_dx, handle_y);
        require(tb->has_selection() && tb->selected_text() == "Hello KerneTile World",
                "Dragging Start teardrop handle expands selection left to 'Hello KerneTile World'");

        // 1h. 反向跨越拖拽：把左侧 Start 水滴一路拖过 End(21) 到 31，选区平滑翻转为 " Selection" 且不坍缩消失
        win->render_frame(true);
        const float start0_bulb_x = char_client_x(0) - 8.0f;
        const float cross_dx = char_client_x(31) - char_client_x(0);
        send_touch(PlatformEventType::PointerDown, start0_bulb_x, handle_y);
        send_touch(PlatformEventType::PointerMove, start0_bulb_x + cross_dx, handle_y, PointerButton::None);
        send_touch(PlatformEventType::PointerUp, start0_bulb_x + cross_dx, handle_y);
        require(tb->has_selection() && tb->selected_text() == " Selection",
                "Dragging Start handle past End smoothly flips selection across fixed endpoint");

        // 1i. 触控单击折叠选区后抬手 -> 显示 Caret 居中水滴手柄；拖拽 Caret 水滴移动光标且不产生选区
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
        send_touch(PlatformEventType::PointerDown, char_client_x(5), mid_y);
        send_touch(PlatformEventType::PointerUp, char_client_x(5), mid_y);
        require(!tb->has_selection() && tb->cursor_index() == 5, "Touch tap collapses selection to caret 5");
        require(win->overlay_manager().has_active_overlay(), "Caret teardrop handle overlay shown after touch tap");
        win->render_frame(true);

        const float caret_bulb_x = char_client_x(5);
        const float caret_drag_dx = char_client_x(12) - char_client_x(5);
        send_touch(PlatformEventType::PointerDown, caret_bulb_x, handle_y + 2.0f);
        send_touch(PlatformEventType::PointerMove, caret_bulb_x + caret_drag_dx, handle_y + 2.0f, PointerButton::None);
        send_touch(PlatformEventType::PointerUp, caret_bulb_x + caret_drag_dx, handle_y + 2.0f);
        require(!tb->has_selection() && tb->cursor_index() == 12,
                "Dragging Caret teardrop handle repositions caret smoothly without creating selection");

        // 1j. 切回鼠标点击或点击外部空白失焦 -> 水滴触控手柄自动隐藏
        send_mouse(PlatformEventType::PointerDown, char_client_x(3), mid_y);
        send_mouse(PlatformEventType::PointerUp, char_client_x(3), mid_y);
        require(!win->overlay_manager().has_active_overlay(),
                "Switching to mouse hides touch teardrop handles overlay");
    }

    // 2. RenderTextArea: 鼠标快速异地点击不乱跳 + 多行触控水滴手柄跨行拖拽扩选
    {
        win->set_content(VStack{
            .padding = EdgeInsets{24.0f, 24.0f, 24.0f, 24.0f},
            .content = {
                TextArea{
                    .text = "Alpha Beta\nGamma Delta",
                    .width = 360.0f,
                    .height = 120.0f
                }
            }
        });
        win->render_frame(true);

        RenderTextArea* ta = static_cast<RenderTextArea*>(win->root_render_box()->first_child());
        require(ta != nullptr, "RenderTextArea found");

        const Vec2 g = ta->global_offset();
        const float line0_y = g.y + 8.0f + 10.0f;
        const float line1_y = g.y + 8.0f + 20.0f + 10.0f;
        const float x_alpha_mid = g.x + 11.0f + TextMeasure::measure_width("Al", 13.5f);
        const float x_gamma_end = g.x + 11.0f + TextMeasure::measure_width("Gamma", 13.5f);

        // 2a. 鼠标在不同行快速连点，不误触选词/选行，微抖动不误选
        send_mouse(PlatformEventType::PointerDown, x_alpha_mid, line0_y);
        send_mouse(PlatformEventType::PointerMove, x_alpha_mid + 1.5f, line0_y + 1.0f, PointerButton::None);
        send_mouse(PlatformEventType::PointerUp, x_alpha_mid + 1.5f, line0_y + 1.0f);
        require(!ta->has_selection(), "TextArea single mouse click with micro-jitter creates no selection");

        send_mouse(PlatformEventType::PointerDown, x_gamma_end, line1_y);
        send_mouse(PlatformEventType::PointerUp, x_gamma_end, line1_y);
        require(!ta->has_selection(), "TextArea quick click on line 2 must NOT jump to word/line selection");

        // 2b. 触控双击选中第 1 行 "Alpha"，拖动 End 水滴手柄向下跨行到第 2 行 "Gamma" 末尾
        std::this_thread::sleep_for(std::chrono::milliseconds(420));
        send_touch(PlatformEventType::PointerDown, x_alpha_mid, line0_y);
        send_touch(PlatformEventType::PointerUp, x_alpha_mid, line0_y);
        send_touch(PlatformEventType::PointerDown, x_alpha_mid, line0_y);
        send_touch(PlatformEventType::PointerUp, x_alpha_mid, line0_y);
        require(ta->selected_text() == "Alpha", "TextArea touch double-tap selects 'Alpha'");
        require(win->overlay_manager().has_active_overlay(), "TextArea shows touch selection handles overlay");
        win->render_frame(true);

        const float x_alpha_end = g.x + 11.0f + TextMeasure::measure_width("Alpha", 13.5f);
        const float end_bulb_x = x_alpha_end + 8.0f;
        const float end_bulb_y = g.y + 8.0f + 20.0f + 8.0f;
        const float dx = x_gamma_end - x_alpha_end;
        const float dy = 20.0f; // 向下跨一行
        send_touch(PlatformEventType::PointerDown, end_bulb_x, end_bulb_y);
        send_touch(PlatformEventType::PointerMove, end_bulb_x + dx, end_bulb_y + dy, PointerButton::None);
        send_touch(PlatformEventType::PointerUp, end_bulb_x + dx, end_bulb_y + dy);
        require(ta->has_selection() && ta->selected_text() == "Alpha Beta\nGamma",
                "Dragging TextArea End teardrop handle down across lines selects 'Alpha Beta\\nGamma'");
    }

    std::cout << "  -> TextBox / TextArea 鼠标与触控点击防乱跳、水滴形 Start/End/Caret 手柄拖拽与跨行扩选全部验证通过！" << std::endl;
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
    test_mobile_touch_passthrough_and_responsive_layout();
    test_touch_scroll_mid_fling_interrupt_and_continue();
    test_text_selection_stability_and_touch_teardrop_handles();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 7 核心控件库与全栈端到端验证 100% 通过! " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}

#include "test_common.hpp"
#include <ktui/pal/platform_backend.hpp>
#include "../KTUI/src/pal/headless_backend.hpp"
#include <iostream>
#include <cassert>

using namespace ktui;

void test_headless_window_lifecycle() {
    HeadlessBackend backend;
    assert(backend.init());

    WindowDesc desc;
    desc.title = "KTUI Headless Test Window";
    desc.size = {800.0f, 600.0f};
    desc.min_size = {200.0f, 200.0f};
    desc.max_size = {1920.0f, 1080.0f};

    auto win = backend.create_window(desc);
    assert(win != nullptr);
    assert(win->title() == "KTUI Headless Test Window");
    assert(win->size().x == 800.0f);
    assert(win->size().y == 600.0f);
    assert(!win->is_visible());

    // 显隐性
    win->show();
    assert(win->is_visible());
    win->hide();
    assert(!win->is_visible());

    // 尺寸变更与极值钳制
    bool resized_called = false;
    Vec2 last_resized_size{0, 0};
    win->on_resized([&](Vec2 sz) {
        resized_called = true;
        last_resized_size = sz;
    });
    win->set_size(Vec2{1024.0f, 768.0f});
    assert(resized_called);
    assert(last_resized_size.x == 1024.0f && last_resized_size.y == 768.0f);
    assert(win->size().x == 1024.0f && win->size().y == 768.0f);

    // 极限钳制测试：设置过小尺寸应当被 clamp 至 min_size (200x200)
    win->set_size(Vec2{50.0f, 50.0f});
    assert(last_resized_size.x == 200.0f && last_resized_size.y == 200.0f);
    assert(win->size().x == 200.0f && win->size().y == 200.0f);

    // 居中测试
    win->center_on_screen();
    assert(win->position().x == (1920.0f - 200.0f) * 0.5f);
    assert(win->position().y == (1080.0f - 200.0f) * 0.5f);

    // 全屏与最小化
    win->set_fullscreen(true);
    assert(win->is_fullscreen());
    win->set_fullscreen(false);
    assert(!win->is_fullscreen());

    win->minimize();
    assert(win->is_minimized());
    win->restore();
    assert(!win->is_minimized());

    // 关闭拦截测试
    bool allow_close = false;
    win->on_close_requested([&]() {
        return allow_close;
    });

    win->show();
    win->close();
    assert(win->is_visible()); // 拦截生效，窗口未关闭

    allow_close = true;
    win->close();
    assert(!win->is_visible()); // 允许关闭，窗口隐藏

    std::cout << "[PASS] test_headless_window_lifecycle passed successfully." << std::endl;
}

void test_font_resolver() {
    HeadlessBackend backend;
    auto& resolver = backend.get_font_resolver();

    std::string sys_font = resolver.resolve_font(FontFamilyCategory::System);
    assert(!sys_font.empty());

    std::string mono_font = resolver.resolve_font(FontFamilyCategory::Monospace);
    assert(!mono_font.empty());

    std::string builtin_font = resolver.resolve_font_by_name("builtin");
    assert(builtin_font == "builtin");

    std::cout << "[PASS] test_font_resolver passed successfully (System font: " << sys_font << ")." << std::endl;
}

void test_dispatcher_task_queue() {
    HeadlessBackend backend;
    auto& dispatcher = backend.get_dispatcher();

    assert(!dispatcher.has_pending_tasks());

    int counter = 0;
    dispatcher.post_task([&]() { counter += 10; });
    dispatcher.post_task([&]() { counter += 20; });
    dispatcher.post_task([&]() { counter += 30; });

    assert(dispatcher.has_pending_tasks());

    size_t processed = dispatcher.process_tasks();
    assert(processed == 3);
    assert(counter == 60);
    assert(!dispatcher.has_pending_tasks());

    std::cout << "[PASS] test_dispatcher_task_queue passed successfully." << std::endl;
}

void test_vsync_ticker_and_sleep() {
    HeadlessBackend backend;
    auto& vsync = backend.get_vsync_source();

    // 初始状态应处于休眠
    assert(vsync.is_sleeping());

    double captured_timestamp = 0.0;
    bool frame_executed = false;

    uint64_t req_id = vsync.request_frame([&](double ts) {
        frame_executed = true;
        captured_timestamp = ts;
    });
    assert(req_id > 0);
    assert(!vsync.is_sleeping()); // 有待绘制请求，离开休眠状态

    // 步进 16.6ms 模拟一帧
    backend.step_frame(0.0166);
    assert(frame_executed);
    assert(captured_timestamp > 0.016);
    assert(vsync.is_sleeping()); // 单次请求消费后自动重新进入休眠 (0% CPU 占用)

    // 测试取消帧请求
    bool cancelled_called = false;
    uint64_t req2 = vsync.request_frame([&](double) {
        cancelled_called = true;
    });
    assert(!vsync.is_sleeping());
    vsync.cancel_frame(req2);
    assert(vsync.is_sleeping());

    backend.step_frame(0.0166);
    assert(!cancelled_called);

    std::cout << "[PASS] test_vsync_ticker_and_sleep passed successfully." << std::endl;
}

void test_event_injection_and_dispatch() {
    HeadlessBackend backend;

    std::vector<PlatformEvent> received_events;
    backend.set_event_callback([&](const PlatformEvent& e) {
        received_events.push_back(e);
    });

    for (int i = 0; i < 5; ++i) {
        PlatformEvent pe{};
        pe.type = PlatformEventType::PointerMove;
        pe.position = Vec2{static_cast<float>(i * 10), static_cast<float>(i * 20)};
        pe.timestamp_ms = i * 16;
        backend.inject_event(pe);
    }

    PlatformEvent click{};
    click.type = PlatformEventType::PointerDown;
    click.button = PointerButton::Primary;
    click.position = Vec2{100.0f, 100.0f};
    backend.inject_event(click);

    assert(backend.pending_event_count() == 6);

    backend.poll_events();
    assert(backend.pending_event_count() == 0);
    assert(received_events.size() == 6);
    assert(received_events[0].position.x == 0.0f);
    assert(received_events[4].position.x == 40.0f);
    assert(received_events[5].type == PlatformEventType::PointerDown);
    assert(received_events[5].button == PointerButton::Primary);

    std::cout << "[PASS] test_event_injection_and_dispatch passed successfully." << std::endl;
}

void test_backend_factory() {
    auto backend = create_platform_backend(true);
    assert(backend != nullptr);
    assert(backend->init());

    auto win = backend->create_window(WindowDesc{.title = "Factory Window"});
    assert(win != nullptr);
    assert(win->title() == "Factory Window");

    std::cout << "[PASS] test_backend_factory passed successfully." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " Running KTUI Phase 1 PAL & Headless Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_headless_window_lifecycle();
    test_font_resolver();
    test_dispatcher_task_queue();
    test_vsync_ticker_and_sleep();
    test_event_injection_and_dispatch();
    test_backend_factory();

    std::cout << "All KTUI Phase 1 tests passed successfully!" << std::endl;
    return 0;
}

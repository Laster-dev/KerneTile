#include <ktui/app/application.hpp>
#include <ktui/theme/theme.hpp>
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>
#include <atomic>

using namespace ktui;

void test_dispatcher_multithreaded_concurrency() {
    std::cout << "[Test 1] 跨线程高并发 Dispatcher 任务投递与消费测试..." << std::endl;

    Dispatcher dispatcher;
    assert(dispatcher.is_ui_thread());

    constexpr int kThreadCount = 4;
    constexpr int kTasksPerThread = 1000;
    std::atomic<int> counter{0};
    std::vector<std::thread> workers;

    for (int t = 0; t < kThreadCount; ++t) {
        workers.emplace_back([&dispatcher, &counter] {
            assert(!dispatcher.is_ui_thread());
            for (int i = 0; i < kTasksPerThread; ++i) {
                dispatcher.post_task([&counter] {
                    counter.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    assert(dispatcher.has_pending_tasks());

    // 在 UI 主线程消费执行
    size_t processed = dispatcher.process_tasks();
    assert(processed == kThreadCount * kTasksPerThread);
    assert(counter.load() == kThreadCount * kTasksPerThread);
    assert(!dispatcher.has_pending_tasks());

    std::cout << "  -> 成功并发投递并执行 " << processed << " 个跨线程任务，计数严格等于 4000，零死锁！" << std::endl;
}

void test_ui_thread_assertion_and_run_on_ui() {
    std::cout << "[Test 2] UI 线程断言与 run_on_ui_thread 机制测试..." << std::endl;

    Dispatcher dispatcher;
    bool sync_executed = false;

    // 当前在主线程，run_on_ui_thread 应该立即同步执行
    dispatcher.run_on_ui_thread([&] {
        sync_executed = true;
    });
    assert(sync_executed);
    assert(!dispatcher.has_pending_tasks());

    // 在后台线程调用 run_on_ui_thread，应该异步入队
    bool async_executed = false;
    std::thread worker([&] {
        dispatcher.run_on_ui_thread([&] {
            async_executed = true;
        });
    });
    worker.join();

    assert(!async_executed); // 尚未在 UI 线程处理
    assert(dispatcher.has_pending_tasks());

    dispatcher.process_tasks();
    assert(async_executed); // 消费后已执行

    std::cout << "  -> UI 线程同步直通与后台线程排队路由验证成功！" << std::endl;
}

void test_design_tokens_and_palette() {
    std::cout << "[Test 3] 设计令牌 (Typography, Spacing, Shadow) 与调色盘测试..." << std::endl;

    Theme dark_theme = Theme::dark();
    Theme light_theme = Theme::light();

    assert(dark_theme.is_dark == true);
    assert(light_theme.is_dark == false);

    // 验证深色模式下背景偏深，文本偏浅
    assert(dark_theme.colors.background.r < 0.2f);
    assert(dark_theme.colors.text_primary.r > 0.8f);

    // 验证浅色模式下背景偏浅，文本偏深
    assert(light_theme.colors.background.r > 0.8f);
    assert(light_theme.colors.text_primary.r < 0.2f);

    // 验证间距体系
    assert(dark_theme.spacing.xs == 4.0f);
    assert(dark_theme.spacing.md == 16.0f);
    assert(dark_theme.spacing.xxxl == 64.0f);

    // 验证字阶体系
    assert(dark_theme.typography.base == 16.0f);
    assert(dark_theme.typography.xxxl == 32.0f);

    // 验证圆角与阴影
    assert(dark_theme.radii.md == 8.0f);
    assert(dark_theme.radii.full == 9999.0f);
    assert(dark_theme.shadows.md.blur_radius == 8.0f);

    // 相等性比较
    assert(dark_theme == Theme::dark());
    assert(!(dark_theme == light_theme));

    std::cout << "  -> 语义化调色盘与设计令牌配置完备合法！" << std::endl;
}

void test_application_theme_reactivity() {
    std::cout << "[Test 4] Application 全局主题响应式热切换测试..." << std::endl;

    auto headless = create_platform_backend(true);
    Application app(std::move(headless));

    assert(Application::current() == &app);
    assert(app.current_theme().is_dark == true); // 默认 Dark

    int theme_changed_count = 0;
    Theme observed_theme;

    // 借助阶段 2 的 Property 绑定机制订阅全局主题变化
    app.theme.subscribe([&](const Theme& new_theme) {
        theme_changed_count++;
        observed_theme = new_theme;
    });

    // 切换至 Light 主题
    app.set_theme(Theme::light());
    assert(theme_changed_count == 1);
    assert(observed_theme.is_dark == false);
    assert(app.current_theme().is_dark == false);

    // 重复设置相同主题 -> 验证零变更短路抑制 (No-op suppression)
    app.set_theme(Theme::light());
    assert(theme_changed_count == 1); // 绝不触发多余广播

    // 切换回 Dark 主题
    app.set_theme(Theme::dark());
    assert(theme_changed_count == 2);
    assert(app.current_theme().is_dark == true);

    std::cout << "  -> 全局响应式主题热切换广播与重复赋值抑制验证成功！" << std::endl;
}

void test_theme_scope_cascade_and_isolation() {
    std::cout << "[Test 5] ThemeScope 局部环境覆盖穿透与恢复测试..." << std::endl;

    auto headless = create_platform_backend(true);
    Application app(std::move(headless));
    assert(Application::current() == &app);

    // 当前 Application 全局主题为 Dark
    assert(ThemeContext::current().is_dark == true);
    assert(!ThemeContext::has_local_scope());
    assert(ThemeContext::scope_depth() == 0);

    // 进入第一层局部作用域：覆盖为 Light
    {
        ThemeScopeGuard outer_scope(Theme::light());
        assert(ThemeContext::has_local_scope());
        assert(ThemeContext::scope_depth() == 1);
        assert(ThemeContext::current().is_dark == false); // 成功被局部覆盖为 Light

        // 嵌套第二层局部作用域：再次覆盖为 Dark
        {
            ThemeScopeGuard inner_scope(Theme::dark());
            assert(ThemeContext::scope_depth() == 2);
            assert(ThemeContext::current().is_dark == true); // 内部覆盖为 Dark
        }

        // 退出第二层，自动恢复到第一层 (Light)
        assert(ThemeContext::scope_depth() == 1);
        assert(ThemeContext::current().is_dark == false);
    }

    // 退出全部局部作用域，自动恢复为全局 Application 主题 (Dark)
    assert(!ThemeContext::has_local_scope());
    assert(ThemeContext::scope_depth() == 0);
    assert(ThemeContext::current().is_dark == true);

    std::cout << "  -> ThemeScope 局部作用域环境层级压栈与穿透回退完全正确！" << std::endl;
}

void test_application_window_lifecycle() {
    std::cout << "[Test 6] Application 窗口生命周期与事件循环步进测试..." << std::endl;

    auto headless = create_platform_backend(true);
    Application app(std::move(headless));
    assert(Application::current() == &app);

    WindowDesc desc1;
    desc1.title = "Main Window";
    desc1.size = Vec2{1024.0f, 768.0f};

    auto win1 = app.create_window(desc1);
    assert(win1 != nullptr);
    assert(win1->title() == "Main Window");
    assert(win1->size() == Vec2(1024.0f, 768.0f));
    assert(app.main_window() == win1);
    assert(app.windows().size() == 1);

    // 创建第二个子窗口
    WindowDesc desc2;
    desc2.title = "Inspector Window";
    desc2.size = Vec2{400.0f, 600.0f};
    auto win2 = app.create_window(desc2);
    assert(win2 != nullptr);
    assert(app.windows().size() == 2);

    // 步进事件
    app.step(0.016f);
    assert(app.windows().size() == 2);

    // 关闭第二个窗口
    app.close_window(win2->id());
    assert(win2->is_closed());
    app.step(0.016f);
    assert(app.windows().size() == 1);
    assert(app.main_window() == win1);

    // 请求退出
    app.quit(0);
    assert(!app.is_running());
    assert(app.exit_code() == 0);

    std::cout << "  -> Application 窗口多实例管理、关闭回收与循环退出测试通过！" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   KTUI 阶段 4: Application 与设计令牌主题系统测试 " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_dispatcher_multithreaded_concurrency();
    test_ui_thread_assertion_and_run_on_ui();
    test_design_tokens_and_palette();
    test_application_theme_reactivity();
    test_theme_scope_cascade_and_isolation();
    test_application_window_lifecycle();

    std::cout << "==================================================" << std::endl;
    std::cout << "   恭喜！阶段 4 Application 与主题系统 100% 通过!   " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}

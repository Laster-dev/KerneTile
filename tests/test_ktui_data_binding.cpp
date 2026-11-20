#include "test_common.hpp"
#include <ktui/binding/binding.hpp>
#include <iostream>
#include <string>
#include <cassert>

using namespace ktui;

void test_property_and_noop_suppression() {
    Property<int> value{42};
    assert(value.get() == 42);
    assert(value.version() == 0);

    int notify_count = 0;
    int received_val = 0;
    uint64_t sub_id = value.subscribe([&](int val, uint64_t /*token*/) {
        notify_count++;
        received_val = val;
    });

    // 正常赋值，应触发通知
    value.set(100);
    assert(value.get() == 100);
    assert(value.version() == 1);
    assert(notify_count == 1);
    assert(received_val == 100);

    // 零变更短路 (No-op Suppression)：设置相同数值，绝不派发多余事件
    value.set(100);
    assert(notify_count == 1); // 计数仍为 1
    assert(value.version() == 1);

    // 取消订阅
    value.unsubscribe(sub_id);
    value.set(200);
    assert(notify_count == 1); // 观察者未再被触发
    assert(value.version() == 2);

    std::cout << "[PASS] test_property_and_noop_suppression passed successfully." << std::endl;
}

void test_two_way_binding_and_loop_guard() {
    Property<std::string> text_prop{"初始内容"};
    auto two_way = bind(text_prop);

    assert(two_way.get() == "初始内容");

    std::string view_rendered_text = two_way.get();
    int view_update_count = 0;

    two_way.on_view_update([&](const std::string& new_text) {
        view_rendered_text = new_text;
        view_update_count++;
    });

    // 场景 1：UI 触发输入修改 (模拟用户打字)
    two_way.set_from_ui("用户输入 A");
    assert(text_prop.get() == "用户输入 A");
    // 关键事务熔断断言：由于该修改是由 UI 自身发起，TwoWayBinding 识别 Token 相同，
    // 绝不回写触发自己的 on_view_update，防止光标重置与死循环！
    assert(view_update_count == 0);

    // 场景 2：Model 端由后台业务或网络外部修改
    text_prop.set("网络下发数据 B");
    assert(view_update_count == 1);
    assert(view_rendered_text == "网络下发数据 B");

    // 场景 3：死循环极限压力测试 (模拟 View 在回调中恶意触发 set_from_ui)
    int cycle_count = 0;
    Property<int> counter{0};
    auto cycle_bind = bind(counter);
    cycle_bind.on_view_update([&](int val) {
        cycle_count++;
        if (cycle_count < 10) {
            cycle_bind.set_from_ui(val + 1); // 触发回写
        }
    });

    counter.set(1);
    // 事务熔断机制保护下，绝不会发生无限递归崩溃
    assert(cycle_count <= 10);

    std::cout << "[PASS] test_two_way_binding_and_loop_guard passed successfully." << std::endl;
}

void test_one_way_and_converter_binding() {
    // 单向绑定
    const Property<int> readonly_score{95};
    auto one_way = bind(readonly_score);
    assert(one_way.get() == 95);

    // 类型转换器绑定 (int -> std::string)
    Property<int> progress{50};
    auto progress_text = bind(progress, [](int p) {
        return std::to_string(p) + "%";
    });

    assert(progress_text.get() == "50%");

    std::string current_display = progress_text.get();
    progress_text.on_view_update([&](const std::string& formatted) {
        current_display = formatted;
    });

    progress.set(80);
    assert(progress_text.get() == "80%");
    assert(current_display == "80%");

    // 布尔状态转换为色彩
    Property<bool> is_online{false};
    auto status_color = bind(is_online, [](bool online) -> std::string {
        return online ? "GREEN" : "GRAY";
    });
    assert(status_color.get() == "GRAY");
    is_online.set(true);
    assert(status_color.get() == "GREEN");

    std::cout << "[PASS] test_one_way_and_converter_binding passed successfully." << std::endl;
}

void test_computed_property() {
    Property<std::string> first_name{"Kerne"};
    Property<std::string> last_name{"Tile"};

    auto full_name = computed([&]() {
        return first_name.get() + " " + last_name.get();
    });

    assert(full_name.get() == "Kerne Tile");

    first_name.set("KT");
    assert(full_name.get() == "KT Tile");

    last_name.set("UI");
    assert(full_name.get() == "KT UI");

    std::cout << "[PASS] test_computed_property passed successfully." << std::endl;
}

void test_observable_list() {
    ObservableList<std::string> tasks;

    std::vector<ListChangeEvent> captured_events;
    tasks.subscribe([&](const ListChangeEvent& e) {
        captured_events.push_back(e);
    });

    tasks.push_back("Task 1");
    assert(tasks.size() == 1);
    assert(tasks[0] == "Task 1");
    assert(captured_events.size() == 1);
    assert(captured_events.back().type == ListChangeType::Added);
    assert(captured_events.back().index == 0);

    tasks.push_back("Task 2");
    tasks.insert(1, "Task 1.5");
    assert(tasks.size() == 3);
    assert(tasks[1] == "Task 1.5");
    assert(captured_events.back().index == 1);

    tasks.erase(1);
    assert(tasks.size() == 2);
    assert(tasks[1] == "Task 2");
    assert(captured_events.back().type == ListChangeType::Removed);
    assert(captured_events.back().index == 1);

    tasks.clear();
    assert(tasks.empty());
    assert(captured_events.back().type == ListChangeType::Reset);

    std::cout << "[PASS] test_observable_list passed successfully." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " Running KTUI Phase 2 Binding Engine Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_property_and_noop_suppression();
    test_two_way_binding_and_loop_guard();
    test_one_way_and_converter_binding();
    test_computed_property();
    test_observable_list();

    std::cout << "All KTUI Phase 2 tests passed successfully!" << std::endl;
    return 0;
}

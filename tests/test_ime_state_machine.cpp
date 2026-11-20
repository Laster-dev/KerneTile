#include "test_common.hpp"
#include "kernetile/input/ime_state_machine.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace kernetile::input;

void test_win32_ime_lifecycle() {
    std::cout << "[Test 1] Testing Win32 IMM32 IME Lifecycle..." << std::endl;

    ImeStateMachine ime;
    assert(ime.get_state() == ImeState::Idle);

    // 1. 开始组合 (WM_IME_STARTCOMPOSITION)
    ime.on_win32_start_composition();
    assert(ime.get_state() == ImeState::Composing);
    assert(ime.get_composition().is_composing == true);

    // 2. 键入拼音 "pin"
    ime.on_win32_composition("pin", "", 3);
    assert(ime.get_state() == ImeState::Composing);
    assert(ime.get_composition().preedit_text == "pin");
    assert(ime.get_composition().cursor_position == 3);

    // 3. 键入完整 "pinyin"
    ime.on_win32_composition("pinyin", "", 6);
    assert(ime.get_composition().preedit_text == "pinyin");

    // 4. 选词 "拼音" 上屏
    ime.on_win32_composition("", "拼音", 0);
    assert(ime.get_state() == ImeState::Committed);
    assert(ime.get_document_text() == "拼音");
    assert(ime.get_composition().preedit_text.empty());

    // 5. 组合结束 (WM_IME_ENDCOMPOSITION)
    ime.on_win32_end_composition();
    assert(ime.get_state() == ImeState::Idle);
    assert(ime.get_composition().is_composing == false);

    std::cout << "  -> Win32 IMM32 IME Lifecycle PASSED." << std::endl;
}

void test_wayland_six_step_commit() {
    std::cout << "[Test 2] Testing Wayland zwp_text_input_v3 Six-Step Atomic Protocol..." << std::endl;

    ImeStateMachine ime;

    // 事务 1: 提交 "KerneTile "，并开启预编辑 "xuan"
    WaylandCommitTransaction tx1{};
    tx1.commit_string = "KerneTile ";
    tx1.preedit_string = "xuan";
    tx1.preedit_cursor = 4;
    ime.execute_wayland_commit(tx1);

    assert(ime.get_state() == ImeState::Composing);
    assert(ime.get_document_text() == "KerneTile ");
    assert(ime.get_composition().preedit_text == "xuan");

    // 事务 2: 退格删除 1 字节并提交 "渲染引擎"
    WaylandCommitTransaction tx2{};
    tx2.commit_string = "渲染引擎";
    tx2.preedit_string = "";
    ime.execute_wayland_commit(tx2);

    assert(ime.get_state() == ImeState::Committed);
    assert(ime.get_document_text() == "KerneTile 渲染引擎");
    assert(ime.get_composition().preedit_text.empty());

    // 事务 3: 删除周边字符 (例如按退格删除 "引擎" 6 字节)
    WaylandCommitTransaction tx3{};
    tx3.delete_before = 6; // UTF-8 中文每个字符 3 字节，两个汉字 6 字节
    ime.execute_wayland_commit(tx3);

    assert(ime.get_document_text() == "KerneTile 渲染");

    std::cout << "  -> Wayland zwp_text_input_v3 Six-Step Atomic Protocol PASSED." << std::endl;
}

void test_utf8_boundary_safety() {
    std::cout << "[Test 3] Testing UTF-8 Safe Truncation (4000 Bytes Limit)..." << std::endl;

    // 构造由中文多字节字符构成的超长文本 (例如每个汉字 3 字节)
    std::string sample = "高性能图形界面计算渲染引擎核心"; // 15 个汉字 = 45 字节
    std::string long_str;
    while (long_str.size() < 4200) {
        long_str += sample;
    }

    // 在 4000 字节处截断
    std::string truncated = ImeStateMachine::truncate_utf8_safe(long_str, 4000);

    // 验证：
    // 1. 尺寸必须 <= 4000
    assert(truncated.size() <= 4000);
    // 2. 必须是完全合法的 UTF-8，没有截断的损坏码点
    assert(ImeStateMachine::is_valid_utf8(truncated));
    // 3. 截断边界必须是汉字倍数 (45 字节 sample，4000 / 3 = 1333 汉字 = 3999 字节)
    assert(truncated.size() % 3 == 0);

    std::cout << "  Truncated length: " << truncated.size() << " bytes (Limit: 4000, Valid UTF-8: YES)" << std::endl;
    std::cout << "  -> UTF-8 boundary safety PASSED." << std::endl;
}

int main() {
    configure_test_environment();
    std::cout << "=== Phase 8 IME State Machine Unit Tests ===" << std::endl;

    test_win32_ime_lifecycle();
    test_wayland_six_step_commit();
    test_utf8_boundary_safety();

    std::cout << "=== All IME State Machine Tests PASSED (100%) ===" << std::endl;
    return 0;
}

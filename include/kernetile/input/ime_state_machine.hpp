#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

namespace kernetile::input {

enum class ImeState : uint8_t {
    Idle       = 0, // 无活动组合
    Composing  = 1, // 正在输入拼音/假名 (Preedit 未决状态)
    Committed  = 2, // 词汇已敲定上屏
    Canceled   = 3  // 组合被用户放弃/取消
};

// 预编辑组合文本状态
struct ImeCompositionState {
    std::string preedit_text{};   // 当前组合中的 UTF-8 字符串
    int32_t     cursor_position{0}; // 预编辑字符串内的字节/字符偏移
    int32_t     selection_start{-1};
    int32_t     selection_end{-1};
    bool        is_composing{false};
};

// 光标浮动定位包围盒 (用于操作系统候选窗贴靠)
struct ImeCursorAnchor {
    float x{0.0f};
    float y{0.0f};
    float width{2.0f};
    float height{20.0f};
};

// Wayland zwp_text_input_v3 事务参数
struct WaylandCommitTransaction {
    uint32_t    delete_before{0};  // 向左删除的字节数
    uint32_t    delete_after{0};   // 向右删除的字节数
    std::string commit_string{};   // 正式提交入库的文字
    std::string preedit_string{};  // 下一轮继续组合的未决文字
    int32_t     preedit_cursor{0}; // 预编辑串内的光标偏移
};

// ============================================================================
// 跨平台统一异步 IME 状态机
// 完整覆盖 Windows IMM32/TSF 与 Linux Wayland zwp_text_input_v3 六步原子规范
// ============================================================================
class ImeStateMachine {
public:
    ImeStateMachine() = default;

    // 获取当前状态
    ImeState get_state() const { return m_state; }

    // 获取当前组合状态
    const ImeCompositionState& get_composition() const { return m_composition; }

    // 获取当前正式已提交的文本内容
    const std::string& get_document_text() const { return m_document_text; }

    // 获取光标定位矩形
    const ImeCursorAnchor& get_cursor_anchor() const { return m_anchor; }

    // 更新光标在屏幕/视口中的像素坐标 (通知输入法候选框对齐)
    void update_cursor_anchor(float x, float y, float w, float h);

    // ------------------------------------------------------------------------
    // Wayland zwp_text_input_v3 严格六步原子事务执行
    // ------------------------------------------------------------------------
    void execute_wayland_commit(const WaylandCommitTransaction& tx);

    // ------------------------------------------------------------------------
    // Windows IMM32 事件桥接
    // ------------------------------------------------------------------------
    void on_win32_start_composition();
    void on_win32_composition(
        std::string_view preedit_str,
        std::string_view result_str,
        int32_t cursor_pos);
    void on_win32_end_composition();

    // 手动重置/清空状态机
    void reset();

    // UTF-8 安全截断工具函数 (确保单条消息不超过 max_bytes 且不破坏多字节码点)
    static std::string truncate_utf8_safe(std::string_view str, size_t max_bytes);

    // 验证 UTF-8 序列有效性
    static bool is_valid_utf8(std::string_view str);

private:
    ImeState            m_state{ImeState::Idle};
    ImeCompositionState m_composition{};
    std::string         m_document_text{}; // 模拟文档真实已提交文本
    ImeCursorAnchor     m_anchor{};
};

} // namespace kernetile::input

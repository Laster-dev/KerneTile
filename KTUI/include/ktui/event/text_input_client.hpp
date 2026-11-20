#pragma once

#include <ktui/pal/platform_types.hpp>
#include <string>

namespace ktui {

// 框架级 IME 输入法宿主协议接口 (ITextInputClient)
// 任何需要接收系统输入法（中文/日文/韩文/Emoji 等）的控件（TextInput、Terminal、CodeEditor 等）
// 只需继承此接口并实现以下回调，KTUI 框架会自动接管焦点切换时的 IME 启停与候选框光标跟随定位。
class ITextInputClient {
public:
    virtual ~ITextInputClient() = default;

    // 声明当前控件是否接受文本/IME 输入
    [[nodiscard]] virtual bool accepts_text_input() const noexcept { return true; }

    // 向框架汇报当前输入光标在窗口客户区中的绝对矩形坐标 (供系统输入法定位候选框)
    [[nodiscard]] virtual Rect get_ime_cursor_rect() const = 0;

    // 接收输入法组合态预编辑文本 (如拼音 "nihao")；text 为空字符串表示组合态结束/取消
    virtual void on_ime_preedit(const std::string& text, size_t cursor_byte_pos) = 0;

    // 接收最终确认上屏的 UTF-8 文本 (如 "你好" 或普通键入字符)
    virtual void on_ime_commit(const std::string& utf8_text) = 0;
};

} // namespace ktui

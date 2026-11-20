#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <string>

namespace ktui {

// 跨平台系统剪贴板统一门面 (Clipboard)
// 自动代理当前活跃的 IPlatformBackend (Win32 CF_UNICODETEXT / Linux / Headless)，
// 在独立控件单元测试无 Application 实例时自动降级为进程内内存剪贴板。
class Clipboard {
public:
    static void bind_backend(IPlatformBackend* backend) noexcept {
        s_backend = backend;
    }

    [[nodiscard]] static std::string get_text() {
        if (s_backend) {
            return s_backend->get_clipboard_text();
        }
        return s_fallback;
    }

    static void set_text(const std::string& utf8_text) {
        if (s_backend) {
            s_backend->set_clipboard_text(utf8_text);
        } else {
            s_fallback = utf8_text;
        }
    }

private:
    inline static IPlatformBackend* s_backend = nullptr;
    inline static std::string s_fallback;
};

} // namespace ktui

#pragma once

#include <ktui/pal/platform_backend.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ktui {

// 剪贴板 RGBA8 位图数据
struct ClipboardImage {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint8_t> rgba8; // 紧密排布 RGBA8 (width * height * 4)

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 &&
               rgba8.size() >= static_cast<size_t>(width) * height * 4u;
    }
};

// 跨平台系统剪贴板统一门面 (Clipboard)
// 默认包含轻量 UTF-8 文本剪贴板；图片 (CF_DIB) 与文件列表 (CF_HDROP) 通过 CMake
// -DKTUI_ENABLE_RICH_CLIPBOARD=ON 按需启用原生系统桥接，默认关闭零核心体积开销。
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

    // ==================== 可选富媒体剪贴板 (图片 / 文件列表) ====================
    [[nodiscard]] static bool is_rich_clipboard_enabled() noexcept {
#if defined(KTUI_ENABLE_RICH_CLIPBOARD) && KTUI_ENABLE_RICH_CLIPBOARD
        return true;
#else
        return false;
#endif
    }

    [[nodiscard]] static bool has_image();
    [[nodiscard]] static ClipboardImage get_image();
    static bool set_image(const ClipboardImage& image);

    [[nodiscard]] static bool has_files();
    [[nodiscard]] static std::vector<std::string> get_files();
    static bool set_files(const std::vector<std::string>& paths);

private:
    inline static IPlatformBackend* s_backend = nullptr;
    inline static std::string s_fallback;
    inline static ClipboardImage s_fallback_image;
    inline static std::vector<std::string> s_fallback_files;
};

#if !defined(KTUI_ENABLE_RICH_CLIPBOARD) || !KTUI_ENABLE_RICH_CLIPBOARD
// 未启用原生富媒体剪贴板时提供零依赖进程内回退，不引入任何系统图像/Shell 剪贴板代码
inline bool Clipboard::has_image() { return s_fallback_image.valid(); }
inline ClipboardImage Clipboard::get_image() { return s_fallback_image; }
inline bool Clipboard::set_image(const ClipboardImage& image) {
    if (!image.valid()) return false;
    s_fallback_image = image;
    return true;
}
inline bool Clipboard::has_files() { return !s_fallback_files.empty(); }
inline std::vector<std::string> Clipboard::get_files() { return s_fallback_files; }
inline bool Clipboard::set_files(const std::vector<std::string>& paths) {
    s_fallback_files = paths;
    return !paths.empty();
}
#endif

} // namespace ktui

#pragma once

#include <ktui/core/fn.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace ktui {

// 文件过滤器项 (例如 {"图像文件", "*.png;*.jpg;*.bmp"})
struct FileFilter {
    std::string name;     // 显示名称 (如 "Images")
    std::string pattern;  // 分号分隔的扩展名通配符 (如 "*.png;*.jpg;*.bmp")
};

// 对话框类型
enum class FileDialogKind : uint8_t {
    OpenFile = 0,
    OpenMultipleFiles,
    SaveFile,
    SelectFolder
};

// 对话框配置选项
struct FileDialogOptions {
    FileDialogKind kind{FileDialogKind::OpenFile};
    std::string title;
    std::string default_dir;
    std::string default_name;
    std::vector<FileFilter> filters;
    void* owner_window_handle{nullptr}; // 可选父窗口原生句柄 (Win32 HWND)
    // Web 端 save_file 可直接传入待下载的字节内容
    std::vector<uint8_t> save_bytes;
};

// 对话框返回结果
struct FileDialogResult {
    bool accepted{false};
    std::vector<std::string> paths; // 选中的文件或目录 UTF-8 路径 (Web 端为文件名)
    // Web 端读取到的首个文件二进制内容 (桌面端留空，由调用方按 paths 读取)
    std::vector<uint8_t> file_bytes;

    [[nodiscard]] std::string first_path() const {
        return paths.empty() ? std::string{} : paths.front();
    }
};

// 跨平台原生文件/文件夹对话框统一门面 (Win32 IFileDialog / Linux xdg-desktop-portal / Web <input type="file"> / Headless Mock)
class FileDialog {
public:
    using Callback = Fn<void(const FileDialogResult&)>;
    using MockHandler = Fn<FileDialogResult(const FileDialogOptions&)>;

    // 异步/回调式统一接口 (全平台兼容，包含 Web 异步 <input type="file">)
    static void show(const FileDialogOptions& options, Callback on_complete);

    static void open_file(const FileDialogOptions& options, Callback on_complete) {
        FileDialogOptions opt = options;
        if (opt.kind != FileDialogKind::OpenMultipleFiles) {
            opt.kind = FileDialogKind::OpenFile;
        }
        show(opt, std::move(on_complete));
    }

    static void save_file(const FileDialogOptions& options, Callback on_complete) {
        FileDialogOptions opt = options;
        opt.kind = FileDialogKind::SaveFile;
        show(opt, std::move(on_complete));
    }

    static void select_folder(const FileDialogOptions& options, Callback on_complete) {
        FileDialogOptions opt = options;
        opt.kind = FileDialogKind::SelectFolder;
        show(opt, std::move(on_complete));
    }

    // 同步便捷接口 (适用于 Win32 / Linux 桌面端与 Headless 测试；Web 端请使用回调版本)
    static FileDialogResult show_sync(const FileDialogOptions& options);

    // 单元测试 / Headless 模拟注入 (设置后绕过操作系统原生弹窗)
    static void set_mock_handler(MockHandler handler);
    static void clear_mock_handler();
};

} // namespace ktui

#include "../gallery_pages.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

std::string extract_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

} // namespace

// ==================== 2. 原生文件 / 文件夹对话框演示页 ====================
WidgetPtr build_file_dialog_page(GalleryApp& app) {
    auto owner_hwnd = [&app]() -> void* {
        if (app.window && app.window->platform_window()) {
            return app.window->platform_window()->get_native_handle();
        }
        return nullptr;
    };

    auto apply_result = [&app](const char* action_name, const FileDialogResult& res) {
        if (!res.accepted || res.paths.empty()) {
            app.file_dialog_status.set(std::string(action_name) + ": 用户已取消选择");
            return;
        }
        app.file_dialog_status.set(std::string(action_name) + ": 已确认选择 " +
                                   std::to_string(res.paths.size()) + " 项");
        std::string list;
        for (size_t i = 0; i < res.paths.size(); ++i) {
            if (i > 0) list += "\n";
            list += std::to_string(i + 1) + ". " + res.paths[i];
        }
        app.file_dialog_result.set(list);
        Toast::show(ToastDesc{
            .title = action_name,
            .message = "已选中 " + std::to_string(res.paths.size()) + " 项: " + extract_filename(res.first_path()),
            .severity = ToastSeverity::Success
        });
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("FileDialog 原生文件对话框", 22.0f),
            app.make_muted("跨平台操作系统原生文件 / 文件夹选择器（Win32 COM IFileOpenDialog / IFileSaveDialog，按需延迟加载 shell32.dll；Linux zenity/kdialog；Web <input type=\"file\">；Headless Mock 可测）。", 12.5f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "原生系统弹窗触发器", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Wrap {
                        .spacing = 10.0f,
                        .run_spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "打开单个文件...",
                                .variant = ButtonVariant::Accent,
                                .on_click = [owner_hwnd, apply_result]() {
                                    FileDialogOptions opt;
                                    opt.kind = FileDialogKind::OpenFile;
                                    opt.title = "选择单个文件";
                                    opt.owner_window_handle = owner_hwnd();
                                    opt.filters = {
                                        {"图像文件", "*.bmp;*.png;*.jpg;*.svg"},
                                        {"C++ 源代码", "*.cpp;*.hpp;*.h;*.cmake"},
                                        {"所有文件", "*.*"}
                                    };
                                    FileDialog::open_file(opt, [apply_result](const FileDialogResult& res) {
                                        apply_result("打开单个文件", res);
                                    });
                                }
                            },
                            Button {
                                .text = "多选文件...",
                                .on_click = [owner_hwnd, apply_result]() {
                                    FileDialogOptions opt;
                                    opt.kind = FileDialogKind::OpenMultipleFiles;
                                    opt.title = "选择一个或多个文件";
                                    opt.owner_window_handle = owner_hwnd();
                                    opt.filters = {
                                        {"所有文件", "*.*"},
                                        {"文本文档", "*.txt;*.md;*.json"}
                                    };
                                    FileDialog::open_file(opt, [apply_result](const FileDialogResult& res) {
                                        apply_result("多选文件", res);
                                    });
                                }
                            },
                            Button {
                                .text = "保存文件...",
                                .on_click = [owner_hwnd, apply_result]() {
                                    FileDialogOptions opt;
                                    opt.kind = FileDialogKind::SaveFile;
                                    opt.title = "选择保存位置（演示模式不会覆盖磁盘）";
                                    opt.default_name = "kernetile_export.txt";
                                    opt.owner_window_handle = owner_hwnd();
                                    opt.filters = {
                                        {"文本文档", "*.txt"},
                                        {"JSON 数据", "*.json"},
                                        {"所有文件", "*.*"}
                                    };
                                    const std::string demo_content = "Exported from KerneTile KTUI Gallery\n";
                                    opt.save_bytes.assign(demo_content.begin(), demo_content.end());
                                    FileDialog::save_file(opt, [apply_result](const FileDialogResult& res) {
                                        apply_result("保存文件", res);
                                    });
                                }
                            },
                            Button {
                                .text = "选择文件夹...",
                                .on_click = [owner_hwnd, apply_result]() {
                                    FileDialogOptions opt;
                                    opt.kind = FileDialogKind::SelectFolder;
                                    opt.title = "选择目标目录";
                                    opt.owner_window_handle = owner_hwnd();
                                    FileDialog::select_folder(opt, [apply_result](const FileDialogResult& res) {
                                        apply_result("选择文件夹", res);
                                    });
                                }
                            }
                        }
                    },
                    app.make_accent(app.file_dialog_status, 12.5f, FontWeight::Medium)
                }
            }),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            Text { .text = "返回路径结果", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                            HStack {
                                .spacing = 8.0f,
                                .content = {
                                    Button {
                                        .text = "复制结果",
                                        .on_click = [&app]() {
                                            Clipboard::set_text(app.file_dialog_result.get());
                                            Toast::show(ToastDesc{ .title = "已复制到剪贴板", .message = "路径结果已复制", .severity = ToastSeverity::Success });
                                        }
                                    },
                                    Button {
                                        .text = "清空",
                                        .on_click = [&app]() {
                                            app.file_dialog_status.set("已清空选择记录");
                                            app.file_dialog_result.set("尚未选择任何文件或目录");
                                        }
                                    }
                                }
                            }
                        }
                    },
                    Box {
                        .background_color = Color{0.0f, 0.0f, 0.0f, 0.16f},
                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.08f},
                        .border_width = 1.0f,
                        .corner_radius = 6.0f,
                        .padding = EdgeInsets(10.0f, 12.0f),
                        .child = Text { .font_size = 12.0f }
                            .bind(&RenderText::set_text, app.file_dialog_result)
                            .bind(&RenderText::set_color, app.muted_text)
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

#include "../gallery_pages.hpp"
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

// ==============================================================================
// 5. Terminal (Windows Terminal 对标终端控件 — 自由对接任意后端)
// ==============================================================================
WidgetPtr build_terminal_page(GalleryApp& app) {
    auto ctrl = std::make_shared<TerminalController>();
    auto status = std::make_shared<Property<std::string>>("当前后端: DemoShell (Built-in)  |  支持 Ctrl+Shift+F 查找 / Ctrl+Shift+C/V / Alt 块选 / 右键菜单 / 任意自定义后端");

    auto make_echo_backend = []() -> std::shared_ptr<ITerminalBackend> {
        auto ptr = std::make_shared<std::weak_ptr<CallbackTerminalBackend>>();
        auto cb = std::make_shared<CallbackTerminalBackend>(CallbackTerminalBackend::Handlers{
            .name = "CallbackTerminalBackend (Custom Echo/Pipe)",
            .on_start = [ptr](int cols, int rows) {
                if (auto b = ptr->lock()) {
                    b->feed_output("\x1b]0;Custom Callback Backend\x07");
                    b->feed_output("\x1b[1;35m[CallbackTerminalBackend]\x1b[0m 已接入自定义回调后端 (" +
                                   std::to_string(cols) + "x" + std::to_string(rows) + ")\r\n");
                    b->feed_output("\x1b[90m该适配器可零侵入对接 SSH / WebSocket / 串口 / Docker Exec / 命名管道。\x1b[0m\r\n");
                    b->feed_output("任意键入内容都将以 24-bit TrueColor 回显 > ");
                }
                return true;
            },
            .on_write = [ptr](std::string_view bytes) {
                if (auto b = ptr->lock()) {
                    for (char ch : bytes) {
                        if (ch == '\r' || ch == '\n') {
                            b->feed_output("\r\n\x1b[38;2;96;205;255m[RX]\x1b[0m > ");
                        } else if (ch == 0x7f || ch == 0x08) {
                            b->feed_output("\b \b");
                        } else {
                            b->feed_output(std::string_view(&ch, 1));
                        }
                    }
                }
            }
        });
        *ptr = cb;
        return cb;
    };

    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 10.0f,
        .content = {
            app.make_page_title("Terminal"),
            app.make_muted("对标微软开源 Windows Terminal (TermControl)：完整 VT500/ANSI/256色/24-bit TrueColor、双屏缓冲、WinUIScrollBar 历史回滚、IME 中文输入、Ctrl+Shift+F 内联搜索、OSC 9;4 进度条，并通过 ITerminalBackend 自由对接任意后端。"),
            // 工具栏：后端切换 + 配色方案 + 光标样式 + 快捷操作
            Wrap {
                .spacing = 8.0f,
                .run_spacing = 6.0f,
                .content = {
                    ComboBox {
                        .items = {
                            "DemoShell (内置演示)",
#if defined(_WIN32)
                            "ConPTY: PowerShell",
                            "ConPTY: CMD",
#endif
                            "自定义回调后端 (Callback)"
                        },
                        .selected_index = 0,
                        .width = 196.0f,
                        .on_change = [ctrl, status, make_echo_backend](int idx, const std::string&) {
#if defined(_WIN32)
                            if (idx == 0) {
                                ctrl->set_backend(std::make_shared<DemoShellBackend>());
                            } else if (idx == 1) {
                                ctrl->set_backend(std::make_shared<ConPtyTerminalBackend>("powershell.exe -NoLogo"));
                            } else if (idx == 2) {
                                ctrl->set_backend(std::make_shared<ConPtyTerminalBackend>("cmd.exe"));
                            } else {
                                ctrl->set_backend(make_echo_backend());
                            }
#else
                            if (idx == 0) ctrl->set_backend(std::make_shared<DemoShellBackend>());
                            else ctrl->set_backend(make_echo_backend());
#endif
                            status->set("当前后端: " + ctrl->backend_name());
                        }
                    },
                    ComboBox {
                        .items = {"Campbell", "One Half Dark", "One Half Light", "Solarized Dark", "Tango Dark", "Vintage"},
                        .selected_index = 0,
                        .width = 148.0f,
                        .on_change = [ctrl](int idx, const std::string&) {
                            switch (idx) {
                                case 0: ctrl->set_color_scheme(TerminalColorScheme::campbell()); break;
                                case 1: ctrl->set_color_scheme(TerminalColorScheme::one_half_dark()); break;
                                case 2: ctrl->set_color_scheme(TerminalColorScheme::one_half_light()); break;
                                case 3: ctrl->set_color_scheme(TerminalColorScheme::solarized_dark()); break;
                                case 4: ctrl->set_color_scheme(TerminalColorScheme::tango_dark()); break;
                                case 5: ctrl->set_color_scheme(TerminalColorScheme::vintage()); break;
                                default: break;
                            }
                        }
                    },
                    ComboBox {
                        .items = {"光标: Bar |", "光标: FilledBox █", "光标: Vintage _", "光标: EmptyBox □", "光标: Double _"},
                        .selected_index = 0,
                        .width = 148.0f,
                        .on_change = [ctrl](int idx, const std::string&) {
                            static constexpr TerminalCursorStyle kStyles[] = {
                                TerminalCursorStyle::Bar,
                                TerminalCursorStyle::FilledBox,
                                TerminalCursorStyle::Vintage,
                                TerminalCursorStyle::EmptyBox,
                                TerminalCursorStyle::DoubleUnderscore
                            };
                            if (idx >= 0 && idx < 5) ctrl->set_cursor_style(kStyles[idx]);
                        }
                    },
                    Button {
                        .text = "TrueColor 测试",
                        .on_click = [ctrl]() {
                            ctrl->feed_output("\r\n\x1b[1m[24-bit TrueColor]\x1b[0m ");
                            for (int i = 0; i < 36; ++i) {
                                int r = (i < 12) ? (255 - i * 21) : (i >= 24 ? (i - 24) * 21 : 0);
                                int g = (i < 12) ? (i * 21) : (i < 24 ? (255 - (i - 12) * 21) : 0);
                                int b = (i >= 12 && i < 24) ? ((i - 12) * 21) : (i >= 24 ? (255 - (i - 24) * 21) : 0);
                                char seq[48];
                                std::snprintf(seq, sizeof(seq), "\x1b[48;2;%d;%d;%dm ", r, g, b);
                                ctrl->feed_output(seq);
                            }
                            ctrl->feed_output("\x1b[0m\r\n");
                        }
                    },
                    Button {
                        .text = "OSC 9;4 进度",
                        .on_click = [ctrl, prog_on = std::make_shared<bool>(false)]() {
                            *prog_on = !*prog_on;
                            ctrl->feed_output(*prog_on ? "\x1b]9;4;1;72\x07" : "\x1b]9;4;0;0\x07");
                        }
                    },
                    Button {
                        .text = "查找",
                        .on_click = [ctrl]() { ctrl->toggle_find(); }
                    },
                    Button {
                        .text = "清屏",
                        .on_click = [ctrl]() { ctrl->clear(); }
                    }
                }
            },
            Terminal {
                .backend = std::make_shared<DemoShellBackend>(),
                .controller = ctrl,
                .color_scheme = TerminalColorScheme::campbell(),
                .cursor_style = TerminalCursorStyle::Bar,
                .font_size = 13.5f,
                .flex_grow = 1.0f,
                .on_title_changed = [status, ctrl](const std::string& t) {
                    status->set("标题: " + t + "  |  后端: " + ctrl->backend_name());
                },
                .on_exit = [status](int code) {
                    status->set("终端会话已退出 (exit code = " + std::to_string(code) + ")");
                }
            },
            app.make_accent(*status, 12.0f, FontWeight::Regular)
        }
    });
}

} // namespace gallery::pages

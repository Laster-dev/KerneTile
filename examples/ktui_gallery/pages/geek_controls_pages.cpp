#include "gallery_pages.hpp"
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

class GeekRefCaptureWidget : public Widget {
public:
    GeekRefCaptureWidget(WidgetPtr inner, std::shared_ptr<std::weak_ptr<RenderBox>> slot)
        : m_inner(std::move(inner)), m_slot(std::move(slot)) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = m_inner ? m_inner->create_render_node() : nullptr;
        if (m_inner && node) m_inner->set_render_node(node);
        if (m_slot) *m_slot = node;
        return node;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (m_slot) *m_slot = node;
        if (m_inner) m_inner->update_render_node(std::move(node));
    }
private:
    WidgetPtr m_inner;
    std::shared_ptr<std::weak_ptr<RenderBox>> m_slot;
};

WidgetPtr capture_box(WidgetPtr w, std::shared_ptr<std::weak_ptr<RenderBox>> slot) {
    return make_widget<GeekRefCaptureWidget>(std::move(w), std::move(slot));
}

} // namespace

// ============================================================================
// 1. 紧凑型键值对 / 属性检查器网格 (InspectorGrid)
// ============================================================================
WidgetPtr build_inspector_grid_page(GalleryApp& app) {
    auto status = std::make_shared<Property<std::string>>("bool 行直接复用原生 CheckBox，int/float 行点击弹出原生 NumberBox，str 行复用原生 TextBox；悬停可复制 ⧉ 或恢复默认 ↺。");
    auto ref = std::make_shared<std::weak_ptr<RenderBox>>();

    std::vector<InspectorRow> rows = {
        InspectorRow{"renderer.backend",     "D3D12 Compute", "D3D12 Compute", "str",  false},
        InspectorRow{"tile.size_px",         "16",            "16",            "int",  false},
        InspectorRow{"frames_in_flight",     "2",             "2",             "int",  false},
        InspectorRow{"atlas.size",           "1024",          "2048",          "int",  false},
        InspectorRow{"vsync.native_pacing",  "true",          "true",          "bool", false},
        InspectorRow{"occlusion.culling",    "true",          "false",         "bool", false},
        InspectorRow{"debug.base_addr",      "0x00007FF6A100","0x00007FF6A100","hex",  true},
        InspectorRow{"font.stem_darkening",  "0.18",          "0.15",          "float",false},
        InspectorRow{"mica.backdrop",        "true",          "true",          "bool", false},
        InspectorRow{"env.log_level",        "warn",          "info",          "str",  false},
        InspectorRow{"gpu.max_tiles_x",      "240",           "240",           "int",  false},
        InspectorRow{"gpu.max_tiles_y",      "135",           "135",           "int",  false},
        InspectorRow{"text.atlas_padding",   "2",             "2",             "int",  false},
        InspectorRow{"shadow.blur_passes",   "3",             "2",             "int",  false},
        InspectorRow{"input.high_dpi_scale", "1.50",          "1.00",          "float",false},
        InspectorRow{"perf.frame_budget_ms", "8.33",          "8.33",          "float",false},
        InspectorRow{"telemetry.enabled",    "false",         "false",         "bool", false},
        InspectorRow{"cache.shader_dir",     "build/shaders", "build/shaders", "str",  false}
    };

    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("InspectorGrid (紧凑型属性检查器网格)", 24.0f),
            app.make_muted("工程参数表高密度双列布局：按数据类型直接复用原生 CheckBox、NumberBox 与 TextBox，支持拖拽中线实时调节列宽、非默认值高亮与一键复制。", 13.0f),
            Expanded {
                .flex = 1.0f,
                .child = capture_box(InspectorGrid {
                    .rows = rows,
                    .key_column_ratio = 0.42f,
                    .on_value_changed = [status, ref](size_t idx, const std::string& val) {
                        std::string k = "#" + std::to_string(idx);
                        if (auto b = ref->lock()) {
                            auto* g = static_cast<RenderInspectorGrid*>(b.get());
                            if (idx < g->rows().size()) k = g->rows()[idx].key;
                        }
                        status->set("已更新参数 [" + k + "] = " + val);
                    }
                }, ref)
            },
            HStack {
                .justify = JustifyContent::SpaceBetween,
                .align_items = AlignItems::Center,
                .content = {
                    app.make_accent(*status, 12.0f, FontWeight::Medium),
                    Button {
                        .text = "全部恢复默认",
                        .on_click = [ref, status]() {
                            if (auto b = ref->lock()) static_cast<RenderInspectorGrid*>(b.get())->reset_all();
                            status->set("已将全部修改项恢复为默认值");
                        }
                    }
                }
            }
        }
    });
}

// ============================================================================
// 2. 十六进制 / 字节流查看与编辑格 (HexViewer)
// ============================================================================
WidgetPtr build_hex_viewer_page(GalleryApp& app) {
    auto status = std::make_shared<Property<std::string>>("支持按住鼠标左键拖动连续选择字节（Hex ↔ ASCII 双向联动高亮），宽高与列数随窗口自适应铺满，选中后可按 0-9 / A-F 编辑。");
    std::vector<uint8_t> sample = {
        0x7F, 0x45, 0x4C, 0x46, 0x02, 0x01, 0x01, 0x00, 0x4B, 0x65, 0x72, 0x6E, 0x65, 0x54, 0x69, 0x6C,
        0x65, 0x20, 0x4B, 0x54, 0x55, 0x49, 0x20, 0x32, 0x30, 0x32, 0x36, 0x00, 0x00, 0x80, 0x3F, 0x40,
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE, 0x10, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x47, 0x50, 0x55, 0x20, 0x55, 0x49, 0x21, 0x0A, 0x00
    };
    const char* payload = "KerneTile-D3D12-Compute-TileBinning-16x16-Fluent2-Mica-Atlas1024-ThinLTO-ZeroExceptions-";
    sample.reserve(4096);
    for (size_t i = 0; sample.size() < 4096; ++i) {
        if ((i % 32) < 24) sample.push_back(static_cast<uint8_t>(payload[i % 88]));
        else sample.push_back(static_cast<uint8_t>((i * 37 + 0xA5) & 0xFF));
    }

    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("HexViewer (十六进制字节流查看与编辑格)", 24.0f),
            app.make_muted("三栏等宽矩阵（Offset + 自适应 8/16/24/32 字节 Hex + ASCII），支持按住拖动连续选择、Hex ↔ ASCII 双向联动高亮、就地半字节编辑与 LE/BE 解码。", 13.0f),
            Expanded {
                .flex = 1.0f,
                .child = HexViewer {
                    .bytes = sample,
                    .editable = true,
                    .big_endian = false,
                    .on_byte_edited = [status](size_t off, uint8_t val) {
                        char buf[80];
                        std::snprintf(buf, sizeof(buf), "已修改偏移 0x%04X -> 0x%02X (%u)", static_cast<unsigned>(off), static_cast<unsigned>(val), static_cast<unsigned>(val));
                        status->set(buf);
                    }
                }
            },
            app.make_accent(*status, 12.0f, FontWeight::Medium)
        }
    });
}

// ============================================================================
// 4. 语法感知过滤输入框 (SyntaxFilterBar)
// ============================================================================
WidgetPtr build_syntax_filter_page(GalleryApp& app) {
    auto diag_text = std::make_shared<Property<std::string>>("✓ 语法合法，表达式可直接应用");
    auto ref = std::make_shared<std::weak_ptr<RenderBox>>();

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("SyntaxFilterBar (语法感知过滤输入框)", 24.0f),
            app.make_muted("直接继承单一 RenderTextBox：Wireshark 风格底色状态灯（合法浅绿 / 警告浅黄 / 语法错误浅红），支持字段名 Tab 自动补全。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    capture_box(SyntaxFilterBar {
                        .text = "ip == 10.0.0.1 && port > 1024",
                        .on_evaluated = [diag_text](const std::string&, SyntaxFilterState, const std::string& diag) {
                            diag_text->set(diag);
                        }
                    }, ref),
                    app.make_accent(*diag_text, 12.5f, FontWeight::Medium),
                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "合法示例 (绿)",
                                .on_click = [ref]() {
                                    if (auto b = ref->lock()) static_cast<RenderSyntaxFilterBar*>(b.get())->set_expression("ip == 10.0.0.1 && port >= 443");
                                }
                            },
                            Button {
                                .text = "警告示例 (黄)",
                                .on_click = [ref]() {
                                    if (auto b = ref->lock()) static_cast<RenderSyntaxFilterBar*>(b.get())->set_expression("custom_tag == prod && port > 80");
                                }
                            },
                            Button {
                                .text = "错误示例 (红)",
                                .on_click = [ref]() {
                                    if (auto b = ref->lock()) static_cast<RenderSyntaxFilterBar*>(b.get())->set_expression("(ip == 10.0.0.1 &&");
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

// ============================================================================
// 5. 结构化分段式状态栏 (StatusBar)
// ============================================================================
WidgetPtr build_status_bar_page(GalleryApp& app) {
    auto status = std::make_shared<Property<std::string>>("点击状态栏右侧的 UTF-8 / LF / 1000ms 槽位，直接弹出 MenuFlyout 切换配置！");
    auto ref = std::make_shared<std::weak_ptr<RenderBox>>();
    auto sel_cnt = std::make_shared<int>(3);

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("StatusBar (结构化分段式状态栏)", 24.0f),
            app.make_muted("经典 24~26px 工程底部状态栏：左侧常驻选中统计与偏移信息，右侧放编码、换行符、刷新频率等可点击切换槽位（复用 MenuFlyout）。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 14.0f,
                .content = {
                    capture_box(StatusBar {
                        .slots = {
                            StatusBarSlot{"conn",   "已连接 D3D12",         false, Color{0.10f, 0.72f, 0.45f, 1.0f}},
                            StatusBarSlot{"stats",  "已选 3 项 / 共 14,208 项", false},
                            StatusBarSlot{"offset", "偏移 0x004F",          false},
                            StatusBarSlot{"enc",    "UTF-8",                true,  {}, {"UTF-8", "UTF-16 LE", "GBK", "ASCII"},
                                [status](const std::string& v) { status->set("已切换编码格式: " + v); }},
                            StatusBarSlot{"eol",    "LF",                   true,  {}, {"LF", "CRLF", "CR"},
                                [status](const std::string& v) { status->set("已切换换行符: " + v); }},
                            StatusBarSlot{"rate",   "1000ms",               true,  {}, {"100ms", "500ms", "1000ms", "5000ms"},
                                [status](const std::string& v) { status->set("已切换刷新频率: " + v); }}
                        }
                    }, ref),
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            app.make_accent(*status, 12.5f, FontWeight::Medium),
                            Button {
                                .text = "模拟选中 +1",
                                .on_click = [ref, sel_cnt]() {
                                    ++(*sel_cnt);
                                    if (auto b = ref->lock()) {
                                        static_cast<RenderStatusBar*>(b.get())->set_slot_text(
                                            "stats", "已选 " + std::to_string(*sel_cnt) + " 项 / 共 14,208 项");
                                    }
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

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

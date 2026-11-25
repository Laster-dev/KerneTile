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

} // namespace gallery::pages

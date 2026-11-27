#include "../gallery_pages.hpp"
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

} // namespace gallery::pages

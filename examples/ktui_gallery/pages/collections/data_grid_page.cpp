#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_data_grid_page(GalleryApp& app) {
    static const std::vector<DataGridColumn> kCols = {
        DataGridColumn{ .title = "ID", .width = 76.0f, .sortable = true, .numeric = true },
        DataGridColumn{ .title = "模块名称", .width = 180.0f, .sortable = true, .numeric = false },
        DataGridColumn{ .title = "渲染后端", .width = 140.0f, .sortable = true, .numeric = false },
        DataGridColumn{ .title = "耗时 (ms)", .width = 100.0f, .sortable = true, .numeric = true },
        DataGridColumn{ .title = "状态", .width = 0.0f, .sortable = true, .numeric = false }
    };

    static const std::vector<std::vector<std::string>> kRows = []() {
        static constexpr const char* kMods[] = {"TileBinning", "GaussianShadow", "MicaBackdrop", "ThemeRipple", "NanoSVG", "FlexLayout", "TextShaper", "Compositor"};
        static constexpr const char* kBackends[] = {"D3D12 Compute", "Vulkan Compute", "CPU AVX2", "WebGPU / WASM"};
        static constexpr const char* kStatus[] = {"120 FPS 就绪", "GPU 加速", "缓存命中", "活跃"};
        std::vector<std::vector<std::string>> r;
        r.reserve(5000);
        for (int i = 0; i < 5000; ++i) {
            char ms_buf[16];
            std::snprintf(ms_buf, sizeof(ms_buf), "%.2f", 0.15 + static_cast<double>((i * 37) % 450) / 100.0);
            r.push_back({
                std::to_string(i + 1),
                std::string(kMods[i % 8]) + " #" + std::to_string((i / 8) + 1),
                kBackends[i % 4],
                ms_buf,
                kStatus[i % 4]
            });
        }
        return r;
    }();

    auto dg_overflow = std::make_shared<Property<DataGridCellOverflow>>(DataGridCellOverflow::Ellipsis);

    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("DataGrid", 22.0f),
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    Expanded {
                        .flex = 1.0f,
                        .child = app.make_muted("高性能虚拟化数据表格：支持拖拽调列宽、平滑行选中、Win32 拉框多选 (Ctrl/Shift) 及省略号/裁剪溢出模式切换。", 12.5f)
                    },
                    SegmentedControl {
                        .items = {
                            SegmentedItem{ .label = "省略号 (...)" },
                            SegmentedItem{ .label = "直接裁剪" }
                        },
                        .selected_index = 0,
                        .on_selected_index_changed = [dg_overflow](int idx) {
                            dg_overflow->set(idx == 0 ? DataGridCellOverflow::Ellipsis : DataGridCellOverflow::Clip);
                        }
                    }
                }
            },
            Expanded {
                .flex = 1.0f,
                .child = DataGrid {
                    .columns = kCols,
                    .rows = kRows,
                    .row_height = 34.0f,
                    .height = 0.0f,
                    .on_row_selected = [&app](size_t orig_idx) {
                        app.datagrid_status.set("已选中原始行 #" + std::to_string(orig_idx + 1) +
                                                " (" + kRows[orig_idx][1] + " · " + kRows[orig_idx][3] + " ms)");
                    }
                }.bind(&RenderDataGrid::set_cell_overflow, *dg_overflow)
            },
            app.make_accent(app.datagrid_status, 12.5f)
        }
    });
}

} // namespace gallery::pages

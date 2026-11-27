#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_tree_view_page(GalleryApp& app) {
    static constexpr const char* kFolderSvg =
        "M3 6.75A2.75 2.75 0 0 1 5.75 4h3.58c.7 0 1.37.28 1.86.78l1.03 1.03c.2.2.48.31.76.31h5.27A2.75 2.75 0 0 1 21 8.87v8.38A2.75 2.75 0 0 1 18.25 20H5.75A2.75 2.75 0 0 1 3 17.25V6.75z";
    static constexpr const char* kFileSvg =
        "M6.75 3h6.5L19 8.75v10.5A1.75 1.75 0 0 1 17.25 21H6.75A1.75 1.75 0 0 1 5 19.25V4.75C5 3.78 5.78 3 6.75 3zm5.75 1.5v4.25h4.25L12.5 4.5z";

    std::vector<TreeViewNode> project_tree = {
        TreeViewNode{
            .id = "root_kt", .label = "KerneTile", .svg_path = kFolderSvg, .is_expanded = true,
            .children = {
                TreeViewNode{
                    .id = "src_ktui", .label = "KTUI", .svg_path = kFolderSvg, .is_expanded = true,
                    .children = {
                        TreeViewNode{ .id = "f_auto", .label = "auto_suggest_box.hpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "f_tree", .label = "tree_view.hpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "f_cal",  .label = "calendar_date_picker.hpp", .svg_path = kFileSvg }
                    }
                },
                TreeViewNode{
                    .id = "src_hal", .label = "HAL 后端", .svg_path = kFolderSvg, .is_expanded = false,
                    .children = {
                        TreeViewNode{ .id = "hal_d3d12",  .label = "d3d12_backend.cpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "hal_vulkan", .label = "vulkan_backend.cpp", .svg_path = kFileSvg },
                        TreeViewNode{ .id = "hal_cpu",    .label = "cpu_rasterizer.cpp", .svg_path = kFileSvg }
                    }
                },
                TreeViewNode{ .id = "f_cmake", .label = "CMakeLists.txt", .svg_path = kFileSvg }
            }
        }
    };

    std::vector<TreeViewNode> feature_tree = {
        TreeViewNode{
            .id = "pkg_core", .label = "渲染与合成核心", .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "feat_tile", .label = "16×16 瓦片分箱引擎", .is_checked = true },
                TreeViewNode{ .id = "feat_mica", .label = "云母动态聚焦背景", .is_checked = true },
                TreeViewNode{ .id = "feat_hdr",  .label = "广色域 HDR 合成 (实验)", .is_checked = false }
            }
        },
        TreeViewNode{
            .id = "pkg_platform", .label = "跨平台目标", .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "plat_win", .label = "Windows 11 (Win32 + D3D12)", .is_checked = true },
                TreeViewNode{ .id = "plat_lin", .label = "Linux (X11 + XInput2)", .is_checked = false },
                TreeViewNode{ .id = "plat_web", .label = "WebOS Desktop (WASM)", .is_checked = false }
            }
        }
    };

    std::vector<TreeViewNode> tree_list_nodes = {
        TreeViewNode{
            .id = "tl_engine", .label = "KerneTile.Core", .svg_path = kFolderSvg, .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "tl_bin", .label = "tile_binner.cpp", .svg_path = kFileSvg, .sub_columns = {"C++20", "18.4 KB", "0.12 ms"} },
                TreeViewNode{ .id = "tl_sdf", .label = "sdf_rasterizer.hlsl", .svg_path = kFileSvg, .sub_columns = {"HLSL SM6", "11.2 KB", "0.28 ms"} },
                TreeViewNode{ .id = "tl_atl", .label = "dynamic_atlas.cpp", .svg_path = kFileSvg, .sub_columns = {"C++20", "9.6 KB", "0.04 ms"} }
            },
            .sub_columns = {"模块", "39.2 KB", "0.44 ms"}
        },
        TreeViewNode{
            .id = "tl_ui", .label = "KTUI.Widgets", .svg_path = kFolderSvg, .is_expanded = true,
            .children = {
                TreeViewNode{ .id = "tl_tv", .label = "tree_view.hpp", .svg_path = kFileSvg, .sub_columns = {"Header", "22.1 KB", "120 FPS"} },
                TreeViewNode{ .id = "tl_dg", .label = "data_grid.hpp", .svg_path = kFileSvg, .sub_columns = {"Header", "19.8 KB", "120 FPS"} }
            },
            .sub_columns = {"模块", "41.9 KB", "120 FPS"}
        }
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TreeView", 22.0f),
            app.make_muted("WinUI 3 层级树形视图：平滑展开/折叠高度与箭头旋转动画、单选/多选三态联动、以及多列可调宽 TreeListView 模式。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "多列树形表格 (TreeListView，支持表头拖拽调列宽 + 平滑折叠)", .font_size = 14.0f },
                    TreeView {
                        .nodes = std::move(tree_list_nodes),
                        .selection_mode = TreeViewSelectionMode::Single,
                        .selected_id = "tl_bin",
                        .height = 240.0f,
                        .columns = {
                            TreeListColumn{ .title = "名称", .width = 240.0f },
                            TreeListColumn{ .title = "类型", .width = 110.0f },
                            TreeListColumn{ .title = "体积", .width = 100.0f },
                            TreeListColumn{ .title = "耗时 / 帧率", .width = 0.0f }
                        }
                    }
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "单选文件目录树 (SelectionMode::Single)", .font_size = 14.0f },
                    TreeView {
                        .nodes = std::move(project_tree),
                        .selection_mode = TreeViewSelectionMode::Single,
                        .height = 220.0f,
                        .on_selection_changed = [&app](const std::string& id, const std::string& label) {
                            app.tree_single_status.set("当前选中节点: " + label + " (" + id + ")");
                        }
                    }.bind_two_way(app.tree_single_selected),
                    app.make_accent(app.tree_single_status, 12.5f)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "多选三态勾选树 (SelectionMode::Multiple)", .font_size = 14.0f },
                    TreeView {
                        .nodes = std::move(feature_tree),
                        .selection_mode = TreeViewSelectionMode::Multiple,
                        .height = 220.0f,
                        .on_checked_changed = [&app](const std::vector<std::string>& ids) {
                            app.tree_multi_status.set("当前已勾选 " + std::to_string(ids.size()) + " 项");
                        }
                    },
                    app.make_accent(app.tree_multi_status, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

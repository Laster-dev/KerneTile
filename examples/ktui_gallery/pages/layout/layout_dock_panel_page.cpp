#include "../gallery_pages.hpp"
#include <cmath>

using namespace ktui;

namespace gallery::pages {

// ==================== 7. Visual Studio + Fluent 2 可拖拽 DockPanel 独立展示页 ====================
WidgetPtr build_layout_dock_panel_page(GalleryApp& app) {
    auto zone_name = [](DockZone z) -> const char* {
        switch (z) {
            case DockZone::Left:   return "左侧 (Left)";
            case DockZone::Right:  return "右侧 (Right)";
            case DockZone::Top:    return "顶部 (Top)";
            case DockZone::Bottom: return "底部 (Bottom)";
            case DockZone::Center: return "中央工作区 (Center)";
        }
        return "";
    };

    auto make_pane_body = [&app](const std::string& heading, const std::string& line1, const std::string& line2) -> WidgetPtr {
        return Box {
            .padding = EdgeInsets(10.0f, 12.0f),
            .child = VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 6.0f,
                .content = {
                    Text { .text = heading, .font_size = 13.0f, .font_weight = FontWeight::SemiBold },
                    Text { .text = line1, .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text),
                    Text { .text = line2, .font_size = 12.0f }.bind(&RenderText::set_color, app.muted_text)
                }
            }
        };
    };

    WidgetPtr dock_widget = DockPanel {
        .panes = {
            DockPane {
                .id = "explorer",
                .title = "解决方案",
                .zone = DockZone::Left,
                .content = make_pane_body("KerneTile.sln", "├─ renderer.cpp", "└─ dock_panel.hpp")
            },
            DockPane {
                .id = "editor",
                .title = "renderer.cpp",
                .zone = DockZone::Center,
                .content = make_pane_body("主代码编辑区", "按住标题栏拖到中心五向指南针即可停靠。", "拖到另一面板标题栏可合并为多标签页。")
            },
            DockPane {
                .id = "preview",
                .title = "设计视图",
                .zone = DockZone::Center,
                .content = make_pane_body("可视化设计器 (同区标签 2)", "点击顶部标签切换，或拖出拆分停靠。", "拖拽相邻分隔条可实时调节面板宽高。"),
                .closable = true
            },
            DockPane {
                .id = "inspector",
                .title = "属性",
                .zone = DockZone::Right,
                .content = make_pane_body("属性检查器", "Theme: Fluent 2", "FPS: VSync"),
                .closable = true
            },
            DockPane {
                .id = "output",
                .title = "输出终端",
                .zone = DockZone::Bottom,
                .content = make_pane_body("构建输出", "[64/64] Linking ktui_gallery.exe", "100% tests passed (36/36)"),
                .closable = true
            }
        },
        .left_width = 185.0f,
        .right_width = 185.0f,
        .top_height = 115.0f,
        .bottom_height = 115.0f,
        .allow_detach_window = true,
        .on_dock_changed = [&app, zone_name](const std::string& id, DockZone z) {
            app.layout_dock_status.set("已将面板 [" + id + "] 停靠至: " + zone_name(z));
        },
        .on_detach_changed = [&app](const std::string& id, bool detached) {
            app.layout_dock_status.set(detached
                ? ("已将面板 [" + id + "] 撕出为独立窗口 (状态完整保留，关闭窗口或点顶部按钮自动停靠回原位)")
                : ("面板 [" + id + "] 已停靠回主工作区"));
        }
    };

    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            HStack {
                .align_items = AlignItems::Center,
                .spacing = 12.0f,
                .content = {
                    VStack {
                        .spacing = 4.0f,
                        .content = {
                            app.make_page_title("DockPanel (Visual Studio + Fluent 2 拖拽停靠)", 22.0f),
                            app.make_accent(app.layout_dock_status, 12.5f)
                        }
                    },
                    Spacer {},
                    Button {
                        .text = "重置默认停靠",
                        .variant = ButtonVariant::Accent,
                        .on_click = [&app, dock_widget]() {
                            if (auto rn = dock_widget->render_node()) {
                                static_cast<RenderDockPanel*>(rn.get())->reset_layout();
                                app.layout_dock_status.set("已恢复默认五区停靠布局与面板尺寸");
                            }
                        }
                    }
                }
            },
            Expanded {
                .flex = 1.0f,
                .child = dock_widget
            }
        }
    });
}

} // namespace gallery::pages

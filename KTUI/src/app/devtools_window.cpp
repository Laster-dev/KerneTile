#include <ktui/app/ui_inspector.hpp>

#if KTUI_INSPECTOR_ACTIVE

#include <ktui/ktui.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ktui {

struct DevToolsWindowHost {
    std::shared_ptr<Window> win;
    Window* target{nullptr};

    // 响应式属性（直接驱动声明式 KTUI 控件更新）
    Property<std::string> fps_text{"-- FPS"};
    Property<std::string> perf_summary{"等待帧统计..."};
    Property<std::string> dirty_summary{"脏瓦片: 0 / 0 (100.0% 剔除)"};
    Property<std::string> pick_btn_text{"拾取元素 (Pick)"};
    Property<bool> show_dirty_tiles{true};
    Property<std::string> selected_id{""};
    Property<std::string> node_title{"未选中控件（点击树节点或点击“拾取元素”）"};
    Property<std::string> node_geom{"位置: -  |  尺寸: -"};
    Property<std::string> node_constraints{"约束: -"};

    // 树节点 ID -> 真实 RenderBox* 映射（刷新树时重建）
    std::unordered_map<std::string, RenderBox*> id_to_node;
    std::weak_ptr<RenderTreeView> tree_view_ref;
    std::chrono::steady_clock::time_point last_sync{};

    static std::string node_id_str(const RenderBox* box) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "0x%llx",
                      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(box)));
        return std::string(buf);
    }

    static void format_dim(char* buf, size_t buf_size, float v) noexcept {
        if (!std::isfinite(v) || v >= 1e20f) {
            std::snprintf(buf, buf_size, "inf");
        } else {
            std::snprintf(buf, buf_size, "%.0f", static_cast<double>(v));
        }
    }

    TreeViewNode build_tree_node(RenderBox* box, int depth) {
        TreeViewNode tn;
        tn.id = node_id_str(box);
        id_to_node[tn.id] = box;
        tn.label = std::string("<") + UiInspector::infer_node_tag(box) + ">";
        tn.is_expanded = (depth < 3);

        char sz_buf[32];
        Vec2 sz = box->size();
        std::snprintf(sz_buf, sizeof(sz_buf), "%.0fx%.0f",
                      static_cast<double>(sz.x), static_cast<double>(sz.y));
        tn.sub_columns.push_back(sz_buf);
        tn.sub_columns.push_back(std::to_string(box->child_count()));

        for (RenderNode* c = box->first_child(); c != nullptr; c = c->next_sibling()) {
            if (auto* cbox = c->as_render_box()) {
                tn.children.push_back(build_tree_node(cbox, depth + 1));
            }
        }
        return tn;
    }

    void rebuild_tree() {
        id_to_node.clear();
        std::vector<TreeViewNode> roots;
        if (target && target->root_render_box()) {
            roots.push_back(build_tree_node(target->root_render_box(), 0));
        }
        if (auto tv = tree_view_ref.lock()) {
            tv->set_nodes(std::move(roots));
            if (target && target->inspector_state().selected_node) {
                std::string sid = node_id_str(target->inspector_state().selected_node);
                selected_id.set(sid);
                tv->set_selected_id_value(sid);
            }
        }
    }

    void update_node_details(RenderBox* box) {
        if (!box) {
            node_title.set("未选中控件（点击树节点或点击“拾取元素”）");
            node_geom.set("位置: -  |  尺寸: -");
            node_constraints.set("约束: -");
            return;
        }
        char buf[192];
        std::snprintf(buf, sizeof(buf), "<%s>  ·  子节点: %zu  %s%s%s",
                      UiInspector::infer_node_tag(box),
                      box->child_count(),
                      box->is_relayout_boundary() ? "[Boundary] " : "",
                      box->is_focusable ? "[Focusable] " : "",
                      box->is_active ? "[Active]" : "");
        node_title.set(buf);

        Vec2 gpos = box->global_offset();
        Vec2 loff = box->offset();
        Vec2 bsz = box->size();
        std::snprintf(buf, sizeof(buf),
                      "全局: (%.0f, %.0f)  |  局部: (%.0f, %.0f)  |  尺寸: %.0f x %.0f",
                      static_cast<double>(gpos.x), static_cast<double>(gpos.y),
                      static_cast<double>(loff.x), static_cast<double>(loff.y),
                      static_cast<double>(bsz.x), static_cast<double>(bsz.y));
        node_geom.set(buf);

        const auto& c = box->constraints();
        char min_w[16], max_w[16], min_h[16], max_h[16];
        format_dim(min_w, sizeof(min_w), c.min_width);
        format_dim(max_w, sizeof(max_w), c.max_width);
        format_dim(min_h, sizeof(min_h), c.min_height);
        format_dim(max_h, sizeof(max_h), c.max_height);
        std::snprintf(buf, sizeof(buf), "BoxConstraints: W[%s .. %s], H[%s .. %s]",
                      min_w, max_w, min_h, max_h);
        node_constraints.set(buf);
    }

    void sync_stats(bool force) {
        if (!target || !win || win->is_closed()) return;
        const auto now = std::chrono::steady_clock::now();
        if (!force && last_sync.time_since_epoch().count() != 0) {
            if (std::chrono::duration<float>(now - last_sync).count() < 0.08f) return;
        }
        last_sync = now;

        auto& st = target->inspector_state();
        const auto& fs = st.prev_stats;
        char buf[192];

        if (st.fps > 0.1f) {
            std::snprintf(buf, sizeof(buf), "%.1f FPS", static_cast<double>(st.fps));
        } else {
            std::snprintf(buf, sizeof(buf), "0.0 FPS (Idle)");
        }
        fps_text.set(buf);

        const uint32_t total_nodes = UiInspector::count_nodes(target->root_render_box());
        std::snprintf(buf, sizeof(buf),
                      "%s  ·  Frame #%u  ·  图元 %u  ·  节点 %u",
                      st.prev_hw_accel ? "GPU Compute" : "CPU Tile",
                      fs.frame_index, fs.total_primitives, total_nodes);
        perf_summary.set(buf);

        std::snprintf(buf, sizeof(buf),
                      "脏瓦片 %u/%u (%.1f%% 剔除)  ·  热力块 %zu  ·  分箱 %.2fms  ·  渲染 %.2fms",
                      fs.dirty_tiles, fs.total_tiles, fs.culling_ratio,
                      st.tile_heats.size(),
                      fs.cpu_binning_time_us / 1000.0,
                      fs.render_time_us / 1000.0);
        dirty_summary.set(buf);

        pick_btn_text.set(st.pick_mode ? "点击目标画布拾取中..." : "拾取元素 (Pick)");
        if (show_dirty_tiles.get() != st.show_dirty_tiles) {
            show_dirty_tiles.set(st.show_dirty_tiles);
        }

        RenderBox* active_box = (st.pick_mode && st.hovered_node) ? st.hovered_node : st.selected_node;
        update_node_details(active_box);
        if (st.selected_node) {
            std::string sid = node_id_str(st.selected_node);
            if (selected_id.get() != sid) {
                selected_id.set(sid);
                if (auto tv = tree_view_ref.lock()) {
                    tv->set_selected_id_value(sid);
                }
            }
        }
        win->request_redraw();
    }
};

void UiInspector::toggle_devtools_window(Window* target) {
    if (!target) return;
    auto& st = target->inspector_state();

    // 若在 DevTools 窗口内按 F12，直接关闭 DevTools 窗口本身
    if (st.is_devtools_window) {
        target->close();
        return;
    }

    if (st.enabled && st.devtools_host && st.devtools_host->win && !st.devtools_host->win->is_closed()) {
        close_devtools_window(target);
        return;
    }

    Application* app = Application::current();
    if (!app) {
        st.enabled = !st.enabled;
        if (!st.enabled) st.tile_heats.clear();
        return;
    }

    auto host = std::make_shared<DevToolsWindowHost>();
    host->target = target;
    host->show_dirty_tiles.set(st.show_dirty_tiles);

    WindowDesc desc;
    desc.title = "KTUI DevTools — " + target->title();
    desc.size = Vec2{460.0f, 680.0f};
    desc.resizable = true;
    auto dt_win = app->create_window(desc);
    if (!dt_win) return;

    // 自动停靠定位到目标窗口右侧（若右侧空间不足则偏移显示）
    Vec2 tpos = target->position();
    Vec2 tsz = target->size();
    dt_win->set_position(Vec2{tpos.x + tsz.x + 12.0f, tpos.y});
    dt_win->inspector_state().is_devtools_window = true;
    host->win = dt_win;

    st.enabled = true;
    st.devtools_host = host;

    std::weak_ptr<DevToolsWindowHost> weak_host = host;

    // 监听 DevTools 窗口关闭事件 -> 自动清理目标窗口覆盖层
    dt_win->on_close([weak_host]() -> bool {
        if (auto h = weak_host.lock()) {
            if (h->target) {
                auto& tst = h->target->inspector_state();
                tst.enabled = false;
                tst.pick_mode = false;
                tst.selected_node = nullptr;
                tst.hovered_node = nullptr;
                tst.tile_heats.clear();
                tst.devtools_host.reset();
                h->target->request_redraw();
            }
        }
        return true;
    });

    // 构建初始 TreeView 节点快照
    std::vector<TreeViewNode> initial_roots;
    if (target->root_render_box()) {
        initial_roots.push_back(host->build_tree_node(target->root_render_box(), 0));
    }

    // 使用 100% 声明式 KTUI 控件组装独立 DevTools 界面
    WidgetPtr tree_widget = TreeView {
        .nodes = std::move(initial_roots),
        .selection_mode = TreeViewSelectionMode::Single,
        .on_selection_changed = [weak_host](const std::string& id, const std::string&) {
            if (auto h = weak_host.lock()) {
                auto it = h->id_to_node.find(id);
                if (h->target && it != h->id_to_node.end()) {
                    h->target->inspector_state().selected_node = it->second;
                    h->update_node_details(it->second);
                    h->target->request_redraw();
                }
            }
        },
        .columns = {
            TreeListColumn{ .title = "控件树 (Elements)", .width = 0.0f },
            TreeListColumn{ .title = "尺寸", .width = 92.0f },
            TreeListColumn{ .title = "子项", .width = 52.0f }
        }
    }.bind_two_way(host->selected_id);

    WidgetPtr content = Box {
        .padding = EdgeInsets(12.0f),
        .child = VStack {
            .align_items = AlignItems::Stretch,
            .spacing = 10.0f,
            .content = {
                // 1. 顶部操作工具栏
                HStack {
                    .align_items = AlignItems::Center,
                    .spacing = 8.0f,
                    .content = {
                        Button {
                            .text = "拾取元素 (Pick)",
                            .variant = ButtonVariant::Accent,
                            .on_click = [weak_host]() {
                                if (auto h = weak_host.lock()) {
                                    if (h->target) {
                                        auto& tst = h->target->inspector_state();
                                        tst.pick_mode = !tst.pick_mode;
                                        h->pick_btn_text.set(tst.pick_mode ? "点击目标画布拾取中..." : "拾取元素 (Pick)");
                                        if (tst.pick_mode) h->target->focus();
                                        h->target->request_redraw();
                                    }
                                }
                            }
                        }.bind(&RenderButton::set_text, host->pick_btn_text),
                        Button {
                            .text = "刷新树",
                            .on_click = [weak_host]() {
                                if (auto h = weak_host.lock()) {
                                    h->rebuild_tree();
                                }
                            }
                        },
                        Spacer {},
                        ToggleSwitch {
                            .checked = host->show_dirty_tiles.get(),
                            .label = "脏区热力图",
                            .on_change = [weak_host](bool on) {
                                if (auto h = weak_host.lock()) {
                                    if (h->target) {
                                        auto& tst = h->target->inspector_state();
                                        tst.show_dirty_tiles = on;
                                        if (!on) tst.tile_heats.clear();
                                        h->target->request_redraw();
                                    }
                                }
                            }
                        }.bind_two_way(host->show_dirty_tiles)
                    }
                },

                // 2. 实时 FPS 与瓦片性能监测卡片
                Box {
                    .padding = EdgeInsets(10.0f),
                    .theme_card = true,
                    .child = VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 5.0f,
                        .content = {
                            HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 10.0f,
                                .content = {
                                    Text {
                                        .text = "-- FPS",
                                        .font_size = 18.0f,
                                        .color = Color{0.18f, 0.80f, 0.48f, 1.0f},
                                        .font_weight = FontWeight::SemiBold
                                    }.bind(&RenderText::set_text, host->fps_text),
                                    Expanded {
                                        .child = Text {
                                            .text = "等待帧统计...",
                                            .font_size = 12.0f
                                        }.bind(&RenderText::set_text, host->perf_summary)
                                    }
                                }
                            },
                            Text {
                                .text = "脏瓦片: 0 / 0",
                                .font_size = 11.5f
                            }.bind(&RenderText::set_text, host->dirty_summary),
                            HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
                                .content = {
                                    Text { .text = "热力图例:", .font_size = 11.0f },
                                    Box { .width = 12.0f, .height = 10.0f, .background_color = Color{0.10f, 0.86f, 0.48f, 0.90f}, .corner_radius = 2.0f },
                                    Text { .text = "单次刷新", .font_size = 11.0f },
                                    Box { .width = 12.0f, .height = 10.0f, .background_color = Color{1.0f, 0.65f, 0.10f, 0.92f}, .corner_radius = 2.0f },
                                    Text { .text = "中频", .font_size = 11.0f },
                                    Box { .width = 12.0f, .height = 10.0f, .background_color = Color{0.98f, 0.14f, 0.18f, 0.95f}, .corner_radius = 2.0f },
                                    Text { .text = "高频热点", .font_size = 11.0f }
                                }
                            }
                        }
                    }
                },

                // 3. Elements 多列控件树 (使用原生 KTUI TreeView)
                Expanded {
                    .child = Box {
                        .padding = EdgeInsets(4.0f),
                        .theme_card = true,
                        .child = tree_widget
                    }
                },

                // 4. 选中节点盒模型与约束详情卡片
                Box {
                    .padding = EdgeInsets(10.0f),
                    .theme_card = true,
                    .child = VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 4.0f,
                        .content = {
                            Text {
                                .text = "未选中控件",
                                .font_size = 12.5f,
                                .font_weight = FontWeight::SemiBold
                            }.bind(&RenderText::set_text, host->node_title),
                            Text {
                                .text = "位置: -  |  尺寸: -",
                                .font_size = 11.5f
                            }.bind(&RenderText::set_text, host->node_geom),
                            Text {
                                .text = "约束: -",
                                .font_size = 11.5f
                            }.bind(&RenderText::set_text, host->node_constraints)
                        }
                    }
                }
            }
        }
    };

    dt_win->set_content(content);
    if (auto tv_node = std::static_pointer_cast<RenderTreeView>(tree_widget->render_node())) {
        host->tree_view_ref = tv_node;
    }
    dt_win->show();
    host->sync_stats(true);
    target->request_redraw();
}

void UiInspector::sync_devtools_window(Window* target, bool force) {
    if (!target) return;
    auto& st = target->inspector_state();
    if (st.devtools_host) {
        st.devtools_host->sync_stats(force);
    }
}

void UiInspector::close_devtools_window(Window* target) {
    if (!target) return;
    auto& st = target->inspector_state();
    st.enabled = false;
    st.pick_mode = false;
    st.selected_node = nullptr;
    st.hovered_node = nullptr;
    st.tile_heats.clear();
    if (auto host = std::move(st.devtools_host)) {
        host->target = nullptr;
        if (host->win && !host->win->is_closed()) {
            host->win->close();
        }
    }
    target->request_redraw();
}

} // namespace ktui

#endif // KTUI_INSPECTOR_ACTIVE

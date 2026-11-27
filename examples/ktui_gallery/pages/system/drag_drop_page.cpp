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

// ==================== 窗口内拖拽演示专用看板节点 ====================
struct KanbanTask {
    std::string id;
    std::string title;
    std::string tag;
    Color accent;
    int column{0}; // 0: 待处理, 1: 进行中, 2: 已完成
};

struct KanbanBoardState {
    std::vector<KanbanTask> tasks;
    std::vector<std::string> logs;
    std::string dragging_id;
    bool needs_rebuild{false};
    Fn<void()> on_changed;

    void reset() {
        tasks = {
            {"task_atlas",  "RGBA8 彩色图集",   "Renderer", Color{0.00f, 0.56f, 0.92f, 1.0f}, 0},
            {"task_dialog", "原生文件对话框",   "PAL",      Color{0.52f, 0.36f, 0.90f, 1.0f}, 0},
            {"task_dnd",    "OS 文件拖放协议",  "Event",    Color{0.08f, 0.68f, 0.58f, 1.0f}, 1},
            {"task_gdi",    "Pure-GDI 光栅器",  "CPU",      Color{0.88f, 0.56f, 0.14f, 1.0f}, 2}
        };
        dragging_id.clear();
        logs.clear();
        push_log("看板已初始化：按住任意卡片拖动超过 5px 后松手投入目标列");
    }

    void push_log(const std::string& entry) {
        logs.push_back(entry);
        if (logs.size() > 5u) {
            logs.erase(logs.begin());
        }
    }

    std::string format_logs() const {
        std::string out;
        for (size_t i = 0; i < logs.size(); ++i) {
            if (i > 0) out += "\n";
            out += "• " + logs[i];
        }
        return out;
    }

    bool move_task(const std::string& id, int target_col) {
        for (auto& t : tasks) {
            if (t.id == id) {
                if (t.column == target_col) return false;
                t.column = target_col;
                return true;
            }
        }
        return false;
    }
};

// 可拖拽任务卡片 RenderBox (展示 is_draggable + on_drag_start + on_drag_end)
class RenderDraggableTaskCard : public RenderBox {
public:
    RenderDraggableTaskCard(std::shared_ptr<KanbanBoardState> state, KanbanTask task,
                            GalleryApp* app)
        : m_state(std::move(state)), m_task(std::move(task)), m_app(app) {
        is_draggable = true;
        cursor_shape = CursorShape::PointingHand;

        on_pointer_enter = [this](PointerEvent&) {
            m_hovered = true;
            mark_needs_paint();
        };
        on_pointer_exit = [this](PointerEvent&) {
            m_hovered = false;
            mark_needs_paint();
        };
        on_drag_start = [this](DragEvent& ev) {
            m_dragging = true;
            ev.accepted = true;
            ev.payload.mime_type = "application/x-ktui-kanban-task";
            ev.payload.data = m_task.id;
            ev.payload.source = this;
            if (m_state) {
                m_state->dragging_id = m_task.id;
                m_state->push_log("[DragStart] 卡片「" + m_task.title + "」 mime=" + ev.payload.mime_type);
                if (m_app) m_app->drag_event_log.set(m_state->format_logs());
            }
            mark_needs_paint();
        };
        on_drag_end = [this](DragEvent& ev) {
            m_dragging = false;
            auto state = m_state;
            auto* app_ptr = m_app;
            std::string title = m_task.title;
            mark_needs_paint();
            if (state) {
                state->dragging_id.clear();
                state->push_log(std::string("[DragEnd] 卡片「") + title + "」 " +
                                (ev.accepted ? "投放成功" : "已取消/未命中目标"));
                if (app_ptr) app_ptr->drag_event_log.set(state->format_logs());
                if (state->needs_rebuild) {
                    state->needs_rebuild = false;
                    if (state->on_changed) state->on_changed();
                }
            }
        };
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 200.0f;
        m_size = m_constraints.constrain(Vec2{std::max(140.0f, w), 54.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const Rect r = bounds();

        Color bg = theme.is_dark
            ? Color{1.0f, 1.0f, 1.0f, m_dragging ? 0.03f : (m_hovered ? 0.10f : 0.06f)}
            : Color{1.0f, 1.0f, 1.0f, m_dragging ? 0.45f : (m_hovered ? 0.96f : 0.82f)};
        Color border = m_dragging
            ? Color{m_task.accent.r, m_task.accent.g, m_task.accent.b, 0.85f}
            : (m_hovered ? Color{theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, 0.65f}
                         : (theme.is_dark ? Color{1, 1, 1, 0.10f} : Color{0, 0, 0, 0.10f}));

        ctx.draw_rect(r, bg, 6.0f, 0.0f);
        ctx.draw_rect(r, border, 6.0f, m_dragging ? 1.6f : 1.0f);

        // 左侧彩色指示条
        Rect stripe = Rect::from_xywh(r.min_x + 6.0f, r.min_y + 10.0f, 4.0f, r.height() - 20.0f);
        Color stripe_c = m_task.accent;
        if (m_dragging) stripe_c.a *= 0.45f;
        ctx.draw_rect(stripe, stripe_c, 2.0f, 0.0f);

        // 标题与标签
        Color title_c = theme.colors.text_primary;
        Color sub_c = theme.colors.text_secondary;
        if (m_dragging) {
            title_c.a *= 0.45f;
            sub_c.a *= 0.45f;
        }
        ctx.draw_text(m_task.title, PointF{r.min_x + 18.0f, r.min_y + 10.0f}, 13.0f, title_c, FontWeight::Medium);
        std::string subtitle = m_dragging ? "正在拖拽中..." : ("模块: " + m_task.tag + "  ·  按住拖拽");
        ctx.draw_text(subtitle, PointF{r.min_x + 18.0f, r.min_y + 30.0f}, 11.0f, sub_c, FontWeight::Regular);
    }

private:
    std::shared_ptr<KanbanBoardState> m_state;
    KanbanTask m_task;
    GalleryApp* m_app{nullptr};
    bool m_hovered{false};
    bool m_dragging{false};
};

// 投放区容器 RenderBox (展示 is_drop_target + on_drag_enter/over/leave/drop)
class RenderTaskDropColumn : public RenderBox {
public:
    RenderTaskDropColumn(int col_idx, std::string title, Color header_accent,
                         std::shared_ptr<KanbanBoardState> state, GalleryApp* app)
        : m_col_idx(col_idx), m_title(std::move(title)), m_accent(header_accent),
          m_state(std::move(state)), m_app(app) {
        is_drop_target = true;
        rebuild_children();

        on_drag_enter = [this](DragEvent& ev) {
            if (ev.payload.mime_type == "application/x-ktui-kanban-task") {
                m_drag_over = true;
                ev.accepted = true;
                if (m_state) {
                    m_state->push_log("[DragEnter] 进入列「" + m_title + "」");
                    if (m_app) m_app->drag_event_log.set(m_state->format_logs());
                }
                mark_needs_paint();
            }
        };
        on_drag_over = [this](DragEvent& ev) {
            if (ev.payload.mime_type == "application/x-ktui-kanban-task") {
                m_drag_over = true;
                ev.accepted = true;
            }
        };
        on_drag_leave = [this](DragEvent&) {
            if (m_drag_over) {
                m_drag_over = false;
                if (m_state) {
                    m_state->push_log("[DragLeave] 离开列「" + m_title + "」");
                    if (m_app) m_app->drag_event_log.set(m_state->format_logs());
                }
                mark_needs_paint();
            }
        };
        on_drop = [this](DragEvent& ev) {
            m_drag_over = false;
            if (ev.payload.mime_type == "application/x-ktui-kanban-task" && m_state) {
                ev.accepted = true;
                bool moved = m_state->move_task(ev.payload.data, m_col_idx);
                m_state->push_log("[Drop] 投放到「" + m_title + "」 data=" + ev.payload.data +
                                  " @(" + std::to_string(static_cast<int>(ev.local_position.x)) + "," +
                                  std::to_string(static_cast<int>(ev.local_position.y)) + ")" +
                                  (moved ? " [已移动]" : " [同列]"));
                if (m_app) m_app->drag_event_log.set(m_state->format_logs());
                if (moved) m_state->needs_rebuild = true;
            }
            mark_needs_paint();
        };
    }

    void rebuild_children() {
        for (auto& ch : m_cards) {
            if (ch && ch->parent() == this) {
                remove_child(ch.get());
            }
        }
        m_cards.clear();
        if (!m_state) return;
        for (const auto& t : m_state->tasks) {
            if (t.column == m_col_idx) {
                auto card = make_render_node<RenderDraggableTaskCard>(m_state, t, m_app);
                append_child(card.get());
                m_cards.push_back(std::move(card));
            }
        }
        mark_needs_layout();
        mark_needs_paint();
    }

    ~RenderTaskDropColumn() override {
        for (auto& ch : m_cards) {
            if (ch && ch->parent() == this) {
                remove_child(ch.get());
            }
        }
        m_cards.clear();
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 220.0f;
        float pad = 10.0f;
        float header_h = 34.0f;
        float y = header_h + 6.0f;
        float child_w = std::max(120.0f, w - pad * 2.0f);

        for (auto& ch : m_cards) {
            ch->layout(BoxConstraints{child_w, child_w, 0.0f, 120.0f});
            ch->set_offset(Vec2{pad, y});
            y += ch->size().y + 8.0f;
        }
        float min_h = 228.0f;
        float total_h = std::max(min_h, y + pad);
        m_size = m_constraints.constrain(Vec2{w, total_h});
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const Rect r = bounds();

        Color bg = m_drag_over
            ? Color{theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, theme.is_dark ? 0.16f : 0.10f}
            : (theme.is_dark ? Color{0.0f, 0.0f, 0.0f, 0.18f} : Color{0.0f, 0.0f, 0.0f, 0.035f});
        Color border = m_drag_over
            ? theme.colors.primary
            : (theme.is_dark ? Color{1, 1, 1, 0.09f} : Color{0, 0, 0, 0.09f});

        ctx.draw_rect(r, bg, 8.0f, 0.0f);
        ctx.draw_rect(r, border, 8.0f, m_drag_over ? 2.0f : 1.0f);

        // 列标题栏 + 数量圆点
        ctx.draw_rect(Rect::from_xywh(r.min_x + 12.0f, r.min_y + 13.0f, 8.0f, 8.0f), m_accent, 4.0f, 0.0f);
        std::string header = m_title + " (" + std::to_string(m_cards.size()) + ")";
        ctx.draw_text(header, PointF{r.min_x + 26.0f, r.min_y + 9.0f}, 13.0f,
                      theme.colors.text_primary, FontWeight::SemiBold);

        if (m_drag_over) {
            ctx.draw_text("松手投放至此列", PointF{r.max_x - 98.0f, r.min_y + 10.0f}, 11.0f,
                          theme.colors.primary, FontWeight::Medium);
        }

        // 子卡片
        RenderBox::paint(ctx);

        if (m_cards.empty()) {
            ctx.draw_text("暂无卡片，可拖拽至此处", PointF{r.min_x + 18.0f, r.min_y + 96.0f},
                          12.0f, theme.colors.text_secondary, FontWeight::Regular);
        }
    }

private:
    int m_col_idx{0};
    std::string m_title;
    Color m_accent;
    std::shared_ptr<KanbanBoardState> m_state;
    GalleryApp* m_app{nullptr};
    bool m_drag_over{false};
    std::vector<std::shared_ptr<RenderDraggableTaskCard>> m_cards;
};

class TaskDropColumnWidget : public Widget {
public:
    TaskDropColumnWidget(int col_idx, std::string title, Color accent,
                         std::shared_ptr<KanbanBoardState> state,
                         std::shared_ptr<std::vector<std::weak_ptr<RenderTaskDropColumn>>> registry,
                         GalleryApp* app)
        : m_col_idx(col_idx), m_title(std::move(title)), m_accent(accent),
          m_state(std::move(state)), m_registry(std::move(registry)), m_app(app) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderTaskDropColumn>(m_col_idx, m_title, m_accent, m_state, m_app);
        if (m_registry) m_registry->push_back(node);
        return node;
    }
    void update_render_node(std::shared_ptr<RenderBox>) override {}

private:
    int m_col_idx{0};
    std::string m_title;
    Color m_accent;
    std::shared_ptr<KanbanBoardState> m_state;
    std::shared_ptr<std::vector<std::weak_ptr<RenderTaskDropColumn>>> m_registry;
    GalleryApp* m_app{nullptr};
};

// OS 文件专属投放区 (展示 RenderBox::on_drop 接收 payload.files)
class RenderOsFileDropZone : public RenderBox {
public:
    explicit RenderOsFileDropZone(GalleryApp* app) : m_app(app) {
        is_drop_target = true;
        on_drag_enter = [this](DragEvent& ev) {
            if (!ev.payload.files.empty() || ev.payload.mime_type == "text/uri-list") {
                m_highlighted = true;
                ev.accepted = true;
                mark_needs_paint();
            }
        };
        on_drag_over = [this](DragEvent& ev) {
            if (!ev.payload.files.empty() || ev.payload.mime_type == "text/uri-list") {
                m_highlighted = true;
                ev.accepted = true;
            }
        };
        on_drag_leave = [this](DragEvent&) {
            m_highlighted = false;
            mark_needs_paint();
        };
        on_drop = [this](DragEvent& ev) {
            m_highlighted = false;
            if (!ev.payload.files.empty()) {
                ev.accepted = true;
                m_last_count = ev.payload.files.size();
                if (m_app) {
                    m_app->os_drop_status.set(
                        "专属投放区已命中 on_drop! 收到 " + std::to_string(ev.payload.files.size()) +
                        " 个文件 @局部坐标 (" + std::to_string(static_cast<int>(ev.local_position.x)) + ", " +
                        std::to_string(static_cast<int>(ev.local_position.y)) + ")");
                }
                mark_needs_paint();
            }
        };
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 480.0f;
        m_size = m_constraints.constrain(Vec2{w, 108.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& theme = ThemeContext::current();
        const Rect r = bounds();

        Color bg = m_highlighted
            ? Color{theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, theme.is_dark ? 0.20f : 0.12f}
            : (theme.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.03f} : Color{0.0f, 0.0f, 0.0f, 0.025f});
        Color border = m_highlighted
            ? theme.colors.primary
            : Color{theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, 0.45f};

        ctx.draw_rect(r, bg, 8.0f, 0.0f);
        ctx.draw_rect(r, border, 8.0f, m_highlighted ? 2.0f : 1.2f);

        ctx.draw_text("将操作系统中的文件（支持多选）拖放到此区域",
                      PointF{r.min_x + 20.0f, r.min_y + 24.0f}, 14.5f,
                      theme.colors.text_primary, FontWeight::SemiBold);
        std::string hint = m_last_count > 0
            ? ("上次命中此投放区: " + std::to_string(m_last_count) + " 个文件（可继续拖入新文件，或拖入 .bmp 到 Image 页直接预览）")
            : "Win32 通过 WM_DROPFILES（零静态 shell32.dll 依赖）/ Web 通过 HTML5 drop 自动派发到命中的 RenderBox::on_drop";
        ctx.draw_text(hint, PointF{r.min_x + 20.0f, r.min_y + 52.0f}, 12.0f,
                      theme.colors.text_secondary, FontWeight::Regular);
        ctx.draw_text("支持同时监听窗口级 Window::on_files_dropped 与控件级 RenderBox::on_drop",
                      PointF{r.min_x + 20.0f, r.min_y + 74.0f}, 11.5f,
                      theme.colors.primary, FontWeight::Medium);
    }

private:
    GalleryApp* m_app{nullptr};
    bool m_highlighted{false};
    size_t m_last_count{0};
};

class OsFileDropZoneWidget : public Widget {
public:
    explicit OsFileDropZoneWidget(GalleryApp* app) : m_app(app) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderOsFileDropZone>(m_app);
    }
    void update_render_node(std::shared_ptr<RenderBox>) override {}

private:
    GalleryApp* m_app{nullptr};
};

} // namespace

// ==================== 3. OS 文件拖放与窗口内控件拖拽演示页 ====================
WidgetPtr build_drag_drop_page(GalleryApp& app) {
    auto board_state = std::make_shared<KanbanBoardState>();
    board_state->reset();
    app.drag_event_log.set(board_state->format_logs());

    auto col_registry = std::make_shared<std::vector<std::weak_ptr<RenderTaskDropColumn>>>();
    board_state->on_changed = [col_registry]() {
        for (auto& w : *col_registry) {
            if (auto col = w.lock()) {
                col->rebuild_children();
            }
        }
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Drag & Drop 拖放与文件投放", 22.0f),
            app.make_muted("统一拖拽事件协议：同时支持窗口内控件拖拽（5px 防误触阈值 + DragPayload MIME 载荷 + 悬停高亮投放区）与操作系统文件拖放（Win32 WM_DROPFILES / Web HTML5 Drop）。", 12.5f),

            // 演示 1：窗口内控件拖拽看板
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            Text { .text = "窗口内控件拖拽：三列任务看板 (is_draggable / is_drop_target)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                            Button {
                                .text = "重置看板",
                                .on_click = [&app, board_state]() {
                                    board_state->reset();
                                    app.drag_event_log.set(board_state->format_logs());
                                    if (board_state->on_changed) board_state->on_changed();
                                }
                            }
                        }
                    },
                    HStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 10.0f,
                        .content = {
                            Expanded {
                                .child = make_widget<TaskDropColumnWidget>(
                                    0, "待处理", Color{0.00f, 0.56f, 0.92f, 1.0f}, board_state, col_registry, &app)
                            },
                            Expanded {
                                .child = make_widget<TaskDropColumnWidget>(
                                    1, "进行中", Color{0.88f, 0.56f, 0.14f, 1.0f}, board_state, col_registry, &app)
                            },
                            Expanded {
                                .child = make_widget<TaskDropColumnWidget>(
                                    2, "已完成", Color{0.12f, 0.70f, 0.42f, 1.0f}, board_state, col_registry, &app)
                            }
                        }
                    },
                    Box {
                        .background_color = Color{0.0f, 0.0f, 0.0f, 0.16f},
                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.08f},
                        .border_width = 1.0f,
                        .corner_radius = 6.0f,
                        .padding = EdgeInsets(8.0f, 12.0f),
                        .child = Text { .font_size = 11.5f }
                            .bind(&RenderText::set_text, app.drag_event_log)
                            .bind(&RenderText::set_color, app.accent_text)
                    }
                }
            }),

            // 演示 2：操作系统文件拖放
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            Text { .text = "操作系统文件拖放 (WM_DROPFILES / HTML5 Drop)", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                            HStack {
                                .spacing = 8.0f,
                                .content = {
                                    Button {
                                        .text = "复制路径",
                                        .on_click = [&app]() {
                                            Clipboard::set_text(app.os_drop_files.get());
                                            Toast::show(ToastDesc{ .title = "已复制拖放路径", .message = "已写入系统剪贴板", .severity = ToastSeverity::Success });
                                        }
                                    },
                                    Button {
                                        .text = "清空记录",
                                        .on_click = [&app]() {
                                            app.os_drop_status.set("已清空拖放记录，可继续从资源管理器拖入文件");
                                            app.os_drop_files.set("尚未收到操作系统文件拖放");
                                        }
                                    }
                                }
                            }
                        }
                    },
                    make_widget<OsFileDropZoneWidget>(&app),
                    app.make_accent(app.os_drop_status, 12.0f),
                    Box {
                        .background_color = Color{0.0f, 0.0f, 0.0f, 0.16f},
                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.08f},
                        .border_width = 1.0f,
                        .corner_radius = 6.0f,
                        .padding = EdgeInsets(10.0f, 12.0f),
                        .child = Text { .font_size = 12.0f }
                            .bind(&RenderText::set_text, app.os_drop_files)
                            .bind(&RenderText::set_color, app.muted_text)
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

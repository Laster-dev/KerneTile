#include "gallery_pages.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

// ==================== 程序化 RGBA8 示例位图生成（零外部资源依赖） ====================
std::shared_ptr<ImageBuffer> make_neon_gradient_image(uint32_t w = 256, uint32_t h = 160) {
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4u);
    for (uint32_t y = 0; y < h; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(h - 1u);
        for (uint32_t x = 0; x < w; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(w - 1u);

            // 深海蓝 -> 极光青 -> 紫罗兰渐变基底
            float r = 0.08f + 0.55f * u * u + 0.25f * (1.0f - v);
            float g = 0.16f + 0.52f * std::sin(u * 3.14159f) * (1.0f - 0.35f * v) + 0.15f * (1.0f - u);
            float b = 0.38f + 0.58f * (1.0f - u * 0.4f) * (0.55f + 0.45f * v);

            // 居中柔和光环
            float dx = (u - 0.52f) * (static_cast<float>(w) / static_cast<float>(h));
            float dy = v - 0.48f;
            float dist = std::sqrt(dx * dx + dy * dy);
            float ring = std::exp(-std::pow((dist - 0.28f) / 0.055f, 2.0f));
            float core = std::exp(-std::pow(dist / 0.16f, 2.0f));
            r += ring * 0.42f + core * 0.18f;
            g += ring * 0.32f + core * 0.38f;
            b += ring * 0.55f + core * 0.42f;

            // 细微科技网格线
            if ((x % 24u == 0u) || (y % 24u == 0u)) {
                r += 0.06f;
                g += 0.08f;
                b += 0.10f;
            }

            size_t idx = (static_cast<size_t>(y) * w + x) * 4u;
            px[idx + 0] = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f + 0.5f);
            px[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f + 0.5f);
            px[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f + 0.5f);
            px[idx + 3] = 255u;
        }
    }
    return ImageBuffer::from_rgba8(w, h, std::move(px));
}

std::shared_ptr<ImageBuffer> make_alpha_orb_image(uint32_t w = 192, uint32_t h = 192) {
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4u, 0u);
    for (uint32_t y = 0; y < h; ++y) {
        float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h) * 2.0f - 1.0f;
        for (uint32_t x = 0; x < w; ++x) {
            float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w) * 2.0f - 1.0f;
            float dist = std::sqrt(u * u + v * v);
            size_t idx = (static_cast<size_t>(y) * w + x) * 4u;

            // 柔和透明背景边框，突出展示 Alpha 混合
            if (dist > 0.92f) {
                bool checker = (((x / 12u) + (y / 12u)) & 1u) != 0u;
                px[idx + 0] = checker ? 200u : 120u;
                px[idx + 1] = checker ? 210u : 130u;
                px[idx + 2] = checker ? 230u : 150u;
                px[idx + 3] = checker ? 48u : 22u;
                continue;
            }

            float edge_alpha = std::clamp((0.92f - dist) * 14.0f, 0.0f, 1.0f);
            float z = std::sqrt(std::max(0.0f, 0.85f - dist * dist));
            // 顶部高光 + 渐变彩色玻璃球
            float lx = -0.35f, ly = -0.40f, lz = 0.84f;
            float diff = std::max(0.0f, u * lx + v * ly + z * lz);
            float spec = std::pow(std::max(0.0f, (u + 0.32f) * (-0.3f) + (v + 0.35f) * (-0.35f) + z * 0.88f), 16.0f);

            float r = 0.14f + 0.65f * (0.5f + 0.5f * u) * (0.45f + 0.55f * diff) + spec * 0.85f;
            float g = 0.35f + 0.55f * (0.5f - 0.5f * v) * (0.45f + 0.55f * diff) + spec * 0.85f;
            float b = 0.72f + 0.28f * diff + spec * 0.95f;
            float alpha = edge_alpha * (0.68f + 0.32f * std::min(1.0f, dist * 0.9f + spec));

            px[idx + 0] = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f + 0.5f);
            px[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f + 0.5f);
            px[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f + 0.5f);
            px[idx + 3] = static_cast<uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }
    return ImageBuffer::from_rgba8(w, h, std::move(px));
}

std::shared_ptr<ImageBuffer> make_color_chart_image(uint32_t w = 160, uint32_t h = 120) {
    static constexpr uint8_t kBars[8][3] = {
        {235, 235, 235}, {235, 210, 45}, {45, 215, 225}, {55, 200, 95},
        {215, 65, 205},  {225, 62, 72},  {48, 118, 235}, {36, 40, 52}
    };
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4u);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * 4u;
            if (x < 3u || x >= w - 3u || y < 3u || y >= h - 3u) {
                // 四周边框色带，方便观察 UniformToFill / None 裁剪
                px[idx + 0] = 255u;
                px[idx + 1] = 185u;
                px[idx + 2] = 0u;
                px[idx + 3] = 255u;
            } else if (y < h * 2u / 3u) {
                uint32_t bar = std::min(7u, x * 8u / w);
                px[idx + 0] = kBars[bar][0];
                px[idx + 1] = kBars[bar][1];
                px[idx + 2] = kBars[bar][2];
                px[idx + 3] = 255u;
            } else {
                // 底部灰阶渐变条 + 中心十字准星
                uint8_t g = static_cast<uint8_t>(x * 255u / (w - 1u));
                px[idx + 0] = g;
                px[idx + 1] = g;
                px[idx + 2] = g;
                px[idx + 3] = 255u;
            }
            if ((x == w / 2u && y > 16u && y < h - 16u) || (y == h / 2u && x > 16u && x < w - 16u)) {
                px[idx + 0] = 255u;
                px[idx + 1] = 255u;
                px[idx + 2] = 255u;
                px[idx + 3] = 255u;
            }
        }
    }
    return ImageBuffer::from_rgba8(w, h, std::move(px));
}

std::shared_ptr<ImageBuffer> make_animated_pulse_image(uint32_t w = 128, uint32_t h = 128, uint32_t frames_n = 16) {
    std::vector<ImageFrame> frames;
    frames.reserve(frames_n);
    constexpr float kTwoPi = 6.2831853f;
    for (uint32_t f = 0; f < frames_n; ++f) {
        float phase = static_cast<float>(f) / static_cast<float>(frames_n);
        float angle0 = phase * kTwoPi;
        ImageFrame frame;
        frame.delay_ms = 55u;
        frame.pixels.resize(static_cast<size_t>(w) * h * 4u);
        for (uint32_t y = 0; y < h; ++y) {
            float ny = (static_cast<float>(y) + 0.5f) / static_cast<float>(h) * 2.0f - 1.0f;
            for (uint32_t x = 0; x < w; ++x) {
                float nx = (static_cast<float>(x) + 0.5f) / static_cast<float>(w) * 2.0f - 1.0f;
                float dist = std::sqrt(nx * nx + ny * ny);
                float ang = std::atan2(ny, nx);
                float diff = ang - angle0;
                while (diff < 0.0f) diff += kTwoPi;
                while (diff >= kTwoPi) diff -= kTwoPi;
                float sweep = std::exp(-diff * 2.6f) * (dist < 0.90f ? 1.0f : 0.0f);
                float wave_r = std::fmod(phase, 1.0f) * 0.88f;
                float ring = std::exp(-std::pow((dist - wave_r) / 0.065f, 2.0f)) * (1.0f - wave_r);
                float rim = std::exp(-std::pow((dist - 0.86f) / 0.03f, 2.0f));

                float r = 0.05f + 0.14f * sweep + 0.52f * ring + 0.25f * rim;
                float g = 0.09f + 0.68f * sweep + 0.85f * ring + 0.55f * rim;
                float b = 0.16f + 0.92f * sweep + 0.95f * ring + 0.90f * rim;

                size_t idx = (static_cast<size_t>(y) * w + x) * 4u;
                frame.pixels[idx + 0] = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f + 0.5f);
                frame.pixels[idx + 1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f + 0.5f);
                frame.pixels[idx + 2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f + 0.5f);
                frame.pixels[idx + 3] = 255u;
            }
        }
        frames.push_back(std::move(frame));
    }
    return ImageBuffer::from_frames(w, h, std::move(frames));
}

void ensure_demo_images(GalleryApp& app) {
    if (!app.demo_img_gradient) app.demo_img_gradient = make_neon_gradient_image();
    if (!app.demo_img_alpha)    app.demo_img_alpha = make_alpha_orb_image();
    if (!app.demo_img_pattern)  app.demo_img_pattern = make_color_chart_image();
    if (!app.demo_img_animated) app.demo_img_animated = make_animated_pulse_image();
    if (!app.image_active_src.get()) {
        app.image_active_src.set(app.demo_img_gradient);
    }
}

const char* stretch_name(ImageStretch s) {
    switch (s) {
        case ImageStretch::Uniform:       return "Uniform (等比完整)";
        case ImageStretch::UniformToFill: return "UniformToFill (等比裁切填满)";
        case ImageStretch::Fill:          return "Fill (拉伸填满)";
        case ImageStretch::None:          return "None (原始 1:1 像素)";
    }
    return "Uniform";
}

void refresh_image_status(GalleryApp& app, const std::string& label) {
    auto src = app.image_active_src.get();
    if (!src || !src->valid()) {
        app.image_status.set("当前图像: 无有效图像");
        return;
    }
    std::string frame_info = src->is_animated()
        ? (", 动图 " + std::to_string(src->frame_count()) + " 帧")
        : "";
    app.image_status.set("当前图像: " + label + " (" +
                         std::to_string(src->width) + "x" + std::to_string(src->height) +
                         " RGBA8" + frame_info +
                         ", key=#" + std::to_string(src->cache_key & 0xFFFFu) +
                         ") | 模式: " + stretch_name(app.image_stretch.get()));
}

std::string extract_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

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

class DraggableTaskCardWidget : public Widget {
public:
    DraggableTaskCardWidget(std::shared_ptr<KanbanBoardState> state, KanbanTask task, GalleryApp* app)
        : m_state(std::move(state)), m_task(std::move(task)), m_app(app) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderDraggableTaskCard>(m_state, m_task, m_app);
    }
    void update_render_node(std::shared_ptr<RenderBox>) override {}

private:
    std::shared_ptr<KanbanBoardState> m_state;
    KanbanTask m_task;
    GalleryApp* m_app{nullptr};
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

// ==================== 1. Image 彩色位图控件展示页 ====================
WidgetPtr build_image_page(GalleryApp& app) {
    ensure_demo_images(app);

    auto make_stretch_sample = [&](const char* title, ImageStretch mode) -> WidgetPtr {
        return VStack {
            .align_items = AlignItems::Center,
            .spacing = 6.0f,
            .content = {
                Box {
                    .width = 164.0f,
                    .height = 104.0f,
                    .background_color = Color{0.0f, 0.0f, 0.0f, 0.22f},
                    .border_color = Color{1.0f, 1.0f, 1.0f, 0.14f},
                    .border_width = 1.0f,
                    .corner_radius = 6.0f,
                    .padding = EdgeInsets(2.0f),
                    .child = Image {
                        .source = app.demo_img_pattern,
                        .width = 158.0f,
                        .height = 98.0f,
                        .stretch = mode,
                        .corner_radius = 4.0f
                    }
                },
                Text { .text = title, .font_size = 11.5f }.bind(&RenderText::set_color, app.muted_text)
            }
        };
    };

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("Image 彩色位图与动图", 22.0f),
            app.make_muted("全后端 RGBA8 动态图集纹理渲染（CPU / D3D12 / Vulkan / WebGPU），基于裁剪版 stb_image 原生支持 PNG / JPG / BMP / GIF（含多帧动图）、预乘 Alpha、圆角 SDF 裁剪、超大图自动缩放与壁纸铺底。", 12.5f),

            // 交互式预览与属性调试台
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    HStack {
                        .justify = JustifyContent::SpaceBetween,
                        .align_items = AlignItems::Center,
                        .content = {
                            Text { .text = "交互式图像预览台（支持直接拖入 PNG / JPG / GIF / BMP）", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                            Button {
                                .text = "打开本地图片...",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    FileDialogOptions opt;
                                    opt.title = "选择 PNG / JPG / GIF / BMP 图片或壁纸";
                                    opt.filters = {
                                        {"支持的图片", "*.png;*.jpg;*.jpeg;*.gif;*.bmp"},
                                        {"PNG / JPG 壁纸", "*.png;*.jpg;*.jpeg"},
                                        {"GIF 动图", "*.gif"},
                                        {"所有文件", "*.*"}
                                    };
                                    if (app.window && app.window->platform_window()) {
                                        opt.owner_window_handle = app.window->platform_window()->get_native_handle();
                                    }
                                    FileDialog::open_file(opt, [&app](const FileDialogResult& res) {
                                        if (!res.accepted || res.paths.empty()) return;
                                        auto img = ImageBuffer::load_file(res.first_path());
                                        if (img && img->valid()) {
                                            app.demo_img_custom = img;
                                            app.image_active_src.set(img);
                                            refresh_image_status(app, extract_filename(res.first_path()));
                                            Toast::show(ToastDesc{
                                                .title = img->is_animated() ? "GIF 动图已加载" : "图像已加载",
                                                .message = extract_filename(res.first_path()) + " (" +
                                                           std::to_string(img->width) + "x" + std::to_string(img->height) +
                                                           (img->is_animated() ? (", " + std::to_string(img->frame_count()) + " 帧") : "") + ")",
                                                .severity = ToastSeverity::Success
                                            });
                                        } else {
                                            app.image_status.set("加载失败: 无法解码该文件（支持 PNG / JPG / GIF / BMP）");
                                            Toast::show(ToastDesc{
                                                .title = "图像解码失败",
                                                .message = "请选择有效的 PNG / JPG / GIF / BMP 文件",
                                                .severity = ToastSeverity::Warning
                                            });
                                        }
                                    });
                                }
                            }
                        }
                    },

                    // 预览画布 + 右侧参数控制
                    Wrap {
                        .spacing = 16.0f,
                        .run_spacing = 12.0f,
                        .content = {
                            Box {
                                .width = 340.0f,
                                .height = 220.0f,
                                .background_color = Color{0.0f, 0.0f, 0.0f, 0.20f},
                                .border_color = Color{1.0f, 1.0f, 1.0f, 0.12f},
                                .border_width = 1.0f,
                                .corner_radius = 10.0f,
                                .padding = EdgeInsets(8.0f),
                                .child = Image {
                                    .source = app.image_active_src.get(),
                                    .auto_play = app.image_auto_play.get(),
                                    .width = 324.0f,
                                    .height = 204.0f,
                                    .stretch = app.image_stretch.get(),
                                    .corner_radius = app.image_corner_radius.get(),
                                    .tint = app.image_tint.get(),
                                    .opacity = app.image_opacity.get(),
                                    .border_color = Color{1.0f, 1.0f, 1.0f, 0.18f},
                                    .border_width = 1.0f
                                }.bind(&RenderImage::set_source, app.image_active_src)
                                 .bind(&RenderImage::set_auto_play, app.image_auto_play)
                                 .bind(&RenderImage::set_stretch, app.image_stretch)
                                 .bind(&RenderImage::set_corner_radius, app.image_corner_radius)
                                 .bind(&RenderImage::set_opacity, app.image_opacity)
                                 .bind(&RenderImage::set_tint, app.image_tint),
                                .alignment = Alignment::center()
                            },
                            VStack {
                                .align_items = AlignItems::Stretch,
                                .spacing = 10.0f,
                                .content = {
                                    HStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 8.0f,
                                        .content = {
                                            app.make_muted("内置纹理:", 12.0f),
                                            SegmentedControl {
                                                .items = {
                                                    SegmentedItem{ .label = "霓虹渐变" },
                                                    SegmentedItem{ .label = "透明玻璃球" },
                                                    SegmentedItem{ .label = "色彩测试卡" },
                                                    SegmentedItem{ .label = "雷达动图" }
                                                },
                                                .on_selected_index_changed = [&app](int idx) {
                                                    if (idx == 0) {
                                                        app.image_active_src.set(app.demo_img_gradient);
                                                        refresh_image_status(app, "霓虹流体渐变");
                                                    } else if (idx == 1) {
                                                        app.image_active_src.set(app.demo_img_alpha);
                                                        refresh_image_status(app, "半透明玻璃球");
                                                    } else if (idx == 2) {
                                                        app.image_active_src.set(app.demo_img_pattern);
                                                        refresh_image_status(app, "色彩测试卡");
                                                    } else if (idx == 3) {
                                                        app.image_active_src.set(app.demo_img_animated);
                                                        refresh_image_status(app, "多帧雷达动图");
                                                    }
                                                }
                                            }.bind_two_way(app.image_preset_idx)
                                        }
                                    },
                                    HStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 8.0f,
                                        .content = {
                                            app.make_muted("动图播放:", 12.0f),
                                            ToggleSwitch {
                                                .checked = app.image_auto_play.get(),
                                                .label = "自动播放多帧 GIF / 动图"
                                            }.bind_two_way(app.image_auto_play)
                                        }
                                    },
                                    HStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 8.0f,
                                        .content = {
                                            app.make_muted("填充模式:", 12.0f),
                                            SegmentedControl {
                                                .items = {
                                                    SegmentedItem{ .label = "Uniform" },
                                                    SegmentedItem{ .label = "Cover" },
                                                    SegmentedItem{ .label = "Fill" },
                                                    SegmentedItem{ .label = "None" }
                                                },
                                                .on_selected_index_changed = [&app](int idx) {
                                                    static constexpr ImageStretch kMap[] = {
                                                        ImageStretch::Uniform,
                                                        ImageStretch::UniformToFill,
                                                        ImageStretch::Fill,
                                                        ImageStretch::None
                                                    };
                                                    if (idx >= 0 && idx < 4) {
                                                        app.image_stretch.set(kMap[idx]);
                                                        refresh_image_status(app, "当前预览");
                                                    }
                                                }
                                            }.bind_two_way(app.image_stretch_idx)
                                        }
                                    },
                                    HStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 8.0f,
                                        .content = {
                                            app.make_muted("色调滤镜:", 12.0f),
                                            SegmentedControl {
                                                .items = {
                                                    SegmentedItem{ .label = "原色" },
                                                    SegmentedItem{ .label = "暖金" },
                                                    SegmentedItem{ .label = "冰蓝" },
                                                    SegmentedItem{ .label = "霓虹粉" }
                                                },
                                                .on_selected_index_changed = [&app](int idx) {
                                                    static const Color kTints[] = {
                                                        Color{1.00f, 1.00f, 1.00f, 1.0f},
                                                        Color{1.00f, 0.84f, 0.56f, 1.0f},
                                                        Color{0.62f, 0.86f, 1.00f, 1.0f},
                                                        Color{1.00f, 0.65f, 0.92f, 1.0f}
                                                    };
                                                    if (idx >= 0 && idx < 4) app.image_tint.set(kTints[idx]);
                                                }
                                            }.bind_two_way(app.image_tint_idx)
                                        }
                                    },
                                    HStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 10.0f,
                                        .content = {
                                            Text { .font_size = 12.0f }
                                                .bind(&RenderText::set_text, app.image_radius_label)
                                                .bind(&RenderText::set_color, app.muted_text),
                                            Box {
                                                .width = 180.0f,
                                                .child = Slider {
                                                    .min_value = 0.0f,
                                                    .max_value = 48.0f,
                                                    .step = 2.0f,
                                                    .on_change = [&app](float v) {
                                                        app.image_radius_label.set("圆角半径: " + std::to_string(static_cast<int>(std::lround(v))) + " px");
                                                    }
                                                }.bind_two_way(app.image_corner_radius)
                                            }
                                        }
                                    },
                                    HStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 10.0f,
                                        .content = {
                                            Text { .font_size = 12.0f }
                                                .bind(&RenderText::set_text, app.image_opacity_label)
                                                .bind(&RenderText::set_color, app.muted_text),
                                            Box {
                                                .width = 180.0f,
                                                .child = Slider {
                                                    .min_value = 0.1f,
                                                    .max_value = 1.0f,
                                                    .step = 0.05f,
                                                    .on_change = [&app](float v) {
                                                        app.image_opacity_label.set("不透明度: " + std::to_string(static_cast<int>(std::lround(v * 100.0f))) + "%");
                                                    }
                                                }.bind_two_way(app.image_opacity)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    },
                    app.make_accent(app.image_status, 12.0f)
                }
            }),

            // 壁纸铺底演示卡片 (ZStack + UniformToFill + 叠加半透明卡片)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "壁纸铺底演示（ZStack + ImageStretch::UniformToFill，实时同步当前加载的图片/壁纸/GIF）", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Box {
                        .height = 156.0f,
                        .corner_radius = 10.0f,
                        .child = ZStack {
                            .alignment = Alignment::center(),
                            .content = {
                                Image {
                                    .source = app.image_active_src.get(),
                                    .auto_play = app.image_auto_play.get(),
                                    .width = 760.0f,
                                    .height = 156.0f,
                                    .stretch = ImageStretch::UniformToFill,
                                    .corner_radius = 10.0f
                                }.bind(&RenderImage::set_source, app.image_active_src)
                                 .bind(&RenderImage::set_auto_play, app.image_auto_play),
                                Box {
                                    .background_color = Color{0.06f, 0.07f, 0.10f, 0.62f},
                                    .border_color = Color{1.0f, 1.0f, 1.0f, 0.22f},
                                    .border_width = 1.0f,
                                    .corner_radius = 8.0f,
                                    .padding = EdgeInsets(12.0f, 20.0f),
                                    .child = VStack {
                                        .align_items = AlignItems::Center,
                                        .spacing = 4.0f,
                                        .content = {
                                            Text { .text = "KerneTile 壁纸容器示例", .font_size = 15.0f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                            Text { .text = "将任意 PNG / JPG / GIF 拖入窗口或点击上方打开按钮即可实时替换此壁纸", .font_size = 12.0f, .color = Color{0.9f, 0.92f, 0.96f, 0.9f} }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }),

            // 四种 ImageStretch 并排对比卡片
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "四种 ImageStretch 缩放模式对比（源图 160x120，容器 158x98）", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    Wrap {
                        .spacing = 14.0f,
                        .run_spacing = 10.0f,
                        .content = {
                            make_stretch_sample("Uniform (完整等比)", ImageStretch::Uniform),
                            make_stretch_sample("UniformToFill (等比裁切)", ImageStretch::UniformToFill),
                            make_stretch_sample("Fill (拉伸填满)", ImageStretch::Fill),
                            make_stretch_sample("None (原始 1:1)", ImageStretch::None)
                        }
                    }
                }
            })
        }
    });
}

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

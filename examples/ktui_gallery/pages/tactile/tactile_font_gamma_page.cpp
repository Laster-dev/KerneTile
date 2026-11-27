#include "../gallery_pages.hpp"
#include <kernetile/math/color_space.hpp>
#include <kernetile/text/stem_darkening.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

using namespace ktui;

namespace gallery::pages {

namespace {

template <typename StateT>
class PageStateHostWidget final : public Widget {
public:
    PageStateHostWidget(std::shared_ptr<StateT> state, WidgetPtr inner)
        : m_state(std::move(state)), m_inner(std::move(inner)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        return m_inner ? m_inner->create_render_node() : nullptr;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (m_inner) {
            m_inner->update_render_node(std::move(node));
        }
    }

private:
    std::shared_ptr<StateT> m_state;
    WidgetPtr m_inner;
};

template <typename StateT>
WidgetPtr attach_page_state(std::shared_ptr<StateT> state, WidgetPtr page) {
    return make_widget<PageStateHostWidget<StateT>>(std::move(state), std::move(page));
}

// ============================================================================
// 页面 1: 暗色/亮色文字光渗补偿 (Irradiation Illusion Compensation / Font Gamma)
// ============================================================================
class RenderFontGammaComparePreview : public RenderBox {
public:
    void set_enabled_hint(bool v) {
        if (m_global_enabled != v) {
            m_global_enabled = v;
            mark_needs_paint();
        }
    }

    void set_sample_size(float sz) {
        float clamped = std::clamp(sz, 9.0f, 20.0f);
        if (m_sample_size != clamped) {
            m_sample_size = clamped;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        const bool stacked = (w < 540.0f);
        m_size = m_constraints.constrain(Vec2{w, stacked ? (224.0f * 2.0f + 12.0f) : 224.0f});
    }

    void paint(PaintContext& ctx) override {
        const float w = m_size.x;
        const bool stacked = (w < 540.0f);
        const float gap = stacked ? 12.0f : 14.0f;
        const float col_w = stacked ? w : std::max(140.0f, (w - gap) * 0.5f);
        const float h = stacked ? 224.0f : m_size.y;

        auto draw_half = [&](float x0, float y0, bool dark_panel) {
            const Rect panel = Rect::from_xywh(x0, y0, col_w, h);
            const Color bg = dark_panel
                ? Color{0.075f, 0.090f, 0.125f, 1.0f}
                : Color{0.965f, 0.975f, 0.992f, 1.0f};
            const Color bd = dark_panel
                ? Color{1.0f, 1.0f, 1.0f, 0.14f}
                : Color{0.0f, 0.0f, 0.0f, 0.12f};
            const Color fg = dark_panel
                ? Color{0.95f, 0.97f, 1.0f, 1.0f}
                : Color{0.07f, 0.09f, 0.13f, 1.0f};
            const Color muted = dark_panel
                ? Color{0.66f, 0.72f, 0.82f, 1.0f}
                : Color{0.34f, 0.39f, 0.48f, 1.0f};
            const Color badge_bg = dark_panel
                ? Color{0.25f, 0.55f, 1.0f, 0.16f}
                : Color{0.12f, 0.42f, 0.90f, 0.10f};
            const Color badge_fg = dark_panel
                ? Color{0.48f, 0.82f, 1.0f, 1.0f}
                : Color{0.08f, 0.36f, 0.78f, 1.0f};

            ctx.draw_box(panel, bg, bd, 8.0f, 1.0f);
            ctx.push_clip(panel, 8.0f);

            const float fg_luma = 0.2126f * fg.r + 0.7152f * fg.g + 0.0722f * fg.b;
            const float bg_luma = 0.2126f * bg.r + 0.7152f * bg.g + 0.0722f * bg.b;
            const float gamma = kernetile::text::StemDarkener::compute_irradiation_gamma(
                m_sample_size, fg_luma, bg_luma, 1.0f);

            char header_buf[96];
            std::snprintf(header_buf, sizeof(header_buf),
                          dark_panel ? "深底浅字 (@%.0fpx γ = %.2f 抑制光渗)" : "浅底深字 (@%.0fpx γ = %.2f 加粗防发虚)",
                          m_sample_size, gamma);
            ctx.draw_text(header_buf, PointF{x0 + 14.0f, y0 + 12.0f}, 12.0f, badge_fg, FontWeight::SemiBold);

            // Row 1: 关闭光渗补偿 (Raw DirectWrite Coverage)
            ctx.draw_text("Raw 未补偿（直接覆盖率线性混合）",
                          PointF{x0 + 14.0f, y0 + 38.0f}, 11.0f, muted, FontWeight::Regular, false, "", false, false);
            ctx.draw_text("KerneTile 锐利中英混排 Crisp Glyphs 0123456789",
                          PointF{x0 + 14.0f, y0 + 58.0f}, m_sample_size, fg, FontWeight::Regular, false, "", false, false);
            ctx.draw_text("极细横竖笔画对比：天地玄黄 宇宙洪荒 ABCabc ()[]{}",
                          PointF{x0 + 14.0f, y0 + 86.0f}, std::max(10.0f, m_sample_size - 1.0f), fg, FontWeight::Regular, false, "", false, false);

            // 分隔线
            ctx.draw_rect(Rect::from_xywh(x0 + 14.0f, y0 + 118.0f, col_w - 28.0f, 1.0f), bd, 0.0f, 0.0f);

            // Row 2: 开启光渗补偿 (Irradiation Compensated)
            const Rect pill = Rect::from_xywh(x0 + 14.0f, y0 + 127.0f, col_w - 28.0f, 84.0f);
            ctx.draw_rect(pill, badge_bg, 6.0f, 0.0f);
            ctx.draw_text(m_global_enabled ? "Font Gamma 已补偿（深浅主题笔画粗细一致）" : "Font Gamma 局部对比示例（全局开关当前已关闭）",
                          PointF{x0 + 22.0f, y0 + 134.0f}, 11.0f, badge_fg, FontWeight::Medium, false, "", false, true);
            ctx.draw_text("KerneTile 锐利中英混排 Crisp Glyphs 0123456789",
                          PointF{x0 + 22.0f, y0 + 154.0f}, m_sample_size, fg, FontWeight::Regular, false, "", false, true);
            ctx.draw_text("极细横竖笔画对比：天地玄黄 宇宙洪荒 ABCabc ()[]{}",
                          PointF{x0 + 22.0f, y0 + 182.0f}, std::max(10.0f, m_sample_size - 1.0f), fg, FontWeight::Regular, false, "", false, true);

            ctx.pop_clip();
        };

        draw_half(0.0f, 0.0f, true);
        draw_half(stacked ? 0.0f : (col_w + gap), stacked ? (h + gap) : 0.0f, false);
    }

private:
    bool m_global_enabled{true};
    float m_sample_size{12.0f};
};

struct FontGammaComparePreview : BindableMixin<FontGammaComparePreview, RenderFontGammaComparePreview> {
    bool global_enabled{true};
    float sample_size{12.0f};
    operator WidgetPtr() const;
};

class FontGammaComparePreviewWidget final : public Widget {
public:
    explicit FontGammaComparePreviewWidget(const FontGammaComparePreview& d) : m_d(d) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderFontGammaComparePreview>();
        n->set_enabled_hint(m_d.global_enabled);
        n->set_sample_size(m_d.sample_size);
        return n;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderFontGammaComparePreview>(node)) {
            n->set_enabled_hint(m_d.global_enabled);
            n->set_sample_size(m_d.sample_size);
        }
    }
private:
    FontGammaComparePreview m_d;
};

inline FontGammaComparePreview::operator WidgetPtr() const {
    return make_widget<FontGammaComparePreviewWidget>(*this);
}

class RenderFontGammaLadder final : public RenderBox {
public:
    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        const bool stacked = (w < 540.0f);
        m_size = m_constraints.constrain(Vec2{w, stacked ? (168.0f * 2.0f + 12.0f) : 168.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const float w = m_size.x;
        const bool stacked = (w < 540.0f);
        const float gap = stacked ? 12.0f : 14.0f;
        const float col_w = stacked ? w : std::max(140.0f, (w - gap) * 0.5f);
        const float h = stacked ? 168.0f : m_size.y;

        static constexpr float kSizes[5] = {10.0f, 11.0f, 12.0f, 14.0f, 17.0f};
        auto draw_col = [&](float x0, float y0, bool compensated) {
            const Rect r = Rect::from_xywh(x0, y0, col_w, h);
            const Color bg = t.is_dark ? Color{0.08f, 0.095f, 0.13f, 1.0f} : Color{0.97f, 0.98f, 1.0f, 1.0f};
            const Color fg = t.is_dark ? Color{0.96f, 0.97f, 1.0f, 1.0f} : Color{0.06f, 0.08f, 0.12f, 1.0f};
            const Color accent = t.is_dark ? Color{0.48f, 0.82f, 1.0f, 1.0f} : Color{0.05f, 0.38f, 0.78f, 1.0f};
            ctx.draw_box(r, bg, t.is_dark ? Color{1, 1, 1, 0.12f} : Color{0, 0, 0, 0.12f}, 8.0f, 1.0f);
            ctx.push_clip(r, 8.0f);

            ctx.draw_text(compensated ? "字号自适应 Gamma 阶梯 (Font Gamma = ON)" : "原始覆盖率对比 (Font Gamma = OFF)",
                          PointF{x0 + 14.0f, y0 + 10.0f}, 11.5f, compensated ? accent : t.colors.text_secondary, FontWeight::SemiBold);

            const float fg_l = 0.2126f * fg.r + 0.7152f * fg.g + 0.0722f * fg.b;
            const float bg_l = 0.2126f * bg.r + 0.7152f * bg.g + 0.0722f * bg.b;

            float y = y0 + 36.0f;
            for (float sz : kSizes) {
                const float g = compensated
                    ? kernetile::text::StemDarkener::compute_irradiation_gamma(sz, fg_l, bg_l, 1.0f)
                    : 1.0f;
                char buf[96];
                std::snprintf(buf, sizeof(buf), "%.0fpx (γ=%.2f) 细节清晰度 · Crisp Text 0123", sz, g);
                ctx.draw_text(buf, PointF{x0 + 14.0f, y}, sz, fg, FontWeight::Regular, false, "", false, compensated);
                y += sz + 11.0f;
            }
            ctx.pop_clip();
        };

        draw_col(0.0f, 0.0f, false);
        draw_col(stacked ? 0.0f : (col_w + gap), stacked ? (h + gap) : 0.0f, true);
    }
};

struct FontGammaLadder : BindableMixin<FontGammaLadder, RenderFontGammaLadder> {
    operator WidgetPtr() const;
};

class FontGammaLadderWidget final : public Widget {
public:
    std::shared_ptr<RenderBox> create_render_node() override {
        return make_render_node<RenderFontGammaLadder>();
    }
    void update_render_node(std::shared_ptr<RenderBox>) override {}
};

inline FontGammaLadder::operator WidgetPtr() const {
    return make_widget<FontGammaLadderWidget>();
}

struct FontGammaPageState {
    Property<bool> font_gamma_on{true};
    Property<float> sample_size{12.0f};
    Property<std::string> size_label{"示例字号: 12 px"};
};

// ============================================================================
// 页面 2: 方向性玻璃高光内描边 (Directional Fresnel Rim Highlight)
// ============================================================================
class RenderDirectionalRimStage : public RenderBox {
public:
    RenderDirectionalRimStage() {
        cursor_shape = CursorShape::Crosshair;
        on_pointer_move = [this](PointerEvent& ev) {
            if (m_pointer_follow) {
                m_pointer = ev.local_position;
                m_has_pointer = true;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_pointer_follow && m_has_pointer) {
                m_has_pointer = false;
                mark_needs_paint();
            }
        };
    }

    void set_angle_deg(float deg) { if (m_angle_deg != deg) { m_angle_deg = deg; mark_needs_paint(); } }
    void set_inner_glow(float g) { if (m_inner_glow != g) { m_inner_glow = g; mark_needs_paint(); } }
    void set_intensity(float v) { if (m_intensity != v) { m_intensity = v; mark_needs_paint(); } }
    void set_border_idx(int idx) {
        static constexpr float kWidths[] = {1.0f, 1.5f, 2.0f};
        float bw = kWidths[std::clamp(idx, 0, 2)];
        if (m_border_w != bw) { m_border_w = bw; mark_needs_paint(); }
    }
    void set_pointer_follow(bool v) {
        if (m_pointer_follow != v) {
            m_pointer_follow = v;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        m_size = m_constraints.constrain(Vec2{w, 250.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const float w = m_size.x;
        const float h = m_size.y;

        ctx.push_clip(bounds(), 10.0f);
        if (is_dark) {
            ctx.draw_linear_gradient(bounds(),
                Color{0.07f, 0.11f, 0.22f, 1.0f},
                Color{0.18f, 0.08f, 0.24f, 1.0f},
                PointF{0.0f, 0.0f}, PointF{w, h}, 10.0f, 0.0f, true);
        } else {
            ctx.draw_linear_gradient(bounds(),
                Color{0.76f, 0.87f, 0.98f, 1.0f},
                Color{0.94f, 0.82f, 0.95f, 1.0f},
                PointF{0.0f, 0.0f}, PointF{w, h}, 10.0f, 0.0f, true);
        }

        // 透底光学色块，用于直观体现玻璃态面板通透与边缘折射层次
        ctx.draw_rect(Rect::from_xywh(w * 0.08f, 34.0f, 148.0f, 148.0f),
                      Color{0.18f, 0.62f, 1.0f, is_dark ? 0.36f : 0.45f}, 74.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(w * 0.44f, 70.0f, 162.0f, 162.0f),
                      Color{0.96f, 0.34f, 0.62f, is_dark ? 0.32f : 0.38f}, 81.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(w * 0.72f, 24.0f, 132.0f, 132.0f),
                      Color{0.10f, 0.86f, 0.68f, is_dark ? 0.28f : 0.36f}, 66.0f, 0.0f);

        Vec2 light_dir;
        PointF light_anchor;
        if (m_pointer_follow && m_has_pointer) {
            Vec2 delta{m_pointer.x - w * 0.5f, m_pointer.y - h * 0.5f};
            float len = std::hypot(delta.x, delta.y);
            light_dir = (len > 2.0f) ? Vec2{delta.x / len, delta.y / len} : Vec2{0.0f, -1.0f};
            light_anchor = m_pointer;
        } else {
            const float rad = m_angle_deg * 3.14159265f / 180.0f;
            light_dir = Vec2{std::sin(rad), -std::cos(rad)};
            light_anchor = PointF{w * 0.5f + light_dir.x * (w * 0.38f), h * 0.5f + light_dir.y * (h * 0.38f)};
        }

        const float pad = 22.0f;
        const float card_gap = 14.0f;
        const float card_w = std::max(100.0f, (w - pad * 2.0f - card_gap * 2.0f) / 3.0f);
        const float card_h = 156.0f;
        const float card_y = 48.0f;
        const float radii[3] = {10.0f, 18.0f, 34.0f};
        const char* labels[3] = {"R=10px 桌面面板", "R=18px 超椭圆卡片", "R=34px 胶囊玻璃"};

        for (int i = 0; i < 3; ++i) {
            const float cx = pad + static_cast<float>(i) * (card_w + card_gap);
            const Rect card_r = Rect::from_xywh(cx, card_y, card_w, card_h);
            const Color glass_fill = is_dark
                ? Color{0.10f, 0.12f, 0.17f, 0.56f}
                : Color{1.0f, 1.0f, 1.0f, 0.54f};

            ctx.draw_elevation_shadow(card_r, radii[i], is_dark, 24.0f, 0.85f);
            ctx.draw_glass_panel(card_r, glass_fill, radii[i], is_dark, light_dir, m_intensity, m_inner_glow, m_border_w);

            ctx.draw_text(labels[i], PointF{cx + 14.0f, card_y + 18.0f}, 13.0f,
                          is_dark ? Color{1, 1, 1, 0.94f} : Color{0.06f, 0.08f, 0.12f, 0.92f}, FontWeight::SemiBold);
            ctx.draw_text("迎光侧 Fresnel 高光", PointF{cx + 14.0f, card_y + 46.0f}, 11.5f,
                          is_dark ? Color{1, 1, 1, 0.72f} : Color{0.10f, 0.14f, 0.22f, 0.74f});
            ctx.draw_text("背光侧厚度衰减暗边", PointF{cx + 14.0f, card_y + 66.0f}, 11.5f,
                          is_dark ? Color{1, 1, 1, 0.64f} : Color{0.10f, 0.14f, 0.22f, 0.66f});

            char dir_txt[48];
            std::snprintf(dir_txt, sizeof(dir_txt), "L=(%.2f, %.2f)", light_dir.x, light_dir.y);
            ctx.draw_text(dir_txt, PointF{cx + 14.0f, card_y + card_h - 28.0f}, 11.0f,
                          is_dark ? Color{0.50f, 0.85f, 1.0f, 0.92f} : Color{0.0f, 0.38f, 0.76f, 0.92f},
                          FontWeight::Medium, false, "monospace");
        }

        ctx.draw_rect(Rect::from_xywh(light_anchor.x - 10.0f, light_anchor.y - 10.0f, 20.0f, 20.0f),
                      Color{1.0f, 0.92f, 0.55f, 0.28f}, 10.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(light_anchor.x - 4.0f, light_anchor.y - 4.0f, 8.0f, 8.0f),
                      Color{1.0f, 0.96f, 0.72f, 0.95f}, 4.0f, 0.0f);

        ctx.draw_text(m_pointer_follow ? "移动鼠标实时改变光源角度，观察四边高光与底边暗棱随 N·L 旋转" : "当前由方位角滑块驱动受光向量 L",
                      PointF{pad, 14.0f}, 12.0f, is_dark ? Color{1, 1, 1, 0.84f} : Color{0.08f, 0.10f, 0.15f, 0.82f});

        ctx.pop_clip();
        ctx.draw_rect(bounds(), is_dark ? Color{1, 1, 1, 0.12f} : Color{0, 0, 0, 0.12f}, 10.0f, 1.0f);
    }

private:
    float m_angle_deg{-14.0f};
    float m_inner_glow{0.45f};
    float m_intensity{1.35f};
    float m_border_w{1.5f};
    bool m_pointer_follow{true};
    bool m_has_pointer{false};
    PointF m_pointer{0.0f, 0.0f};
};

struct DirectionalRimStage : BindableMixin<DirectionalRimStage, RenderDirectionalRimStage> {
    float angle_deg{-14.0f};
    float inner_glow{0.45f};
    float intensity{1.35f};
    int border_idx{1};
    bool pointer_follow{true};
    operator WidgetPtr() const;
};

class DirectionalRimStageWidget final : public Widget {
public:
    explicit DirectionalRimStageWidget(const DirectionalRimStage& d) : m_d(d) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderDirectionalRimStage>();
        n->set_angle_deg(m_d.angle_deg);
        n->set_inner_glow(m_d.inner_glow);
        n->set_intensity(m_d.intensity);
        n->set_border_idx(m_d.border_idx);
        n->set_pointer_follow(m_d.pointer_follow);
        return n;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderDirectionalRimStage>(node)) {
            n->set_angle_deg(m_d.angle_deg);
            n->set_inner_glow(m_d.inner_glow);
            n->set_intensity(m_d.intensity);
            n->set_border_idx(m_d.border_idx);
            n->set_pointer_follow(m_d.pointer_follow);
        }
    }
private:
    DirectionalRimStage m_d;
};

inline DirectionalRimStage::operator WidgetPtr() const {
    return make_widget<DirectionalRimStageWidget>(*this);
}

// ============================================================================
// 页面 3: OkLab 感知均匀色彩插值 (OkLab Perceptual Color Interpolation)
// ============================================================================
struct GradientPair {
    const char* name;
    Color c0;
    Color c1;
};

static constexpr GradientPair kGradientPairs[] = {
    {"蓝 → 黄 (#0066FF → #FFCC00)", Color{0.0f, 0.40f, 1.0f, 1.0f},  Color{1.0f, 0.80f, 0.0f, 1.0f}},
    {"品红 → 青 (#FF0088 → #00E5FF)", Color{1.0f, 0.00f, 0.53f, 1.0f}, Color{0.0f, 0.90f, 1.0f, 1.0f}},
    {"绯红 → 翠绿 (#FF2244 → #00DD66)", Color{1.0f, 0.13f, 0.27f, 1.0f}, Color{0.0f, 0.87f, 0.40f, 1.0f}},
    {"靛紫 → 琥珀 (#6D28D9 → #F59E0B)", Color{0.43f, 0.16f, 0.85f, 1.0f}, Color{0.96f, 0.62f, 0.04f, 1.0f}},
};

class RenderOkLabGradientPreview : public RenderBox {
public:
    void set_pair_idx(int idx) {
        int clamped = std::clamp(idx, 0, 3);
        if (m_pair_idx != clamped) {
            m_pair_idx = clamped;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        m_size = m_constraints.constrain(Vec2{w, 220.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const auto& pair = kGradientPairs[m_pair_idx];
        const float w = m_size.x;
        const float bar_w = std::max(160.0f, w - 134.0f);

        auto draw_row = [&](float y, bool oklab, const char* title) {
            ctx.draw_text(title, PointF{0.0f, y}, 12.0f, oklab ? t.colors.text_primary : t.colors.text_secondary,
                          oklab ? FontWeight::SemiBold : FontWeight::Regular);
            const Rect bar = Rect::from_xywh(0.0f, y + 20.0f, bar_w, 42.0f);
            ctx.draw_linear_gradient(bar, pair.c0, pair.c1,
                                     PointF{bar.min_x, bar.min_y}, PointF{bar.max_x, bar.min_y},
                                     8.0f, 0.0f, oklab);
            ctx.draw_rect(bar, t.is_dark ? Color{1, 1, 1, 0.14f} : Color{0, 0, 0, 0.12f}, 8.0f, 1.0f);

            const Color mid = oklab
                ? kernetile::math::lerp_oklab(pair.c0, pair.c1, 0.5f)
                : Motion::lerp_linear(pair.c0, pair.c1, 0.5f);
            const Rect sw = Rect::from_xywh(bar_w + 14.0f, y + 20.0f, 118.0f, 42.0f);
            ctx.draw_box(sw, mid, t.is_dark ? Color{1, 1, 1, 0.22f} : Color{0, 0, 0, 0.18f}, 8.0f, 1.0f);

            char hex[32];
            std::snprintf(hex, sizeof(hex), "t=0.5 #%02X%02X%02X",
                          static_cast<unsigned>(std::lround(mid.r * 255.0f)),
                          static_cast<unsigned>(std::lround(mid.g * 255.0f)),
                          static_cast<unsigned>(std::lround(mid.b * 255.0f)));
            const float luma = 0.2126f * mid.r + 0.7152f * mid.g + 0.0722f * mid.b;
            ctx.draw_text(hex, PointF{sw.min_x + 10.0f, sw.min_y + 13.0f}, 11.0f,
                          luma > 0.52f ? Color{0.05f, 0.06f, 0.08f, 1.0f} : Color{1, 1, 1, 0.95f},
                          FontWeight::SemiBold, false, "monospace");
        };

        draw_row(0.0f, false, "Linear sRGB 线性混合（高饱和色中段易落入暗灰浑浊带）");
        draw_row(80.0f, true, "OkLab 感知均匀插值（保持等感知亮度与纯正彩度过渡）");

        // 底部 5 段离散采样的 OkLab vs Linear 对照条
        ctx.draw_text("5 等分节点彩度对照 (上: Linear sRGB / 下: OkLab):", PointF{0.0f, 156.0f}, 11.5f, t.colors.text_secondary);
        const float step_w = std::max(40.0f, (w - 40.0f) / 5.0f);
        for (int i = 0; i < 5; ++i) {
            const float s = static_cast<float>(i) / 4.0f;
            const Color cl = Motion::lerp_linear(pair.c0, pair.c1, s);
            const Color co = kernetile::math::lerp_oklab(pair.c0, pair.c1, s);
            const float sx = static_cast<float>(i) * (step_w + 10.0f);
            ctx.draw_rect(Rect::from_xywh(sx, 176.0f, step_w, 18.0f), cl, 4.0f, 0.0f);
            ctx.draw_rect(Rect::from_xywh(sx, 198.0f, step_w, 18.0f), co, 4.0f, 0.0f);
        }
    }

private:
    int m_pair_idx{0};
};

struct OkLabGradientPreview : BindableMixin<OkLabGradientPreview, RenderOkLabGradientPreview> {
    int pair_idx{0};
    operator WidgetPtr() const;
};

class OkLabGradientPreviewWidget final : public Widget {
public:
    explicit OkLabGradientPreviewWidget(const OkLabGradientPreview& d) : m_d(d) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderOkLabGradientPreview>();
        n->set_pair_idx(m_d.pair_idx);
        return n;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderOkLabGradientPreview>(node)) {
            n->set_pair_idx(m_d.pair_idx);
        }
    }
private:
    OkLabGradientPreview m_d;
};

inline OkLabGradientPreview::operator WidgetPtr() const {
    return make_widget<OkLabGradientPreviewWidget>(*this);
}

class RenderOkLabThemeMorphPreview : public RenderBox {
public:
    void set_morph_t(float t) {
        float clamped = std::clamp(t, 0.0f, 1.0f);
        if (m_t != clamped) {
            m_t = clamped;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        m_size = m_constraints.constrain(Vec2{w, 134.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme dark = Theme::dark();
        const Theme light = Theme::light();
        const Theme morphed = Theme::lerp(dark, light, m_t);

        const float w = m_size.x;
        const float gap = 12.0f;
        const float left_w = std::max(160.0f, w * 0.54f);
        const float right_x = left_w + gap;
        const float right_w = std::max(140.0f, w - right_x);

        const Rect card = Rect::from_xywh(0.0f, 0.0f, left_w, 134.0f);
        ctx.draw_box(card, morphed.colors.surface, morphed.colors.border, 8.0f, 1.0f);
        ctx.push_clip(card, 8.0f);
        ctx.draw_text("Theme::lerp(Dark, Light, t) — OkLab 主题态", PointF{14.0f, 12.0f}, 12.5f,
                      morphed.colors.text_primary, FontWeight::SemiBold);
        ctx.draw_text("背景、文字、描边与强调色沿感知均匀轨迹平滑过渡", PointF{14.0f, 34.0f}, 11.5f,
                      morphed.colors.text_secondary);

        const Rect pill = Rect::from_xywh(14.0f, 68.0f, 124.0f, 34.0f);
        ctx.draw_rect(pill, morphed.colors.primary, 6.0f, 0.0f);
        ctx.draw_text("Primary Accent", PointF{26.0f, 77.0f}, 12.0f, morphed.colors.on_primary, FontWeight::SemiBold);

        const Rect chip = Rect::from_xywh(148.0f, 68.0f, std::max(60.0f, left_w - 162.0f), 34.0f);
        ctx.draw_box(chip, morphed.colors.control_fill_default, morphed.colors.control_stroke_default, 6.0f, 1.0f);
        ctx.draw_text("Semantic Surface", PointF{158.0f, 77.0f}, 12.0f, morphed.colors.text_primary);
        ctx.pop_clip();

        const Theme& cur_t = ThemeContext::current();
        const Rect st_box = Rect::from_xywh(right_x, 0.0f, right_w, 134.0f);
        ctx.draw_box(st_box, cur_t.colors.card_surface, cur_t.colors.card_stroke, 8.0f, 1.0f);
        ctx.push_clip(st_box, 8.0f);

        const Color state_a{0.0f, 0.42f, 1.0f, 1.0f};
        const Color state_b{1.0f, 0.68f, 0.0f, 1.0f};
        const Color lin_col = Motion::lerp_linear(state_a, state_b, m_t);
        const Color ok_col = Motion::lerp(state_a, state_b, m_t);

        ctx.draw_text("状态过渡插值 (t 同步驱动)", PointF{right_x + 12.0f, 12.0f}, 12.0f, cur_t.colors.text_primary, FontWeight::SemiBold);
        ctx.draw_rect(Rect::from_xywh(right_x + 12.0f, 42.0f, std::max(40.0f, right_w - 24.0f), 34.0f), lin_col, 6.0f, 0.0f);
        ctx.draw_text("Motion::lerp_linear (中段发灰)", PointF{right_x + 22.0f, 51.0f}, 11.5f, Color{1, 1, 1, 0.94f}, FontWeight::Medium);

        ctx.draw_rect(Rect::from_xywh(right_x + 12.0f, 84.0f, std::max(40.0f, right_w - 24.0f), 34.0f), ok_col, 6.0f, 0.0f);
        ctx.draw_text("Motion::lerp (OkLab 鲜亮)", PointF{right_x + 22.0f, 93.0f}, 11.5f, Color{1, 1, 1, 0.94f}, FontWeight::SemiBold);
        ctx.pop_clip();
    }

private:
    float m_t{0.5f};
};

struct OkLabThemeMorphPreview : BindableMixin<OkLabThemeMorphPreview, RenderOkLabThemeMorphPreview> {
    float morph_t{0.5f};
    operator WidgetPtr() const;
};

class OkLabThemeMorphPreviewWidget final : public Widget {
public:
    explicit OkLabThemeMorphPreviewWidget(const OkLabThemeMorphPreview& d) : m_d(d) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderOkLabThemeMorphPreview>();
        n->set_morph_t(m_d.morph_t);
        return n;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderOkLabThemeMorphPreview>(node)) {
            n->set_morph_t(m_d.morph_t);
        }
    }
private:
    OkLabThemeMorphPreview m_d;
};

inline OkLabThemeMorphPreview::operator WidgetPtr() const {
    return make_widget<OkLabThemeMorphPreviewWidget>(*this);
}

// ============================================================================
// 页面 4: 零纹理亚克力微颗粒噪点 (Zero-Texture Acrylic Micro-Grain)
// ============================================================================
class RenderAcrylicGrainPreview final : public RenderBox {
public:
    void set_grain_amount(float a) {
        float c = std::clamp(a, 0.0f, 0.20f);
        if (m_amount != c) { m_amount = c; mark_needs_paint(); }
    }
    void set_grain_scale(float s) {
        float c = std::clamp(s, 1.0f, 4.0f);
        if (m_scale != c) { m_scale = c; mark_needs_paint(); }
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        m_size = m_constraints.constrain(Vec2{w, 232.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const float w = m_size.x;
        const float h = m_size.y;

        ctx.push_clip(bounds(), 10.0f);
        if (is_dark) {
            ctx.draw_linear_gradient(bounds(),
                Color{0.06f, 0.09f, 0.16f, 1.0f},
                Color{0.16f, 0.09f, 0.22f, 1.0f},
                PointF{0, 0}, PointF{w, h}, 10.0f, 0.0f, true);
        } else {
            ctx.draw_linear_gradient(bounds(),
                Color{0.80f, 0.88f, 0.98f, 1.0f},
                Color{0.94f, 0.84f, 0.94f, 1.0f},
                PointF{0, 0}, PointF{w, h}, 10.0f, 0.0f, true);
        }

        // 高饱和透底色团体现纯渐变对比
        ctx.draw_rect(Rect::from_xywh(w * 0.14f, 34.0f, 150.0f, 150.0f),
                      Color{0.20f, 0.62f, 1.0f, is_dark ? 0.34f : 0.42f}, 75.0f, 0.0f);
        ctx.draw_rect(Rect::from_xywh(w * 0.60f, 44.0f, 164.0f, 164.0f),
                      Color{0.92f, 0.36f, 0.58f, is_dark ? 0.30f : 0.36f}, 82.0f, 0.0f);

        const float pad = 18.0f;
        const float gap = 16.0f;
        const float col_w = std::max(140.0f, (w - pad * 2.0f - gap) * 0.5f);
        const float card_h = h - pad * 2.0f;
        const Color glass_tint = is_dark
            ? Color{0.13f, 0.15f, 0.20f, 0.72f}
            : Color{0.94f, 0.96f, 0.99f, 0.72f};

        // 左侧: 无噪点纯数学磨砂玻璃
        const Rect left_r = Rect::from_xywh(pad, pad, col_w, card_h);
        ctx.draw_elevation_shadow(left_r, 12.0f, is_dark, 20.0f, 0.75f);
        ctx.draw_rect(left_r, glass_tint, 12.0f, 0.0f);
        ctx.draw_directional_rim(left_r,
            Color{1.0f, 1.0f, 1.0f, is_dark ? 0.26f : 0.70f},
            Color{0.0f, 0.0f, 0.0f, is_dark ? 0.28f : 0.12f},
            12.0f, 1.0f, Vec2{-0.18f, -0.98f}, 0.25f);
        ctx.draw_text("无颗粒 · 纯光滑玻璃填充", PointF{pad + 16.0f, pad + 18.0f}, 13.0f,
                      t.colors.text_primary, FontWeight::SemiBold);
        ctx.draw_text("平滑线性混合在大面积浅灰/深蓝", PointF{pad + 16.0f, pad + 44.0f}, 11.5f, t.colors.text_secondary);
        ctx.draw_text("面板上容易出现轻微塑料感与色带", PointF{pad + 16.0f, pad + 62.0f}, 11.5f, t.colors.text_secondary);

        // 右侧: 零纹理 PCG 空间哈希亚克力微颗粒
        const float rx = pad + col_w + gap;
        const Rect right_r = Rect::from_xywh(rx, pad, col_w, card_h);
        ctx.draw_elevation_shadow(right_r, 12.0f, is_dark, 20.0f, 0.75f);
        ctx.draw_acrylic_panel(right_r, glass_tint, 12.0f, m_amount, m_scale);
        ctx.draw_directional_rim(right_r,
            Color{1.0f, 1.0f, 1.0f, is_dark ? 0.34f : 0.85f},
            Color{0.0f, 0.0f, 0.0f, is_dark ? 0.32f : 0.14f},
            12.0f, 1.0f, Vec2{-0.18f, -0.98f}, 0.35f);
        ctx.draw_text("Zero-Texture Acrylic Micro-Grain", PointF{rx + 16.0f, pad + 18.0f}, 13.0f,
                      t.colors.text_primary, FontWeight::SemiBold);
        ctx.draw_text("逐像素确定性 PCG 三角分布微颗粒", PointF{rx + 16.0f, pad + 44.0f}, 11.5f, t.colors.text_secondary);
        ctx.draw_text("零显存纹理 · CPU / GPU 全后端像素对齐", PointF{rx + 16.0f, pad + 62.0f}, 11.5f, t.colors.text_secondary);

        char stats[64];
        std::snprintf(stats, sizeof(stats), "Amp=%.1f%%  Scale=%.1fx  VRAM=0 B", m_amount * 100.0f, m_scale);
        ctx.draw_text(stats, PointF{rx + 16.0f, pad + card_h - 28.0f}, 11.5f,
                      is_dark ? Color{0.52f, 0.86f, 1.0f, 0.94f} : Color{0.0f, 0.38f, 0.76f, 0.94f},
                      FontWeight::Medium, false, "monospace");

        ctx.pop_clip();
        ctx.draw_rect(bounds(), is_dark ? Color{1, 1, 1, 0.12f} : Color{0, 0, 0, 0.12f}, 10.0f, 1.0f);
    }

private:
    float m_amount{0.032f};
    float m_scale{1.0f};
};

struct AcrylicGrainPreview : BindableMixin<AcrylicGrainPreview, RenderAcrylicGrainPreview> {
    float grain_amount{0.032f};
    float grain_scale{1.0f};
    operator WidgetPtr() const;
};

class AcrylicGrainPreviewWidget final : public Widget {
public:
    explicit AcrylicGrainPreviewWidget(const AcrylicGrainPreview& d) : m_d(d) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderAcrylicGrainPreview>();
        n->set_grain_amount(m_d.grain_amount);
        n->set_grain_scale(m_d.grain_scale);
        return n;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderAcrylicGrainPreview>(node)) {
            n->set_grain_amount(m_d.grain_amount);
            n->set_grain_scale(m_d.grain_scale);
        }
    }
private:
    AcrylicGrainPreview m_d;
};

inline AcrylicGrainPreview::operator WidgetPtr() const {
    return make_widget<AcrylicGrainPreviewWidget>(*this);
}

// ============================================================================
// 页面 5: 解析高斯内阴影 (Analytic SDF Inset Shadow)
// ============================================================================
class RenderInsetShadowStage final : public RenderBox {
public:
    void set_blur(float b) { float c = std::clamp(b, 0.5f, 16.0f); if (m_blur != c) { m_blur = c; mark_needs_paint(); } }
    void set_spread(float s) { float c = std::clamp(s, -4.0f, 8.0f); if (m_spread != c) { m_spread = c; mark_needs_paint(); } }
    void set_offset_y(float y) { float c = std::clamp(y, -8.0f, 10.0f); if (m_offset_y != c) { m_offset_y = c; mark_needs_paint(); } }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        m_size = m_constraints.constrain(Vec2{w, 198.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const float w = m_size.x;
        const float gap = 14.0f;
        const float col_w = std::max(120.0f, (w - gap * 2.0f) / 3.0f);

        const Color well_bg = is_dark ? Color{0.10f, 0.115f, 0.145f, 1.0f} : Color{0.89f, 0.91f, 0.945f, 1.0f};
        const Color well_bd = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.08f} : Color{0.0f, 0.0f, 0.0f, 0.08f};
        const Color shadow_col = is_dark ? Color{0.0f, 0.0f, 0.0f, 0.62f} : Color{0.05f, 0.10f, 0.22f, 0.24f};

        struct SlotSpec {
            const char* title;
            float h;
            float radius;
            const char* subtitle;
        };
        const SlotSpec specs[3] = {
            {"输入凹槽 (R=8px)",   48.0f,  8.0f,  "TextBox / 搜索栏纵深"},
            {"仪表凹井 (R=16px)", 122.0f, 16.0f,  "G2 超椭圆贴合拐角"},
            {"胶囊滑轨 (R=26px)",  52.0f, 26.0f,  "Slider / Switch 滑槽"}
        };

        for (int i = 0; i < 3; ++i) {
            const float x = static_cast<float>(i) * (col_w + gap);
            const Rect outer = Rect::from_xywh(x, 0.0f, col_w, 198.0f);
            ctx.draw_box(outer, t.colors.card_surface, t.colors.card_stroke, 10.0f, 1.0f);
            ctx.draw_text(specs[i].title, PointF{x + 14.0f, 14.0f}, 12.5f, t.colors.text_primary, FontWeight::SemiBold);
            ctx.draw_text(specs[i].subtitle, PointF{x + 14.0f, 34.0f}, 11.0f, t.colors.text_secondary);

            const float slot_y = 60.0f + (124.0f - specs[i].h) * 0.5f;
            const Rect slot = Rect::from_xywh(x + 14.0f, slot_y, col_w - 28.0f, specs[i].h);
            ctx.draw_rect(slot, well_bg, specs[i].radius, 0.0f);
            ctx.draw_inset_shadow(slot, shadow_col, specs[i].radius, m_blur, m_spread, Vec2{0.0f, m_offset_y});
            ctx.draw_rect(slot, well_bd, specs[i].radius, 1.0f);
        }
    }

private:
    float m_blur{6.5f};
    float m_spread{1.0f};
    float m_offset_y{2.5f};
};

struct InsetShadowStage : BindableMixin<InsetShadowStage, RenderInsetShadowStage> {
    float blur{6.5f};
    float spread{1.0f};
    float offset_y{2.5f};
    operator WidgetPtr() const;
};

class InsetShadowStageWidget final : public Widget {
public:
    explicit InsetShadowStageWidget(const InsetShadowStage& d) : m_d(d) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderInsetShadowStage>();
        n->set_blur(m_d.blur);
        n->set_spread(m_d.spread);
        n->set_offset_y(m_d.offset_y);
        return n;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderInsetShadowStage>(node)) {
            n->set_blur(m_d.blur);
            n->set_spread(m_d.spread);
            n->set_offset_y(m_d.offset_y);
        }
    }
private:
    InsetShadowStage m_d;
};

inline InsetShadowStage::operator WidgetPtr() const {
    return make_widget<InsetShadowStageWidget>(*this);
}

// ============================================================================
// 页面 6: 灵动岛式液态融合 (SDF Smooth-Minimum Metaballs)
// ============================================================================
class RenderMetaballsStage final : public RenderBox {
public:
    RenderMetaballsStage() {
        cursor_shape = CursorShape::Crosshair;
        on_pointer_move = [this](PointerEvent& ev) {
            if (m_pointer_follow) {
                m_pointer = ev.local_position;
                m_has_pointer = true;
                mark_needs_paint();
            }
        };
        on_pointer_exit = [this](PointerEvent&) {
            if (m_has_pointer) {
                m_has_pointer = false;
                mark_needs_paint();
            }
        };
    }

    void set_smooth_k(float k) { float c = std::clamp(k, 0.0f, 52.0f); if (m_k != c) { m_k = c; mark_needs_paint(); } }
    void set_separation(float d) { float c = std::clamp(d, 0.0f, 160.0f); if (m_sep != c) { m_sep = c; mark_needs_paint(); } }
    void set_pointer_follow(bool v) { if (m_pointer_follow != v) { m_pointer_follow = v; mark_needs_paint(); } }
    void set_two_tone(bool v) { if (m_two_tone != v) { m_two_tone = v; mark_needs_paint(); } }
    void set_outline_mode(bool v) { if (m_outline != v) { m_outline = v; mark_needs_paint(); } }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        m_size = m_constraints.constrain(Vec2{w, 224.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const float w = m_size.x;
        const float h = m_size.y;

        ctx.push_clip(bounds(), 10.0f);
        ctx.draw_linear_gradient(bounds(),
            is_dark ? Color{0.07f, 0.085f, 0.13f, 1.0f} : Color{0.93f, 0.95f, 0.99f, 1.0f},
            is_dark ? Color{0.11f, 0.08f, 0.17f, 1.0f} : Color{0.97f, 0.94f, 0.99f, 1.0f},
            PointF{0, 0}, PointF{w, h}, 10.0f, 0.0f, true);

        // 1. 灵动岛主胶囊 + 交互卫星胶囊
        const float island_w = 176.0f;
        const float island_h = 44.0f;
        const float island_x = w * 0.5f - island_w * 0.5f - 28.0f;
        const float island_y = 58.0f;
        const Rect island_a = Rect::from_xywh(island_x, island_y, island_w, island_h);

        Rect sat_b;
        const float sat_sz = 44.0f;
        if (m_pointer_follow && m_has_pointer && m_pointer.y < 142.0f) {
            sat_b = Rect::from_xywh(m_pointer.x - sat_sz * 0.5f, m_pointer.y - sat_sz * 0.5f, sat_sz, sat_sz);
        } else {
            sat_b = Rect::from_xywh(island_x + island_w + m_sep - 20.0f, island_y, sat_sz, sat_sz);
        }

        const Color col_a = is_dark ? Color{0.05f, 0.06f, 0.08f, 0.98f} : Color{0.10f, 0.13f, 0.19f, 0.96f};
        const Color col_b = m_two_tone
            ? Color{0.12f, 0.58f, 1.0f, 0.98f}
            : col_a;

        if (m_two_tone) {
            ctx.draw_metaball(island_a, island_h * 0.5f, sat_b, sat_sz * 0.5f, m_k,
                              col_a, col_b, m_outline ? 2.2f : 0.0f);
        } else {
            ctx.draw_metaball(island_a, island_h * 0.5f, sat_b, sat_sz * 0.5f, m_k,
                              col_a, m_outline ? 2.2f : 0.0f);
        }

        ctx.draw_text("Dynamic Island", PointF{island_x + 28.0f, island_y + 14.0f}, 12.5f,
                      Color{1.0f, 1.0f, 1.0f, 0.94f}, FontWeight::SemiBold);
        ctx.draw_text("●", PointF{sat_b.min_x + 17.0f, sat_b.min_y + 13.0f}, 12.0f,
                      Color{0.35f, 0.90f, 0.58f, 1.0f}, FontWeight::Bold);

        // 2. 下半区: 持续展示 OkLab 双色融合胶囊示例 (静止也呈现液态桥梁)
        const float y2 = 144.0f;
        const float mid_x = w * 0.5f;
        const Rect pair_a = Rect::from_xywh(mid_x - 148.0f, y2, 134.0f, 40.0f);
        const Rect pair_b = Rect::from_xywh(mid_x + 8.0f, y2, 118.0f, 40.0f);
        ctx.draw_metaball(pair_a, 20.0f, pair_b, 20.0f, std::max(14.0f, m_k),
                          Color{0.0f, 0.45f, 1.0f, 0.95f}, Color{1.0f, 0.24f, 0.62f, 0.95f}, 0.0f);
        ctx.draw_text("Primary Capsule", PointF{pair_a.min_x + 18.0f, y2 + 12.0f}, 11.5f, Color{1, 1, 1, 0.96f}, FontWeight::SemiBold);
        ctx.draw_text("Badge Orb", PointF{pair_b.min_x + 28.0f, y2 + 12.0f}, 11.5f, Color{1, 1, 1, 0.96f}, FontWeight::SemiBold);

        ctx.draw_text(m_pointer_follow ? "移动鼠标靠近上方灵动岛即可触发液态粘连拉丝；也可调节下方平滑半径 k 与分离距离" : "当前由分离距离滑块驱动",
                      PointF{16.0f, 14.0f}, 12.0f, t.colors.text_secondary);

        ctx.pop_clip();
        ctx.draw_rect(bounds(), is_dark ? Color{1, 1, 1, 0.12f} : Color{0, 0, 0, 0.12f}, 10.0f, 1.0f);
    }

private:
    float m_k{26.0f};
    float m_sep{42.0f};
    bool m_pointer_follow{true};
    bool m_two_tone{false};
    bool m_outline{false};
    bool m_has_pointer{false};
    PointF m_pointer{0.0f, 0.0f};
};

struct MetaballsStage : BindableMixin<MetaballsStage, RenderMetaballsStage> {
    float smooth_k{26.0f};
    float separation{42.0f};
    bool pointer_follow{true};
    bool two_tone{false};
    bool outline_mode{false};
    operator WidgetPtr() const;
};

class MetaballsStageWidget final : public Widget {
public:
    explicit MetaballsStageWidget(const MetaballsStage& d) : m_d(d) {}
    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderMetaballsStage>();
        n->set_smooth_k(m_d.smooth_k);
        n->set_separation(m_d.separation);
        n->set_pointer_follow(m_d.pointer_follow);
        n->set_two_tone(m_d.two_tone);
        n->set_outline_mode(m_d.outline_mode);
        return n;
    }
    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderMetaballsStage>(node)) {
            n->set_smooth_k(m_d.smooth_k);
            n->set_separation(m_d.separation);
            n->set_pointer_follow(m_d.pointer_follow);
            n->set_two_tone(m_d.two_tone);
            n->set_outline_mode(m_d.outline_mode);
        }
    }
private:
    MetaballsStage m_d;
};

inline MetaballsStage::operator WidgetPtr() const {
    return make_widget<MetaballsStageWidget>(*this);
}

} // namespace

// ============================================================================
// 独立页面构建函数 1: 文字光渗补偿 (tactile_font_gamma)
// ============================================================================
WidgetPtr build_tactile_font_gamma_page(GalleryApp& app) {
    auto state = std::make_shared<FontGammaPageState>();
    state->font_gamma_on.set(app.window ? app.window->font_gamma_enabled() : true);

    WidgetPtr scroll = app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("文字光渗补偿 (Irradiation Illusion Compensation)", 22.0f),
            app.make_muted(
                "根据前景与背景相对亮度极性（深底浅字 vs 浅底深字）及物理字号动态计算 R8 覆盖率 Gamma 权重，"
                "在 CPU 瓦片光栅器与 D3D12 / Vulkan / WebGPU 着色器中消除白字光渗虚胖与黑字发虚。",
                12.5f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 12.0f,
                        .content = {
                            ToggleSwitch {
                                .checked = state->font_gamma_on.get(),
                                .label = "全局文字光渗补偿 (Window::set_font_gamma_enabled)",
                                .on_change = [&app, state](bool on) {
                                    if (app.window) app.window->set_font_gamma_enabled(on);
                                }
                            }.bind_two_way(state->font_gamma_on),
                            Spacer {},
                            Text { .font_size = 12.0f, .tabular_nums = true }.bind(&RenderText::set_text, state->size_label),
                            Slider {
                                .value = state->sample_size.get(),
                                .min_value = 10.0f,
                                .max_value = 18.0f,
                                .width = 160.0f,
                                .on_change = [state](float v) {
                                    char buf[40];
                                    std::snprintf(buf, sizeof(buf), "示例字号: %.0f px", v);
                                    state->size_label.set(buf);
                                }
                            }.bind_two_way(state->sample_size)
                        }
                    },
                    FontGammaComparePreview {
                        .global_enabled = state->font_gamma_on.get(),
                        .sample_size = state->sample_size.get()
                    }.bind(state->font_gamma_on, [state](RenderFontGammaComparePreview& n, bool on) { n.set_enabled_hint(on); })
                     .bind(state->sample_size, [state](RenderFontGammaComparePreview& n, float sz) { n.set_sample_size(sz); })
                }
            }),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "多字号阶梯自适应衰减（小字强补偿，大字平滑收敛至 γ = 1.0）", .font_size = 14.0f, .font_weight = FontWeight::SemiBold },
                    app.make_muted("小字号（10px~14px）下光渗/发虚效应最为显著；随着物理像素字号增大，Gamma 自动线性收敛。", 12.0f),
                    FontGammaLadder {}
                }
            })
        }
    });

    return attach_page_state(std::move(state), std::move(scroll));
}

} // namespace gallery::pages

#include "gallery_pages.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

using namespace ktui;

namespace gallery::pages {

namespace {

// 交互式 G1 普通圆角 vs Apple G2 连续超椭圆叠加对比 + 曲率分布图控件
class RenderSquircleVisualizer : public RenderBox {
public:
    RenderSquircleVisualizer() = default;

    void set_radius(float r) {
        if (std::abs(m_radius - r) > 0.05f) {
            m_radius = r;
            mark_needs_paint();
        }
    }

    void set_show_circle_ref(bool show) {
        if (m_show_circle_ref != show) {
            m_show_circle_ref = show;
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 640.0f;
        m_size = m_constraints.constrain(Vec2{w, 224.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const Color panel_bg = is_dark ? Color{0.10f, 0.11f, 0.14f, 0.90f} : Color{0.96f, 0.97f, 0.99f, 1.0f};
        const Color grid_col = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.06f} : Color{0.0f, 0.0f, 0.0f, 0.06f};
        const Color border_col = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.10f} : Color{0.0f, 0.0f, 0.0f, 0.10f};
        const Color text_col = t.colors.text_primary;
        const Color muted_col = t.colors.text_secondary;

        const float gap = 14.0f;
        const float half_w = std::max(140.0f, (m_size.x - gap) * 0.5f);
        const float h = m_size.y;

        // ==================== 左侧：局部拐角放大重叠对比 ====================
        Rect left_rect = Rect::from_xywh(0.0f, 0.0f, half_w, h);
        ctx.draw_box(left_rect, panel_bg, border_col, 10.0f, 1.0f);

        // 背景细网格
        for (float gx = 20.0f; gx < half_w - 8.0f; gx += 20.0f) {
            ctx.draw_line(PointF{gx, 28.0f}, PointF{gx, h - 10.0f}, grid_col, 1.0f);
        }
        for (float gy = 40.0f; gy < h - 8.0f; gy += 20.0f) {
            ctx.draw_line(PointF{10.0f, gy}, PointF{half_w - 10.0f, gy}, grid_col, 1.0f);
        }

        ctx.draw_text("拐角轮廓放大对比（左上角 1/4 象限）", PointF{12.0f, 8.0f}, 12.0f, text_col, FontWeight::SemiBold);

        // 在左侧画布中放置一个放大的左上角（实际半径映射到显示半径 R_vis）
        const float ox = 36.0f;
        const float oy = 46.0f;
        const float box_w = std::min(half_w - 60.0f, 190.0f);
        const float box_h = h - 72.0f;
        const float min_side = std::min(box_w, box_h);
        const float norm_ratio = std::clamp(m_radius / 56.0f, 0.12f, 0.85f);
        const float r_vis = min_side * 0.55f * norm_ratio;
        const float r_eff = std::min(min_side * 0.92f, r_vis * 1.35f);

        // 原始直角参考虚框
        const Color corner_guide = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.16f} : Color{0.0f, 0.0f, 0.0f, 0.18f};
        ctx.draw_line(PointF{ox, oy}, PointF{ox + box_w, oy}, corner_guide, 1.0f);
        ctx.draw_line(PointF{ox, oy}, PointF{ox, oy + box_h}, corner_guide, 1.0f);

        // 1. 绘制 Apple G2 超椭圆轮廓 (L4: ((1-u)^4 + (1-v)^4) = 1, 延伸 r_eff)
        const Color sq_col{0.05f, 0.65f, 0.98f, 1.0f};
        const int segs = 36;
        // 顶边直线段到超椭圆切点
        ctx.draw_line(PointF{ox + r_eff, oy}, PointF{ox + box_w, oy}, sq_col, 2.4f);
        // 左边直线段到超椭圆切点
        ctx.draw_line(PointF{ox, oy + r_eff}, PointF{ox, oy + box_h}, sq_col, 2.4f);
        PointF prev_sq{ox, oy + r_eff};
        for (int i = 1; i <= segs; ++i) {
            float theta = (static_cast<float>(i) / static_cast<float>(segs)) * 1.5707963f;
            float c = std::cos(theta);
            float s = std::sin(theta);
            // 参数化 L4 超椭圆：u = cos^(1/2)(t), v = sin^(1/2)(t)
            float u = std::sqrt(std::max(0.0f, c));
            float v = std::sqrt(std::max(0.0f, s));
            PointF cur{ox + r_eff * (1.0f - u), oy + r_eff * (1.0f - v)};
            ctx.draw_line(prev_sq, cur, sq_col, 2.4f);
            prev_sq = cur;
        }

        // 2. 叠加传统 G1 圆形圆角轮廓 (红色)
        const Color cir_col{0.96f, 0.34f, 0.36f, 0.92f};
        if (m_show_circle_ref) {
            ctx.draw_line(PointF{ox + r_vis, oy}, PointF{ox + r_eff, oy}, cir_col, 1.6f);
            ctx.draw_line(PointF{ox, oy + r_vis}, PointF{ox, oy + r_eff}, cir_col, 1.6f);
            PointF prev_c{ox, oy + r_vis};
            for (int i = 1; i <= segs; ++i) {
                float theta = (static_cast<float>(i) / static_cast<float>(segs)) * 1.5707963f;
                PointF cur{ox + r_vis * (1.0f - std::cos(theta)), oy + r_vis * (1.0f - std::sin(theta))};
                ctx.draw_line(prev_c, cur, cir_col, 1.6f);
                prev_c = cur;
            }
            // 标注普通圆角折痕切点（r 处）
            ctx.draw_rect(Rect::from_xywh(ox + r_vis - 3.0f, oy - 3.0f, 6.0f, 6.0f), cir_col, 3.0f);
            ctx.draw_rect(Rect::from_xywh(ox - 3.0f, oy + r_vis - 3.0f, 6.0f, 6.0f), cir_col, 3.0f);
        }

        // 标注超椭圆切点（1.35r 处，曲率=0）
        ctx.draw_rect(Rect::from_xywh(ox + r_eff - 3.5f, oy - 3.5f, 7.0f, 7.0f), sq_col, 3.5f);
        ctx.draw_rect(Rect::from_xywh(ox - 3.5f, oy + r_eff - 3.5f, 7.0f, 7.0f), sq_col, 3.5f);

        // 底部图例
        ctx.draw_rect(Rect::from_xywh(14.0f, h - 22.0f, 14.0f, 3.0f), sq_col, 1.5f);
        ctx.draw_text("Apple G2 超椭圆 (1.35r 平滑切入)", PointF{33.0f, h - 27.0f}, 11.0f, text_col);
        if (m_show_circle_ref) {
            ctx.draw_rect(Rect::from_xywh(half_w - 146.0f, h - 22.0f, 14.0f, 3.0f), cir_col, 1.5f);
            ctx.draw_text("传统 G1 圆角 (r 处折痕)", PointF{half_w - 127.0f, h - 27.0f}, 11.0f, muted_col);
        }

        // ==================== 右侧：曲率分布曲线 κ(s) ====================
        const float rx = half_w + gap;
        Rect right_rect = Rect::from_xywh(rx, 0.0f, half_w, h);
        ctx.draw_box(right_rect, panel_bg, border_col, 10.0f, 1.0f);

        ctx.draw_text("曲率变化曲线 κ(s)（折痕的数学根源）", PointF{rx + 12.0f, 8.0f}, 12.0f, text_col, FontWeight::SemiBold);

        const float px0 = rx + 26.0f;
        const float px1 = rx + half_w - 18.0f;
        const float py0 = 42.0f;
        const float py1 = h - 42.0f;
        const float pw = std::max(40.0f, px1 - px0);
        const float ph = std::max(40.0f, py1 - py0);

        // 坐标轴
        const Color axis_col = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.25f} : Color{0.0f, 0.0f, 0.0f, 0.25f};
        ctx.draw_line(PointF{px0, py1}, PointF{px1, py1}, axis_col, 1.0f);
        ctx.draw_line(PointF{px0, py0}, PointF{px0, py1}, axis_col, 1.0f);
        ctx.draw_text("κ", PointF{px0 - 14.0f, py0 - 4.0f}, 11.0f, muted_col);
        ctx.draw_text("直边", PointF{px0 + 4.0f, py1 + 5.0f}, 10.5f, muted_col);
        ctx.draw_text("45° 拐角中心", PointF{px0 + pw * 0.5f - 32.0f, py1 + 5.0f}, 10.5f, muted_col);
        ctx.draw_text("直边", PointF{px1 - 24.0f, py1 + 5.0f}, 10.5f, muted_col);

        // 1. 传统 G1 圆角曲率：方波阶跃（0 -> 1/r 突变 -> 0）
        if (m_show_circle_ref) {
            const float step_x0 = px0 + pw * 0.24f;
            const float step_x1 = px0 + pw * 0.76f;
            const float step_y  = py1 - ph * 0.62f;
            ctx.draw_line(PointF{px0, py1}, PointF{step_x0, py1}, cir_col, 1.6f);
            ctx.draw_line(PointF{step_x0, py1}, PointF{step_x0, step_y}, cir_col, 1.6f);
            ctx.draw_line(PointF{step_x0, step_y}, PointF{step_x1, step_y}, cir_col, 1.6f);
            ctx.draw_line(PointF{step_x1, step_y}, PointF{step_x1, py1}, cir_col, 1.6f);
            ctx.draw_line(PointF{step_x1, py1}, PointF{px1, py1}, cir_col, 1.6f);
            ctx.draw_text("阶跃突变 -> 视觉折痕", PointF{step_x0 + 6.0f, step_y - 16.0f}, 10.5f, cir_col);
        }

        // 2. Apple G2 超椭圆曲率：从 0 平滑连续上升至峰值再回落到 0（sin^2 钟形，完美 G2 连续）
        const int c_segs = 48;
        PointF prev_k{px0, py1};
        for (int i = 1; i <= c_segs; ++i) {
            float u = static_cast<float>(i) / static_cast<float>(c_segs);
            float x = px0 + pw * u;
            float k_norm = 0.0f;
            if (u > 0.08f && u < 0.92f) {
                float s = (u - 0.08f) / 0.84f; // 0..1
                float sn = std::sin(s * 3.14159265f);
                k_norm = sn * sn; // C1/G2 连续钟形曲率分布
            }
            PointF cur{x, py1 - ph * 0.86f * k_norm};
            ctx.draw_line(prev_k, cur, sq_col, 2.4f);
            prev_k = cur;
        }
        ctx.draw_text("G2 零曲率平滑起落", PointF{px0 + pw * 0.5f - 48.0f, py0 + 2.0f}, 10.5f, sq_col, FontWeight::SemiBold);
    }

private:
    float m_radius{24.0f};
    bool m_show_circle_ref{true};
};

struct SquircleVisualizer : BindableMixin<SquircleVisualizer, RenderSquircleVisualizer> {
    float radius{24.0f};
    bool show_circle_ref{true};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class SquircleVisualizerWidget : public Widget {
public:
    explicit SquircleVisualizerWidget(const SquircleVisualizer& d) : m_desc(d) {}
    explicit SquircleVisualizerWidget(SquircleVisualizer&& d) noexcept : m_desc(std::move(d)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto n = make_render_node<RenderSquircleVisualizer>();
        n->set_radius(m_desc.radius);
        n->set_show_circle_ref(m_desc.show_circle_ref);
        return n;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto n = std::static_pointer_cast<RenderSquircleVisualizer>(node)) {
            n->set_radius(m_desc.radius);
            n->set_show_circle_ref(m_desc.show_circle_ref);
        }
    }

private:
    SquircleVisualizer m_desc;
};

inline SquircleVisualizer::operator WidgetPtr() const& { return make_widget<SquircleVisualizerWidget>(*this); }
inline SquircleVisualizer::operator WidgetPtr() && { return make_widget<SquircleVisualizerWidget>(std::move(*this)); }

} // namespace

WidgetPtr build_squircle_page(GalleryApp& app) {
    auto radius_prop = std::make_shared<Property<float>>(24.0f);
    auto border_prop = std::make_shared<Property<float>>(2.0f);
    auto show_ref_prop = std::make_shared<Property<bool>>(true);
    auto radius_label = std::make_shared<Property<std::string>>("当前圆角半径: 24 px  (延伸过渡区 1.35r ≈ 32 px)");
    auto border_label = std::make_shared<Property<std::string>>("等宽描边: 2.0 px");

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("Apple 风格 G² 连续曲率超椭圆圆角 (Squircle)", 22.0f),
            app.make_muted(
                "KerneTile 已在底层 SDF 管线（D3D12 / Vulkan / WebGPU / CPU 全后端）全局启用 Apple 风格连续曲率超椭圆圆角。"
                "相比传统 G¹ 圆弧在切点处的曲率突变（视觉折痕），超椭圆在直边交界处曲率严格为 0，并具备自适应正圆/胶囊保护。",
                12.5f),

            // 1. 原理可视化：拐角轮廓叠加 + 曲率曲线
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 12.0f,
                        .content = {
                            Text { .text = "为什么普通圆角会有“折痕”？", .font_size = 14.5f, .font_weight = FontWeight::SemiBold },
                            Spacer {},
                            ToggleSwitch {
                                .checked = true,
                                .label = "叠加传统 G¹ 圆角参考线",
                                .on_change = [show_ref_prop](bool on) {
                                    show_ref_prop->set(on);
                                }
                            }.bind_two_way(*show_ref_prop)
                        }
                    },
                    SquircleVisualizer {
                        .radius = 24.0f,
                        .show_circle_ref = true
                    }.bind(&RenderSquircleVisualizer::set_radius, *radius_prop)
                     .bind(&RenderSquircleVisualizer::set_show_circle_ref, *show_ref_prop)
                }
            }),

            // 2. 实时引擎 SDF 渲染演练场（真实 GPU/CPU 瓦片光栅化效果）
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "实时 SDF 渲染演练场（拖动滑块直观看拐角顺滑度与等宽描边）", .font_size = 14.5f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 16.0f,
                        .content = {
                            Expanded {
                                .child = VStack {
                                    .spacing = 4.0f,
                                    .content = {
                                        Text { .font_size = 12.0f }.bind(&RenderText::set_text, *radius_label),
                                        Slider {
                                            .min_value = 4.0f,
                                            .max_value = 46.0f,
                                            .step = 1.0f,
                                            .on_change = [radius_prop, radius_label](float v) {
                                                char buf[96];
                                                std::snprintf(buf, sizeof(buf),
                                                              "当前圆角半径: %.0f px  (延伸过渡区 1.35r ≈ %.0f px)",
                                                              static_cast<double>(v), static_cast<double>(v * 1.35f));
                                                radius_label->set(buf);
                                                (void)radius_prop;
                                            }
                                        }.bind_two_way(*radius_prop)
                                    }
                                }
                            },
                            Box {
                                .width = 210.0f,
                                .child = VStack {
                                    .spacing = 4.0f,
                                    .content = {
                                        Text { .font_size = 12.0f }.bind(&RenderText::set_text, *border_label),
                                        Slider {
                                            .min_value = 0.0f,
                                            .max_value = 6.0f,
                                            .step = 0.5f,
                                            .on_change = [border_prop, border_label](float v) {
                                                char buf[64];
                                                std::snprintf(buf, sizeof(buf), "等宽描边: %.1f px", static_cast<double>(v));
                                                border_label->set(buf);
                                                (void)border_prop;
                                            }
                                        }.bind_two_way(*border_prop)
                                    }
                                }
                            }
                        }
                    },

                    // 真实渲染样品展示行（仿 iOS/macOS 图标 + 卡片 + 描边框）
                    Wrap {
                        .spacing = 18.0f,
                        .run_spacing = 14.0f,
                        .content = {
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 108.0f,
                                        .height = 108.0f,
                                        .background_color = Color{0.04f, 0.52f, 1.0f, 1.0f},
                                        .corner_radius = 24.0f,
                                        .child = Text { .text = "App", .font_size = 20.0f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                        .alignment = Alignment::center()
                                    }.bind(&RenderDecoratedBox::set_corner_radius, *radius_prop),
                                    app.make_muted("macOS / iOS 图标风格", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 108.0f,
                                        .height = 108.0f,
                                        .background_color = Color{0.10f, 0.72f, 0.48f, 0.18f},
                                        .border_color = Color{0.10f, 0.78f, 0.52f, 0.95f},
                                        .border_width = 2.0f,
                                        .corner_radius = 24.0f,
                                        .child = Text { .text = "Stroke", .font_size = 15.0f, .font_weight = FontWeight::SemiBold },
                                        .alignment = Alignment::center()
                                    }.bind(&RenderDecoratedBox::set_corner_radius, *radius_prop)
                                     .bind(&RenderDecoratedBox::set_border_width, *border_prop),
                                    app.make_muted("一阶梯度校正等宽描边", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 176.0f,
                                        .height = 108.0f,
                                        .background_color = Color{0.48f, 0.34f, 0.92f, 0.92f},
                                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.28f},
                                        .border_width = 1.5f,
                                        .corner_radius = 24.0f,
                                        .padding = EdgeInsets(14.0f),
                                        .child = VStack {
                                            .justify = JustifyContent::SpaceBetween,
                                            .content = {
                                                Text { .text = "Fluent Card", .font_size = 15.0f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                                Text { .text = "L4 Superellipse · 零 pow()", .font_size = 11.5f, .color = Color{1.0f, 1.0f, 1.0f, 0.82f} }
                                            }
                                        }
                                    }.bind(&RenderDecoratedBox::set_corner_radius, *radius_prop),
                                    app.make_muted("大圆角卡片无折痕过渡", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 160.0f,
                                        .height = 108.0f,
                                        .background_color = Color{0.92f, 0.42f, 0.16f, 0.92f},
                                        .corner_radius = 24.0f,
                                        .child = Box {
                                            .width = 124.0f,
                                            .height = 72.0f,
                                            .background_color = Color{1.0f, 1.0f, 1.0f, 0.22f},
                                            .border_color = Color{1.0f, 1.0f, 1.0f, 0.65f},
                                            .border_width = 1.5f,
                                            .corner_radius = 16.0f,
                                            .child = Text { .text = "同心嵌套", .font_size = 13.0f, .color = Color::white(), .font_weight = FontWeight::Medium },
                                            .alignment = Alignment::center()
                                        },
                                        .alignment = Alignment::center()
                                    }.bind(&RenderDecoratedBox::set_corner_radius, *radius_prop),
                                    app.make_muted("嵌套圆角视觉贴合", 11.5f)
                                }
                            }
                        }
                    }
                }
            }),

            // 3. 智能正圆 / 胶囊保护展示
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "智能正圆 / 胶囊自适应保护（ratio = r / min_half）", .font_size = 14.5f, .font_weight = FontWeight::SemiBold },
                    app.make_muted("当圆角半径逼近短边一半（ratio ≥ 0.90）时自动平滑回退至 L₂ 标准正圆，确保 RadioButton、Slider 滑块、头像与胶囊按钮绝不被画方。", 12.0f),
                    Wrap {
                        .spacing = 22.0f,
                        .run_spacing = 12.0f,
                        .content = {
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 76.0f,
                                        .height = 76.0f,
                                        .background_color = Color{0.00f, 0.47f, 0.84f, 0.90f},
                                        .corner_radius = 16.0f,
                                        .child = Text { .text = "0.42", .font_size = 13.0f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                        .alignment = Alignment::center()
                                    },
                                    app.make_muted("100% 超椭圆", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 76.0f,
                                        .height = 76.0f,
                                        .background_color = Color{0.06f, 0.62f, 0.56f, 0.90f},
                                        .corner_radius = 29.0f,
                                        .child = Text { .text = "0.76", .font_size = 13.0f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                        .alignment = Alignment::center()
                                    },
                                    app.make_muted("平滑过渡区", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 76.0f,
                                        .height = 76.0f,
                                        .background_color = Color{0.84f, 0.30f, 0.36f, 0.90f},
                                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.45f},
                                        .border_width = 2.0f,
                                        .corner_radius = 38.0f,
                                        .child = Text { .text = "1.00", .font_size = 13.0f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                        .alignment = Alignment::center()
                                    },
                                    app.make_muted("100% 标准正圆", 11.5f)
                                }
                            },
                            VStack {
                                .align_items = AlignItems::Center,
                                .spacing = 6.0f,
                                .content = {
                                    Box {
                                        .width = 148.0f,
                                        .height = 44.0f,
                                        .background_color = Color{0.52f, 0.34f, 0.88f, 0.92f},
                                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.35f},
                                        .border_width = 1.5f,
                                        .corner_radius = 22.0f,
                                        .child = Text { .text = "Pill 胶囊 1.00", .font_size = 12.5f, .color = Color::white(), .font_weight = FontWeight::SemiBold },
                                        .alignment = Alignment::center()
                                    },
                                    app.make_muted("胶囊按钮完美保圆", 11.5f)
                                }
                            }
                        }
                    }
                }
            }),

            // 4. 工程实现摘要
            SettingsCard {
                .header = "全后端解析 SDF 零开销实现",
                .description = "D3D12 (HLSL) · Vulkan (SPIR-V) · WebGPU (WGSL) · CPU 多线程瓦片光栅器像素级一致，全程零 pow() 指令，覆盖矩形、描边、图片圆角与裁剪栈。",
                .content = Text { .text = "G² L₄ Squircle", .font_size = 12.0f, .font_weight = FontWeight::SemiBold }
            }
        }
    });
}

} // namespace gallery::pages

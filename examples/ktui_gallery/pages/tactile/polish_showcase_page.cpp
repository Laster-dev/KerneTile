#include "../gallery_pages.hpp"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

// 对比预览：连续暗色柔和渐变条，方便直观看出 8-bit IGN 抖动消除色带的效果
class RenderGradientBandingPreview : public RenderBox {
public:
    RenderGradientBandingPreview() = default;

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 560.0f;
        m_size = m_constraints.constrain(Vec2{w, 72.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const float w = m_size.x;
        const float h = m_size.y;
        const int steps = 64;
        const float seg_w = w / static_cast<float>(steps);

        ctx.push_clip(bounds(), 6.0f);
        for (int i = 0; i < steps; ++i) {
            float u = static_cast<float>(i) / static_cast<float>(steps - 1);
            // 极微暗灰/深蓝渐变（亮度跨度仅 ~18 LSB），最容易暴露 8-bit 量化阶梯色带
            float r = t.is_dark ? (0.08f + u * 0.09f) : (0.82f + u * 0.12f);
            float g = t.is_dark ? (0.09f + u * 0.11f) : (0.85f + u * 0.11f);
            float b = t.is_dark ? (0.12f + u * 0.15f) : (0.90f + u * 0.08f);
            ctx.draw_rect(Rect::from_xywh(static_cast<float>(i) * seg_w, 0.0f, seg_w + 0.5f, h),
                          Color{r, g, b, 1.0f});
        }
        ctx.pop_clip();

        Color bd = t.is_dark ? Color{1.0f, 1.0f, 1.0f, 0.14f} : Color{0.0f, 0.0f, 0.0f, 0.14f};
        ctx.draw_rect(bounds(), bd, 6.0f, 1.0f);
        ctx.draw_text("极微暗色连续渐变样本（约 18 LSB 跨度 · 开启 IGN 抖动后阶梯色带立即消失）",
                      PointF{12.0f, 26.0f}, 12.0f, t.colors.text_primary);
    }
};

struct GradientBandingPreview {
    operator WidgetPtr() const {
        return make_widget<SimplePreviewWidget>();
    }
private:
    class SimplePreviewWidget : public Widget {
    public:
        std::shared_ptr<RenderBox> create_render_node() override {
            return make_render_node<RenderGradientBandingPreview>();
        }
        void update_render_node(std::shared_ptr<RenderBox>) override {}
    };
};

// 对比预览：多组 1.0px 发丝线网格与细边框（用于直观验证 125%/150% 分数 DPI 物理像素吸附）
class RenderHairlineSnapPreview : public RenderBox {
public:
    RenderHairlineSnapPreview() = default;

    void perform_layout() override {
        float w = m_constraints.has_bounded_width() ? m_constraints.max_width : 560.0f;
        m_size = m_constraints.constrain(Vec2{w, 84.0f});
    }

    void paint(PaintContext& ctx) override {
        const Theme& t = ThemeContext::current();
        const bool is_dark = t.is_dark;
        const Color bg = is_dark ? Color{0.11f, 0.12f, 0.14f, 1.0f} : Color{0.97f, 0.98f, 0.99f, 1.0f};
        const Color line_col = is_dark ? Color{0.75f, 0.85f, 1.0f, 0.65f} : Color{0.10f, 0.25f, 0.45f, 0.65f};
        const Color bd_col = is_dark ? Color{1.0f, 1.0f, 1.0f, 0.35f} : Color{0.0f, 0.0f, 0.0f, 0.35f};

        ctx.draw_box(bounds(), bg, bd_col, 4.0f, 1.0f);

        // 刻意采用带小数的逻辑坐标间隔，在 125% / 150% DPI 下最易出现发虚
        for (int i = 0; i < 6; ++i) {
            float y = 14.3f + static_cast<float>(i) * 11.4f;
            ctx.draw_rect(Rect::from_xywh(16.3f, y, m_size.x - 32.6f, 1.0f), line_col);
        }
        for (int i = 0; i < 8; ++i) {
            float x = 24.3f + static_cast<float>(i) * 42.4f;
            ctx.draw_rect(Rect::from_xywh(x, 12.3f, 1.0f, m_size.y - 24.6f), line_col);
        }
        ctx.draw_text("1.0 px 非整数偏移发丝线网格（开启像素吸附后条纹粗细均匀、无半像素发虚）",
                      PointF{16.0f, m_size.y - 22.0f}, 11.5f, t.colors.text_primary);
    }
};

struct HairlineSnapPreview {
    operator WidgetPtr() const {
        return make_widget<SimpleHairlineWidget>();
    }
private:
    class SimpleHairlineWidget : public Widget {
    public:
        std::shared_ptr<RenderBox> create_render_node() override {
            return make_render_node<RenderHairlineSnapPreview>();
        }
        void update_render_node(std::shared_ptr<RenderBox>) override {}
    };
};

} // namespace

WidgetPtr build_polish_showcase_page(GalleryApp& app) {
    auto dither_on = std::make_shared<Property<bool>>(app.window ? app.window->dithering_enabled() : false);
    auto snap_on = std::make_shared<Property<bool>>(app.window ? app.window->pixel_snapping_enabled() : false);

    auto tnum_step = std::make_shared<int>(0);
    auto sample_counter = std::make_shared<Property<std::string>>("111,111.11");
    auto sample_fps = std::make_shared<Property<std::string>>("FPS: 111.1  |  Mem: 11.1 MB");
    auto reveal_status = std::make_shared<Property<std::string>>("将鼠标移过下方开启 Pointer Reveal 的按钮或设置卡片，查看边缘与表面随指针流动的径向高光。");

    std::vector<WidgetPtr> scroll_items;
    scroll_items.reserve(14);
    for (int i = 1; i <= 14; ++i) {
        scroll_items.push_back(Box {
            .border_color = Color{1.0f, 1.0f, 1.0f, 0.10f},
            .border_width = 1.0f,
            .corner_radius = 6.0f,
            .padding = EdgeInsets(8.0f, 12.0f),
            .theme_card = true,
            .child = HStack {
                .justify = JustifyContent::SpaceBetween,
                .align_items = AlignItems::Center,
                .content = {
                    Text { .text = "橡皮筋越界测试条目 #" + std::to_string(i), .font_size = 13.0f, .font_weight = FontWeight::Medium },
                    app.make_muted("滚到顶/底后继续滚轮或拖拽", 11.5f)
                }
            }
        });
    }

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("渲染质感与微交互特性 (Visual & Tactile Polish)", 22.0f),
            app.make_muted(
                "基础画质增强（IGN 蓝噪点抖动、分数 DPI 像素吸附、橡皮筋回弹）已默认深度融入框架并支持随时手动关闭；"
                "风格化交互（Pointer Reveal、Tabular Figures）支持按控件灵活开启。",
                12.5f),

            // 1. 8-bit IGN 抖动去色带
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 12.0f,
                        .content = {
                            VStack {
                                .spacing = 2.0f,
                                .content = {
                                    Text { .text = "1. 8-bit IGN 抖动去色带 (Interleaved Gradient Noise)", .font_size = 14.5f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
                                    app.make_muted("写入 RGBA8 前叠加 ±0.5 LSB 无纹理梯度噪声（仅 3 条算术指令），消除云母、大渐变与大半径高斯阴影的阶梯色带。", 12.0f)
                                }
                            },
                            Spacer {},
                            ToggleSwitch {
                                .checked = dither_on->get(),
                                .label = "启用 IGN 抖动",
                                .on_change = [&app, dither_on](bool on) {
                                    dither_on->set(on);
                                    if (app.window) app.window->set_dithering_enabled(on);
                                }
                            }.bind_two_way(*dither_on)
                        }
                    },
                    GradientBandingPreview {}
                }
            }),

            // 2. 分数 DPI 物理像素吸附
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 12.0f,
                        .content = {
                            VStack {
                                .spacing = 2.0f,
                                .content = {
                                    Text { .text = "2. 分数 DPI 物理像素吸附 (125% / 150% 锐利发丝线)", .font_size = 14.5f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
                                    app.make_muted("轴对齐 1px 边框与分割线宽度取整至至少 1 物理像素，直边吸附半像素网格，告别非整数缩放发虚。", 12.0f)
                                }
                            },
                            Spacer {},
                            ToggleSwitch {
                                .checked = snap_on->get(),
                                .label = "启用像素吸附",
                                .on_change = [&app, snap_on](bool on) {
                                    snap_on->set(on);
                                    if (app.window) app.window->set_pixel_snapping_enabled(on);
                                }
                            }.bind_two_way(*snap_on)
                        }
                    },
                    HairlineSnapPreview {}
                }
            }),

            // 3. 指针高光跟随 (Pointer Reveal / Specular Sheen)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "3. 指针高光跟随 (Fluent Pointer Reveal / Specular Sheen)", .font_size = 14.5f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, *reveal_status),
                    HStack {
                        .spacing = 10.0f,
                        .content = {
                            Button {
                                .text = "Accent + Pointer Reveal",
                                .variant = ButtonVariant::Accent,
                                .on_click = [reveal_status]() {
                                    reveal_status->set("点击了 Accent Reveal 按钮！仅光标周围 16x16 瓦片触发增量刷新。");
                                },
                                .enable_pointer_reveal = true
                            },
                            Button {
                                .text = "Standard + Pointer Reveal",
                                .variant = ButtonVariant::Standard,
                                .on_click = [reveal_status]() {
                                    reveal_status->set("点击了 Standard Reveal 按钮！边缘与表面泛起径向高光。");
                                },
                                .enable_pointer_reveal = true
                            },
                            Button {
                                .text = "默认按钮（未开启对照）",
                                .variant = ButtonVariant::Standard
                            }
                        }
                    },
                    SettingsCard {
                        .header = "支持 Pointer Reveal 的 SettingsCard（鼠标掠过试试）",
                        .description = "设置卡片显式开启 enable_pointer_reveal = true，指针移动时同步照亮外边框与表面。",
                        .is_click_enabled = true,
                        .on_click = [reveal_status]() {
                            reveal_status->set("触发了带 Pointer Reveal 的 SettingsCard 点击回调！");
                        },
                        .enable_pointer_reveal = true
                    }
                }
            }),

            // 4. 橡皮筋越界回弹 (Rubber-Band Overscroll)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    Text { .text = "4. 橡皮筋越界回弹 (Rubber-Band Overscroll)", .font_size = 14.5f, .font_weight = FontWeight::SemiBold },
                    app.make_muted("下方 ScrollView 已启用 rubber_band_overscroll = true：滑到顶/底后继续滚轮或拖拽，位移按 f(x) = (x*d*c)/(d+c*x) 阻尼拉扯并由二阶解析弹簧回弹。", 12.0f),
                    Box {
                        .border_color = Color{1.0f, 1.0f, 1.0f, 0.14f},
                        .border_width = 1.0f,
                        .corner_radius = 8.0f,
                        .padding = EdgeInsets(6.0f),
                        .child = ScrollView {
                            .height = 210.0f,
                            .child = VStack {
                                .align_items = AlignItems::Stretch,
                                .spacing = 6.0f,
                                .content = std::move(scroll_items)
                            },
                            .rubber_band_overscroll = true
                        }
                    }
                }
            }),

            // 5. 等宽数字排版 (Tabular Figures)
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            VStack {
                                .spacing = 2.0f,
                                .content = {
                                    Text { .text = "5. 等宽数字排版 (DirectWrite tnum / tabular_nums)", .font_size = 14.5f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
                                    app.make_muted("切换窄数字 “111,111.11” 与宽数字 “888,888.88” 对照横向稳定性。", 12.0f)
                                }
                            },
                            Spacer {},
                            Button {
                                .text = "切换数值 (1↔8↔随机)",
                                .variant = ButtonVariant::Accent,
                                .on_click = [tnum_step, sample_counter, sample_fps]() {
                                    static constexpr const char* kVals[] = {"111,111.11", "888,888.88", "104,892.57", "717,171.09"};
                                    static constexpr const char* kFps[]  = {
                                        "FPS: 111.1  |  Mem: 11.1 MB",
                                        "FPS: 888.8  |  Mem: 88.8 MB",
                                        "FPS: 144.0  |  Mem: 12.4 MB",
                                        "FPS: 060.0  |  Mem: 08.2 MB"
                                    };
                                    *tnum_step = (*tnum_step + 1) % 4;
                                    sample_counter->set(kVals[*tnum_step]);
                                    sample_fps->set(kFps[*tnum_step]);
                                },
                                .enable_pointer_reveal = true
                            }
                        }
                    },
                    HStack {
                        .spacing = 12.0f,
                        .content = {
                            Expanded {
                                .child = Box {
                                    .border_color = Color{0.92f, 0.35f, 0.35f, 0.40f},
                                    .border_width = 1.0f,
                                    .corner_radius = 6.0f,
                                    .padding = EdgeInsets(10.0f, 12.0f),
                                    .theme_card = true,
                                    .child = VStack {
                                        .spacing = 4.0f,
                                        .content = {
                                            app.make_muted("默认比例数字 (tabular_nums = false) — 会左右跳动", 11.5f),
                                            HStack {
                                                .spacing = 4.0f,
                                                .content = {
                                                    Text { .font_size = 20.0f, .font_weight = FontWeight::SemiBold, .tabular_nums = false }
                                                        .bind(&RenderText::set_text, *sample_counter),
                                                    Text { .text = "USD", .font_size = 13.0f }
                                                }
                                            },
                                            Text { .font_size = 12.5f, .tabular_nums = false }.bind(&RenderText::set_text, *sample_fps)
                                        }
                                    }
                                }
                            },
                            Expanded {
                                .child = Box {
                                    .border_color = Color{0.15f, 0.78f, 0.48f, 0.55f},
                                    .border_width = 1.0f,
                                    .corner_radius = 6.0f,
                                    .padding = EdgeInsets(10.0f, 12.0f),
                                    .theme_card = true,
                                    .child = VStack {
                                        .spacing = 4.0f,
                                        .content = {
                                            app.make_muted("等宽数字 (tabular_nums = true) — 零横向抖动", 11.5f),
                                            HStack {
                                                .spacing = 4.0f,
                                                .content = {
                                                    Text { .font_size = 20.0f, .font_weight = FontWeight::SemiBold, .tabular_nums = true }
                                                        .bind(&RenderText::set_text, *sample_counter),
                                                    Text { .text = "USD", .font_size = 13.0f }
                                                }
                                            },
                                            Text { .font_size = 12.5f, .tabular_nums = true }.bind(&RenderText::set_text, *sample_fps)
                                        }
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

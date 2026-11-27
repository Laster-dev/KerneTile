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
                    Text { .text = "壁纸铺底演示（ZStack + ImageStretch::UniformToFill，实时同步当前加载的图片/壁纸/GIF）", .font_size = 14.0f, .font_weight = FontWeight::SemiBold, .overflow = TextOverflow::Wrap },
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

} // namespace gallery::pages

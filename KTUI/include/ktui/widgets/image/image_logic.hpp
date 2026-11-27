#pragma once

#include "image_data.hpp"
#include "image_style.hpp"
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ktui {

// 彩色位图渲染节点 (RenderImage)
class RenderImage : public RenderBox {
public:
    RenderImage() noexcept = default;
    ~RenderImage() override = default;

    void set_source(std::shared_ptr<ImageBuffer> src) {
        if (m_source != src) {
            m_source = std::move(src);
            m_source_path.clear();
            m_frame_index = 0;
            m_last_frame_time = std::chrono::steady_clock::now();
            mark_needs_layout();
            mark_needs_paint();
        }
    }

    // 按路径加载 PNG / JPEG / BMP / GIF (自动去重缓存，路径不变时不重复解码)
    void set_source_file(const std::string& path, uint32_t max_dim = 2048) {
        if (m_source_path == path && m_source_max_dim == max_dim) return;
        m_source_path = path;
        m_source_max_dim = max_dim;
        m_frame_index = 0;
        m_last_frame_time = std::chrono::steady_clock::now();
        m_source = path.empty() ? nullptr : ImageBuffer::load_file(path, max_dim);
        mark_needs_layout();
        mark_needs_paint();
    }

    void set_auto_play(bool play) {
        if (m_auto_play != play) {
            m_auto_play = play;
            m_last_frame_time = std::chrono::steady_clock::now();
            mark_needs_paint();
        }
    }

    [[nodiscard]] bool auto_play() const noexcept { return m_auto_play; }

    [[nodiscard]] const std::shared_ptr<ImageBuffer>& source() const noexcept { return m_source; }

    void set_dimensions(float w, float h) {
        if (std::abs(m_width - w) > 0.001f || std::abs(m_height - h) > 0.001f) {
            m_width = w;
            m_height = h;
            mark_needs_layout();
        }
    }

    void set_stretch(ImageStretch s) {
        if (m_stretch != s) {
            m_stretch = s;
            mark_needs_paint();
        }
    }

    void set_corner_radius(float r) {
        if (std::abs(m_corner_radius - r) > 0.001f) {
            m_corner_radius = std::max(0.0f, r);
            mark_needs_paint();
        }
    }

    void set_tint(Color c) {
        if (m_tint != c) {
            m_tint = c;
            mark_needs_paint();
        }
    }

    void set_opacity(float op) {
        float clamped = std::clamp(op, 0.0f, 1.0f);
        if (std::abs(m_opacity - clamped) > 0.001f) {
            m_opacity = clamped;
            mark_needs_paint();
        }
    }

    void set_border(Color color, float width) {
        if (m_border_color != color || std::abs(m_border_width - width) > 0.001f) {
            m_border_color = color;
            m_border_width = std::max(0.0f, width);
            mark_needs_paint();
        }
    }

    void perform_layout() override {
        float w = m_width;
        float h = m_height;
        if (w <= 0.0f && m_source && m_source->valid()) {
            w = static_cast<float>(m_source->width);
        }
        if (h <= 0.0f && m_source && m_source->valid()) {
            h = static_cast<float>(m_source->height);
        }
        if (w <= 0.0f) w = 64.0f;
        if (h <= 0.0f) h = 64.0f;
        m_size = m_constraints.constrain(Vec2{w, h});
    }

    void paint(PaintContext& ctx) override {
        Rect box = bounds();
        if (box.width() <= 0.5f || box.height() <= 0.5f) return;

        if (!m_source || !m_source->valid()) {
            const auto& colors = ThemeContext::current().colors;
            ctx.draw_rect(box, colors.surface_variant, m_corner_radius, 0.0f);
            if (m_border_width > 0.0f && m_border_color.a > 0.001f) {
                ctx.draw_rect(box, m_border_color, m_corner_radius, m_border_width);
            }
            return;
        }

        const float img_w = static_cast<float>(m_source->width);
        const float img_h = static_cast<float>(m_source->height);
        const float box_w = box.width();
        const float box_h = box.height();

        Rect dst = box;
        Rect uv{0.0f, 0.0f, 1.0f, 1.0f};

        switch (m_stretch) {
            case ImageStretch::Fill:
                break;
            case ImageStretch::Uniform: {
                float scale = std::min(box_w / img_w, box_h / img_h);
                float dw = img_w * scale;
                float dh = img_h * scale;
                dst = Rect::from_xywh(box.min_x + (box_w - dw) * 0.5f,
                                      box.min_y + (box_h - dh) * 0.5f, dw, dh);
                break;
            }
            case ImageStretch::UniformToFill: {
                float img_aspect = img_w / img_h;
                float box_aspect = box_w / box_h;
                if (img_aspect > box_aspect) {
                    float vis_u = box_aspect / img_aspect;
                    float pad_u = (1.0f - vis_u) * 0.5f;
                    uv = Rect{pad_u, 0.0f, 1.0f - pad_u, 1.0f};
                } else if (img_aspect < box_aspect) {
                    float vis_v = img_aspect / box_aspect;
                    float pad_v = (1.0f - vis_v) * 0.5f;
                    uv = Rect{0.0f, pad_v, 1.0f, 1.0f - pad_v};
                }
                break;
            }
            case ImageStretch::None: {
                float dw = std::min(img_w, box_w);
                float dh = std::min(img_h, box_h);
                dst = Rect::from_xywh(box.min_x + (box_w - dw) * 0.5f,
                                      box.min_y + (box_h - dh) * 0.5f, dw, dh);
                if (img_w > box_w) {
                    float vis_u = box_w / img_w;
                    float pad_u = (1.0f - vis_u) * 0.5f;
                    uv.min_x = pad_u;
                    uv.max_x = 1.0f - pad_u;
                }
                if (img_h > box_h) {
                    float vis_v = box_h / img_h;
                    float pad_v = (1.0f - vis_v) * 0.5f;
                    uv.min_y = pad_v;
                    uv.max_y = 1.0f - pad_v;
                }
                break;
            }
        }

        Color effective_tint = m_tint;
        effective_tint.a *= m_opacity;
        if (effective_tint.a > 0.001f) {
            const uint64_t draw_key = m_source->cache_key;
            const uint8_t* draw_pixels = m_source->pixels.data();
            uint32_t frame_rev = 0;
            if (!m_source->frames.empty()) {
                if (m_source->is_animated() && m_auto_play) {
                    m_frame_index = m_source->sample_frame_index(std::chrono::steady_clock::now());
                    mark_needs_paint();
                } else if (m_frame_index >= m_source->frames.size()) {
                    m_frame_index = 0;
                }
                const auto& f = m_source->frames[m_frame_index];
                draw_pixels = f.pixels.data();
                frame_rev = static_cast<uint32_t>(m_frame_index);
            }
            if (draw_pixels) {
                ctx.draw_image(draw_key, dst, m_source->width, m_source->height,
                               draw_pixels, m_corner_radius, effective_tint, uv, frame_rev);
            }
        }

        if (m_border_width > 0.0f && m_border_color.a > 0.001f) {
            ctx.draw_rect(dst, m_border_color, m_corner_radius, m_border_width);
        }
    }

private:
    std::shared_ptr<ImageBuffer> m_source{};
    std::string m_source_path{};
    uint32_t m_source_max_dim{2048};
    size_t m_frame_index{0};
    std::chrono::steady_clock::time_point m_last_frame_time{};
    bool m_auto_play{true};
    float m_width{0.0f};
    float m_height{0.0f};
    ImageStretch m_stretch{ImageStretch::Uniform};
    float m_corner_radius{0.0f};
    Color m_tint{1.0f, 1.0f, 1.0f, 1.0f};
    float m_opacity{1.0f};
    Color m_border_color{0.0f, 0.0f, 0.0f, 0.0f};
    float m_border_width{0.0f};
};

// C++20 指定初始化器声明式 Image 组件
struct Image : BindableMixin<Image, RenderImage> {
    std::shared_ptr<ImageBuffer> source{};
    std::string src{""};       // 通用图片路径 (PNG / JPG / JPEG / BMP / GIF 含动图)
    std::string bmp_path{""};  // 兼容旧字段
    uint32_t max_dim{2048};    // 超大壁纸自动降采样最大边长 (0 = 不缩放)
    bool auto_play{true};      // GIF 动图自动播放开关
    float width{0.0f};
    float height{0.0f};
    ImageStretch stretch{ImageStretch::Uniform};
    float corner_radius{0.0f};
    Color tint{1.0f, 1.0f, 1.0f, 1.0f};
    float opacity{1.0f};
    Color border_color{0.0f, 0.0f, 0.0f, 0.0f};
    float border_width{0.0f};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class ImageWidget : public Widget {
public:
    explicit ImageWidget(const Image& desc) : m_desc(desc) {}
    explicit ImageWidget(Image&& desc) noexcept : m_desc(std::move(desc)) {}
    ~ImageWidget() override = default;

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderImage>();
        apply(node);
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto img = std::static_pointer_cast<RenderImage>(node)) {
            apply(img);
        }
    }

private:
    void apply(const std::shared_ptr<RenderImage>& img) {
        if (m_desc.source) {
            img->set_source(m_desc.source);
        } else if (!m_desc.src.empty()) {
            img->set_source_file(m_desc.src, m_desc.max_dim);
        } else if (!m_desc.bmp_path.empty()) {
            img->set_source_file(m_desc.bmp_path, m_desc.max_dim);
        } else {
            img->set_source(nullptr);
        }
        img->set_auto_play(m_desc.auto_play);
        img->set_dimensions(m_desc.width, m_desc.height);
        img->set_stretch(m_desc.stretch);
        img->set_corner_radius(m_desc.corner_radius);
        img->set_tint(m_desc.tint);
        img->set_opacity(m_desc.opacity);
        img->set_border(m_desc.border_color, m_desc.border_width);
    }

    Image m_desc;
};

inline Image::operator WidgetPtr() const& {
    return make_widget<ImageWidget>(*this);
}

inline Image::operator WidgetPtr() && {
    return make_widget<ImageWidget>(std::move(*this));
}
} // namespace ktui
 // namespace ktui

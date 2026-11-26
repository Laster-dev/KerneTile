#pragma once

#include <ktui/binding/binding.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/theme/theme_scope.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace ktui {

// 图像缩放填充模式 (与 WinUI 3 / XAML Stretch 语义一致)
enum class ImageStretch : uint8_t {
    None = 0,          // 按原始像素尺寸居中显示，不缩放
    Fill,              // 拉伸填满容器，不保持宽高比
    Uniform,           // 等比缩放完整显示在容器内 (Contain)
    UniformToFill      // 等比缩放填满容器，超出部分通过 UV 裁剪 (Cover)
};

// 动图单帧数据 (用于 GIF 多帧动画或帧序列)
struct ImageFrame {
    std::vector<uint8_t> pixels; // 紧密排布 RGBA8 (width * height * 4)
    uint32_t delay_ms{100};      // 帧持续时长 (ms)
};

// 轻量 RGBA8 位图/动图缓冲容器 (全平台支持 PNG / JPEG / BMP / GIF 静态与多帧动图，及内存像素构造)
struct ImageBuffer {
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint8_t> pixels;     // 静态图像素 (RGBA8, width * height * 4)
    uint64_t cache_key{0};
    std::vector<ImageFrame> frames;  // 动图多帧序列 (所有帧共享同一 cache_key，切帧时在图集原地覆写)
    uint64_t total_duration_ms{0};
    std::chrono::steady_clock::time_point anim_start_time{};

    [[nodiscard]] bool valid() const noexcept {
        if (width == 0 || height == 0) return false;
        const size_t expected = static_cast<size_t>(width) * height * 4u;
        if (!frames.empty()) {
            return frames.front().pixels.size() >= expected;
        }
        return pixels.size() >= expected;
    }

    [[nodiscard]] bool is_animated() const noexcept {
        return frames.size() > 1u;
    }

    [[nodiscard]] size_t frame_count() const noexcept {
        if (!frames.empty()) return frames.size();
        return valid() ? 1u : 0u;
    }

    // 基于共享播放时钟确定性计算当前帧索引，确保同一动图在多处显示时严格同步且零互相覆盖
    [[nodiscard]] size_t sample_frame_index(std::chrono::steady_clock::time_point now) const noexcept {
        if (frames.size() <= 1u || total_duration_ms == 0u) return 0u;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - anim_start_time).count();
        if (elapsed <= 0) return 0u;
        uint64_t t = static_cast<uint64_t>(elapsed) % total_duration_ms;
        uint64_t acc = 0;
        for (size_t i = 0; i < frames.size(); ++i) {
            acc += std::max<uint32_t>(16u, frames[i].delay_ms);
            if (t < acc) return i;
        }
        return frames.size() - 1u;
    }

    static uint64_t next_cache_key() noexcept {
        static std::atomic<uint64_t> s_counter{0x4B54494D47000001ull}; // "KTIMG..."
        return s_counter.fetch_add(1u, std::memory_order_relaxed);
    }

    // 从内存 RGBA8 字节数组创建共享图像缓冲
    static std::shared_ptr<ImageBuffer> from_rgba8(uint32_t w, uint32_t h, const uint8_t* rgba8) {
        if (w == 0 || h == 0 || !rgba8) return nullptr;
        auto buf = std::make_shared<ImageBuffer>();
        buf->width = w;
        buf->height = h;
        buf->pixels.assign(rgba8, rgba8 + static_cast<size_t>(w) * h * 4u);
        buf->cache_key = next_cache_key();
        return buf;
    }

    static std::shared_ptr<ImageBuffer> from_rgba8(uint32_t w, uint32_t h, std::vector<uint8_t> rgba8) {
        if (w == 0 || h == 0 || rgba8.size() < static_cast<size_t>(w) * h * 4u) return nullptr;
        auto buf = std::make_shared<ImageBuffer>();
        buf->width = w;
        buf->height = h;
        buf->pixels = std::move(rgba8);
        buf->cache_key = next_cache_key();
        return buf;
    }

    // 从多帧 RGBA8 序列构造共享动图缓冲
    static std::shared_ptr<ImageBuffer> from_frames(uint32_t w, uint32_t h, std::vector<ImageFrame> frame_list) {
        if (w == 0 || h == 0 || frame_list.empty()) return nullptr;
        const size_t expected = static_cast<size_t>(w) * h * 4u;
        uint64_t total_ms = 0;
        for (auto& f : frame_list) {
            if (f.pixels.size() < expected) return nullptr;
            if (f.delay_ms < 16u) f.delay_ms = 100u;
            total_ms += f.delay_ms;
        }
        auto buf = std::make_shared<ImageBuffer>();
        buf->width = w;
        buf->height = h;
        buf->cache_key = next_cache_key();
        buf->total_duration_ms = total_ms;
        buf->anim_start_time = std::chrono::steady_clock::now();
        buf->frames = std::move(frame_list);
        return buf;
    }

    // 跨平台统一文件解码 (PNG / JPEG / BMP / GIF 静态与多帧动图，自动支持 Windows UTF-8 路径；超大图按 max_dim 等比缩放)
    static std::shared_ptr<ImageBuffer> load_file(const std::string& file_path, uint32_t max_dim = 2048);

    // 跨平台统一内存字节解码 (PNG / JPEG / BMP / GIF 静态与多帧动图)
    static std::shared_ptr<ImageBuffer> load_memory(const uint8_t* data, size_t size, uint32_t max_dim = 2048);

    // 原地更新像素内容并刷新缓存键 (适用于摄像头/视频帧/动态画布)
    void update_pixels(const uint8_t* rgba8) {
        if (!rgba8 || width == 0 || height == 0) return;
        const size_t bytes = static_cast<size_t>(width) * height * 4u;
        pixels.assign(rgba8, rgba8 + bytes);
        cache_key = next_cache_key();
    }

    // 零依赖轻量 24-bit / 32-bit 未压缩 BMP 文件解码器
    static std::shared_ptr<ImageBuffer> load_bmp(const std::string& file_path) {
        std::FILE* fp = std::fopen(file_path.c_str(), "rb");
        if (!fp) return nullptr;
        uint8_t hdr[54]{};
        if (std::fread(hdr, 1, 54, fp) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
            std::fclose(fp);
            return nullptr;
        }
        auto rd_u32 = [](const uint8_t* p) -> uint32_t {
            return static_cast<uint32_t>(p[0]) |
                   (static_cast<uint32_t>(p[1]) << 8) |
                   (static_cast<uint32_t>(p[2]) << 16) |
                   (static_cast<uint32_t>(p[3]) << 24);
        };
        auto rd_i32 = [&](const uint8_t* p) -> int32_t {
            return static_cast<int32_t>(rd_u32(p));
        };
        uint32_t data_offset = rd_u32(hdr + 10);
        int32_t raw_w = rd_i32(hdr + 18);
        int32_t raw_h = rd_i32(hdr + 22);
        uint16_t bpp = static_cast<uint16_t>(hdr[28] | (hdr[29] << 8));
        uint32_t compression = rd_u32(hdr + 30);
        if (raw_w <= 0 || raw_h == 0 || (bpp != 24 && bpp != 32) ||
            (compression != 0 && compression != 3)) {
            std::fclose(fp);
            return nullptr;
        }
        uint32_t w = static_cast<uint32_t>(raw_w);
        bool top_down = raw_h < 0;
        uint32_t h = static_cast<uint32_t>(top_down ? -raw_h : raw_h);
        if (w > 4096 || h > 4096) {
            std::fclose(fp);
            return nullptr;
        }
        if (std::fseek(fp, static_cast<long>(data_offset), SEEK_SET) != 0) {
            std::fclose(fp);
            return nullptr;
        }
        const uint32_t src_bpp = bpp / 8u;
        const size_t row_stride = ((static_cast<size_t>(w) * src_bpp) + 3u) & ~size_t(3u);
        std::vector<uint8_t> row_buf(row_stride);
        std::vector<uint8_t> out(static_cast<size_t>(w) * h * 4u);

        for (uint32_t y = 0; y < h; ++y) {
            if (std::fread(row_buf.data(), 1, row_stride, fp) != row_stride) {
                std::fclose(fp);
                return nullptr;
            }
            uint32_t dst_y = top_down ? y : (h - 1u - y);
            uint8_t* dst_row = out.data() + static_cast<size_t>(dst_y) * w * 4u;
            for (uint32_t x = 0; x < w; ++x) {
                const uint8_t* sp = row_buf.data() + static_cast<size_t>(x) * src_bpp;
                dst_row[x * 4u + 0] = sp[2]; // R
                dst_row[x * 4u + 1] = sp[1]; // G
                dst_row[x * 4u + 2] = sp[0]; // B
                dst_row[x * 4u + 3] = (src_bpp == 4u) ? sp[3] : 255u;
            }
        }
        std::fclose(fp);
        return from_rgba8(w, h, std::move(out));
    }
};

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

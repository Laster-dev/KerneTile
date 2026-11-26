#pragma once

#include "kernetile/atlas/atlas_types.hpp"
#include "kernetile/atlas/shelf_packer.hpp"
#include <cstdint>
#include <vector>
#include <optional>
#include <span>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace kernetile::atlas {

class DynamicAtlas {
public:
    DynamicAtlas(uint32_t width, uint32_t height, AtlasFormat format)
        : m_width(width),
          m_height(height),
          m_format(format),
          m_bytes_per_pixel(format == AtlasFormat::RGBA8_UNORM ? 4 : 1),
          m_packer(width, height)
    {
        m_pixels.resize(static_cast<size_t>(m_width) * m_height * m_bytes_per_pixel, 0);
    }

    ~DynamicAtlas() = default;

    DynamicAtlas(const DynamicAtlas&) = delete;
    DynamicAtlas& operator=(const DynamicAtlas&) = delete;
    DynamicAtlas(DynamicAtlas&&) noexcept = default;
    DynamicAtlas& operator=(DynamicAtlas&&) noexcept = default;

    // 申请并在图集中分配一个带 1px 保护边距的区域，写入像素数据
    [[nodiscard]] std::optional<AtlasRegion> allocate_and_upload(
        uint32_t content_w, 
        uint32_t content_h, 
        const void* pixel_data, 
        size_t row_stride_bytes = 0,
        bool replicate_edge_border = false)
    {
        if (content_w == 0 || content_h == 0) return std::nullopt;

        // 强制加入 1 像素防渗色安全边距 (四边各 +1px，总宽高各 +2px)
        uint32_t total_w = content_w + 2;
        uint32_t total_h = content_h + 2;

        auto total_rect_opt = m_packer.allocate(total_w, total_h);
        if (!total_rect_opt.has_value()) {
            m_overflowed = true;
            return std::nullopt;
        }

        const auto& total_rect = *total_rect_opt;
        uint32_t content_x = total_rect.x + 1;
        uint32_t content_y = total_rect.y + 1;

        if (row_stride_bytes == 0) {
            row_stride_bytes = content_w * m_bytes_per_pixel;
        }

        // 1 & 2. 写入主体像素与 1px 防渗色边距
        write_pixels_into_region(total_rect, content_x, content_y, content_w, content_h,
                                 pixel_data, row_stride_bytes, replicate_edge_border);

        // 3. 记录增量脏矩形以供 GPU 纹理上传
        m_dirty_rects.push_back(total_rect);

        // 4. 构建定点化 UV 映射坐标 (严格对齐内缩真实内容矩形)
        AtlasRegion region;
        region.total_rect = total_rect;
        region.content_rect = RectU32{content_x, content_y, content_w, content_h};

        float inv_w = 1.0f / static_cast<float>(m_width);
        float inv_h = 1.0f / static_cast<float>(m_height);

        float norm_u0 = static_cast<float>(content_x) * inv_w;
        float norm_v0 = static_cast<float>(content_y) * inv_h;
        float norm_u1 = static_cast<float>(content_x + content_w) * inv_w;
        float norm_v1 = static_cast<float>(content_y + content_h) * inv_h;

        region.u0 = AtlasRegion::to_fixed_uv(norm_u0);
        region.v0 = AtlasRegion::to_fixed_uv(norm_v0);
        region.u1 = AtlasRegion::to_fixed_uv(norm_u1);
        region.v1 = AtlasRegion::to_fixed_uv(norm_v1);

        return region;
    }

    // 原地覆写已分配图集槽位像素 (专为动图 GIF / 视频帧设计：零新槽位分配、零图集膨胀、零 reset 频闪)
    bool update_region(
        const AtlasRegion& region,
        const void* pixel_data,
        size_t row_stride_bytes = 0,
        bool replicate_edge_border = true)
    {
        if (!pixel_data) return false;
        const RectU32& total_rect = region.total_rect;
        const RectU32& c = region.content_rect;
        if (total_rect.right() > m_width || total_rect.bottom() > m_height ||
            c.w == 0 || c.h == 0) {
            return false;
        }
        if (row_stride_bytes == 0) {
            row_stride_bytes = c.w * m_bytes_per_pixel;
        }
        write_pixels_into_region(total_rect, c.x, c.y, c.w, c.h,
                                 pixel_data, row_stride_bytes, replicate_edge_border);
        if (m_dirty_rects.empty() ||
            m_dirty_rects.back().x != total_rect.x || m_dirty_rects.back().y != total_rect.y ||
            m_dirty_rects.back().w != total_rect.w || m_dirty_rects.back().h != total_rect.h) {
            m_dirty_rects.push_back(total_rect);
        }
        return true;
    }

    // 双线性插值采样 (CPU 端镜像验证，保证无渗色)
    [[nodiscard]] std::vector<float> sample_bilinear(float u, float v) const {
        float px = u * static_cast<float>(m_width) - 0.5f;
        float py = v * static_cast<float>(m_height) - 0.5f;

        int32_t x0 = static_cast<int32_t>(std::floor(px));
        int32_t y0 = static_cast<int32_t>(std::floor(py));
        int32_t x1 = x0 + 1;
        int32_t y1 = y0 + 1;

        float fx = px - static_cast<float>(x0);
        float fy = py - static_cast<float>(y0);

        x0 = std::clamp(x0, 0, static_cast<int32_t>(m_width - 1));
        y0 = std::clamp(y0, 0, static_cast<int32_t>(m_height - 1));
        x1 = std::clamp(x1, 0, static_cast<int32_t>(m_width - 1));
        y1 = std::clamp(y1, 0, static_cast<int32_t>(m_height - 1));

        auto get_px = [this](int32_t x, int32_t y) -> std::vector<float> {
            size_t offset = (static_cast<size_t>(y) * m_width + x) * m_bytes_per_pixel;
            if (m_format == AtlasFormat::R8_UNORM) {
                return { static_cast<float>(m_pixels[offset]) / 255.0f };
            } else {
                return {
                    static_cast<float>(m_pixels[offset + 0]) / 255.0f,
                    static_cast<float>(m_pixels[offset + 1]) / 255.0f,
                    static_cast<float>(m_pixels[offset + 2]) / 255.0f,
                    static_cast<float>(m_pixels[offset + 3]) / 255.0f
                };
            }
        };

        auto p00 = get_px(x0, y0);
        auto p10 = get_px(x1, y0);
        auto p01 = get_px(x0, y1);
        auto p11 = get_px(x1, y1);

        size_t channels = p00.size();
        std::vector<float> result(channels);
        for (size_t c = 0; c < channels; ++c) {
            float top = p00[c] * (1.0f - fx) + p10[c] * fx;
            float bottom = p01[c] * (1.0f - fx) + p11[c] * fx;
            result[c] = top * (1.0f - fy) + bottom * fy;
        }
        return result;
    }

    void reset() {
        m_packer.reset();
        std::fill(m_pixels.begin(), m_pixels.end(), uint8_t(0));
        m_dirty_rects.clear();
        m_dirty_rects.push_back(RectU32{0, 0, m_width, m_height});
        m_overflowed = false;
        ++m_generation;
    }

    [[nodiscard]] bool has_overflowed() const noexcept { return m_overflowed; }
    [[nodiscard]] uint64_t generation() const noexcept { return m_generation; }

    [[nodiscard]] std::span<const RectU32> get_dirty_rects() const noexcept {
        return m_dirty_rects;
    }

    void clear_dirty_rects() noexcept {
        m_dirty_rects.clear();
    }

    [[nodiscard]] uint32_t get_width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t get_height() const noexcept { return m_height; }
    [[nodiscard]] AtlasFormat get_format() const noexcept { return m_format; }
    [[nodiscard]] size_t get_bytes_per_pixel() const noexcept { return m_bytes_per_pixel; }
    [[nodiscard]] const uint8_t* get_pixels() const noexcept { return m_pixels.data(); }
    [[nodiscard]] double get_utilization() const noexcept { return m_packer.get_utilization(); }

private:
    void write_pixels_into_region(
        const RectU32& total_rect,
        uint32_t content_x,
        uint32_t content_y,
        uint32_t content_w,
        uint32_t content_h,
        const void* pixel_data,
        size_t row_stride_bytes,
        bool replicate_edge_border) noexcept
    {
        const auto* src_bytes = static_cast<const uint8_t*>(pixel_data);
        for (uint32_t r = 0; r < content_h; ++r) {
            size_t dst_offset = (static_cast<size_t>(content_y + r) * m_width + content_x) * m_bytes_per_pixel;
            const uint8_t* src_row = src_bytes + (r * row_stride_bytes);
            std::memcpy(m_pixels.data() + dst_offset, src_row, content_w * m_bytes_per_pixel);
        }

        if (replicate_edge_border) {
            auto copy_px = [&](uint32_t dx, uint32_t dy, uint32_t sx, uint32_t sy) {
                size_t dst_off = (static_cast<size_t>(dy) * m_width + dx) * m_bytes_per_pixel;
                size_t src_off = (static_cast<size_t>(sy) * m_width + sx) * m_bytes_per_pixel;
                std::memcpy(m_pixels.data() + dst_off, m_pixels.data() + src_off, m_bytes_per_pixel);
            };
            std::memcpy(m_pixels.data() + (static_cast<size_t>(total_rect.y) * m_width + content_x) * m_bytes_per_pixel,
                        m_pixels.data() + (static_cast<size_t>(content_y) * m_width + content_x) * m_bytes_per_pixel,
                        content_w * m_bytes_per_pixel);
            std::memcpy(m_pixels.data() + (static_cast<size_t>(total_rect.bottom() - 1) * m_width + content_x) * m_bytes_per_pixel,
                        m_pixels.data() + (static_cast<size_t>(content_y + content_h - 1) * m_width + content_x) * m_bytes_per_pixel,
                        content_w * m_bytes_per_pixel);
            for (uint32_t r = 0; r < content_h; ++r) {
                copy_px(total_rect.x, content_y + r, content_x, content_y + r);
                copy_px(total_rect.right() - 1, content_y + r, content_x + content_w - 1, content_y + r);
            }
            copy_px(total_rect.x, total_rect.y, content_x, content_y);
            copy_px(total_rect.right() - 1, total_rect.y, content_x + content_w - 1, content_y);
            copy_px(total_rect.x, total_rect.bottom() - 1, content_x, content_y + content_h - 1);
            copy_px(total_rect.right() - 1, total_rect.bottom() - 1, content_x + content_w - 1, content_y + content_h - 1);
        } else {
            for (uint32_t x = total_rect.x; x < total_rect.right(); ++x) {
                set_pixel_zero(x, total_rect.y);
                set_pixel_zero(x, total_rect.bottom() - 1);
            }
            for (uint32_t y = total_rect.y; y < total_rect.bottom(); ++y) {
                set_pixel_zero(total_rect.x, y);
                set_pixel_zero(total_rect.right() - 1, y);
            }
        }
    }

    void set_pixel_zero(uint32_t x, uint32_t y) noexcept {
        size_t offset = (static_cast<size_t>(y) * m_width + x) * m_bytes_per_pixel;
        std::memset(m_pixels.data() + offset, 0, m_bytes_per_pixel);
    }

    uint32_t m_width;
    uint32_t m_height;
    AtlasFormat m_format;
    size_t m_bytes_per_pixel;
    ShelfAtlasPacker m_packer;
    std::vector<uint8_t> m_pixels;
    std::vector<RectU32> m_dirty_rects;
    uint64_t m_generation{1};
    bool m_overflowed{false};
};

} // namespace kernetile::atlas

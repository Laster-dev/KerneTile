#pragma once

#include <ktui/binding/binding.hpp>
#include <ktui/widgets/widget.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <ktui/pal/platform_types.hpp>

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

} // namespace ktui

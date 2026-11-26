#include <ktui/widgets/image.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// 精确裁剪 stb_image：仅保留 PNG / JPEG / BMP / GIF（含多帧动图），关闭 HDR/PSD/TGA/PNM/PIC 与浮点线性转换
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_LINEAR
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100 4244 4456 4457 4505 4996)
#elif defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include <stb_image.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace ktui {

namespace {

bool read_file_bytes(const std::string& path, std::vector<uint8_t>& out) {
    if (path.empty()) return false;
    std::FILE* fp = nullptr;
#if defined(_WIN32)
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen > 0) {
        std::wstring wpath(static_cast<size_t>(wlen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);
        fp = _wfopen(wpath.c_str(), L"rb");
    }
    if (!fp) {
        fp = std::fopen(path.c_str(), "rb");
    }
#else
    fp = std::fopen(path.c_str(), "rb");
#endif
    if (!fp) return false;

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    long sz = std::ftell(fp);
    if (sz <= 0 || sz > 128L * 1024L * 1024L) { // 安全上限 128 MB
        std::fclose(fp);
        return false;
    }
    if (std::fseek(fp, 0, SEEK_SET) != 0) {
        std::fclose(fp);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    size_t n = std::fread(out.data(), 1, out.size(), fp);
    std::fclose(fp);
    return n == out.size();
}

// 轻量双线性降采样（用于将 4K 壁纸等超大图缩放到适配动态图集的尺寸）
std::vector<uint8_t> downsample_rgba8(const uint8_t* src, uint32_t sw, uint32_t sh,
                                      uint32_t dw, uint32_t dh) {
    std::vector<uint8_t> dst(static_cast<size_t>(dw) * dh * 4u);
    const float sx_scale = static_cast<float>(sw) / static_cast<float>(dw);
    const float sy_scale = static_cast<float>(sh) / static_cast<float>(dh);

    for (uint32_t y = 0; y < dh; ++y) {
        float fy = (static_cast<float>(y) + 0.5f) * sy_scale - 0.5f;
        int y0 = static_cast<int>(std::floor(fy));
        float wy = fy - static_cast<float>(y0);
        int y1 = std::min(y0 + 1, static_cast<int>(sh) - 1);
        y0 = std::clamp(y0, 0, static_cast<int>(sh) - 1);
        y1 = std::clamp(y1, 0, static_cast<int>(sh) - 1);

        const uint8_t* row0 = src + static_cast<size_t>(y0) * sw * 4u;
        const uint8_t* row1 = src + static_cast<size_t>(y1) * sw * 4u;
        uint8_t* out_row = dst.data() + static_cast<size_t>(y) * dw * 4u;

        for (uint32_t x = 0; x < dw; ++x) {
            float fx = (static_cast<float>(x) + 0.5f) * sx_scale - 0.5f;
            int x0 = static_cast<int>(std::floor(fx));
            float wx = fx - static_cast<float>(x0);
            int x1 = std::min(x0 + 1, static_cast<int>(sw) - 1);
            x0 = std::clamp(x0, 0, static_cast<int>(sw) - 1);
            x1 = std::clamp(x1, 0, static_cast<int>(sw) - 1);

            const uint8_t* p00 = row0 + static_cast<size_t>(x0) * 4u;
            const uint8_t* p10 = row0 + static_cast<size_t>(x1) * 4u;
            const uint8_t* p01 = row1 + static_cast<size_t>(x0) * 4u;
            const uint8_t* p11 = row1 + static_cast<size_t>(x1) * 4u;

            const float w00 = (1.0f - wx) * (1.0f - wy);
            const float w10 = wx * (1.0f - wy);
            const float w01 = (1.0f - wx) * wy;
            const float w11 = wx * wy;

            for (int c = 0; c < 4; ++c) {
                float val = p00[c] * w00 + p10[c] * w10 + p01[c] * w01 + p11[c] * w11;
                out_row[x * 4u + c] = static_cast<uint8_t>(std::clamp(val + 0.5f, 0.0f, 255.0f));
            }
        }
    }
    return dst;
}

void compute_clamped_dims(uint32_t w, uint32_t h, uint32_t max_dim,
                          uint32_t& out_w, uint32_t& out_h) noexcept {
    out_w = w;
    out_h = h;
    if (max_dim > 0u && (w > max_dim || h > max_dim)) {
        if (w >= h) {
            out_w = max_dim;
            out_h = std::max(1u, static_cast<uint32_t>((static_cast<uint64_t>(h) * max_dim + w / 2u) / w));
        } else {
            out_h = max_dim;
            out_w = std::max(1u, static_cast<uint32_t>((static_cast<uint64_t>(w) * max_dim + h / 2u) / h));
        }
    }
}

bool is_gif_header(const uint8_t* data, size_t size) noexcept {
    return size >= 6u &&
           data[0] == 'G' && data[1] == 'I' && data[2] == 'F' && data[3] == '8' &&
           (data[4] == '7' || data[4] == '9') && data[5] == 'a';
}

} // namespace

std::shared_ptr<ImageBuffer> ImageBuffer::load_memory(const uint8_t* data, size_t size, uint32_t max_dim) {
    if (!data || size == 0u || size > 128u * 1024u * 1024u) return nullptr;

    // 1. GIF 动图/静态图路径（支持多帧 + 帧延迟 + 48 MB 内存硬预算保护）
    if (is_gif_header(data, size)) {
        int* delays = nullptr;
        int w = 0, h = 0, z = 0, comp = 0;
        stbi_uc* raw = stbi_load_gif_from_memory(
            data, static_cast<int>(size), &delays, &w, &h, &z, &comp, 4);
        if (raw && w > 0 && h > 0 && z > 0) {
            uint32_t uw = static_cast<uint32_t>(w);
            uint32_t uh = static_cast<uint32_t>(h);
            uint32_t gif_max_dim = (max_dim > 0u) ? std::min(max_dim, 768u) : 768u;
            uint32_t tw = uw, th = uh;
            compute_clamped_dims(uw, uh, gif_max_dim, tw, th);
            const size_t src_frame_bytes = static_cast<size_t>(uw) * uh * 4u;

            if (z == 1) {
                std::vector<uint8_t> pixels;
                if (tw != uw || th != uh) {
                    pixels = downsample_rgba8(raw, uw, uh, tw, th);
                } else {
                    pixels.assign(raw, raw + src_frame_bytes);
                }
                stbi_image_free(raw);
                if (delays) STBI_FREE(delays);
                return from_rgba8(tw, th, std::move(pixels));
            }

            // 动图硬预算保护：最多保留 180 帧（均匀抽帧并累加延迟保持原速），且总像素内存 <= 48 MB
            constexpr uint32_t kMaxGifFrames = 180u;
            constexpr uint64_t kMaxAnimatedBudgetBytes = 48ull * 1024ull * 1024ull;
            const uint32_t stride = (static_cast<uint32_t>(z) > kMaxGifFrames)
                ? ((static_cast<uint32_t>(z) + kMaxGifFrames - 1u) / kMaxGifFrames)
                : 1u;
            const uint32_t kept_frames = (static_cast<uint32_t>(z) + stride - 1u) / stride;

            const uint64_t est_bytes = uint64_t(tw) * th * 4ull * kept_frames;
            if (est_bytes > kMaxAnimatedBudgetBytes) {
                double scale = std::sqrt(static_cast<double>(kMaxAnimatedBudgetBytes) / static_cast<double>(est_bytes));
                tw = std::max(16u, static_cast<uint32_t>(std::floor(tw * scale)));
                th = std::max(16u, static_cast<uint32_t>(std::floor(th * scale)));
            }

            std::vector<ImageFrame> frames;
            frames.reserve(kept_frames);
            for (int i = 0; i < z; i += static_cast<int>(stride)) {
                const uint8_t* fptr = raw + static_cast<size_t>(i) * src_frame_bytes;
                ImageFrame frame;
                if (tw != uw || th != uh) {
                    frame.pixels = downsample_rgba8(fptr, uw, uh, tw, th);
                } else {
                    frame.pixels.assign(fptr, fptr + src_frame_bytes);
                }
                uint32_t acc_delay = 0;
                for (int k = 0; k < static_cast<int>(stride) && (i + k) < z; ++k) {
                    int d = delays ? delays[i + k] : 100;
                    acc_delay += static_cast<uint32_t>(d < 16 ? 100 : d);
                }
                frame.delay_ms = std::max<uint32_t>(16u, acc_delay);
                frames.push_back(std::move(frame));
            }
            stbi_image_free(raw);
            if (delays) STBI_FREE(delays);
            return from_frames(tw, th, std::move(frames));
        }
        if (raw) stbi_image_free(raw);
        if (delays) STBI_FREE(delays);
        return nullptr;
    }

    // 2. PNG / JPEG / BMP 静态图像路径
    int w = 0, h = 0, comp = 0;
    stbi_uc* raw = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &comp, 4);
    if (!raw || w <= 0 || h <= 0) {
        if (raw) stbi_image_free(raw);
        return nullptr;
    }

    uint32_t uw = static_cast<uint32_t>(w);
    uint32_t uh = static_cast<uint32_t>(h);
    uint32_t tw = uw, th = uh;
    compute_clamped_dims(uw, uh, max_dim, tw, th);

    std::vector<uint8_t> pixels;
    if (tw != uw || th != uh) {
        pixels = downsample_rgba8(raw, uw, uh, tw, th);
    } else {
        pixels.assign(raw, raw + static_cast<size_t>(uw) * uh * 4u);
    }
    stbi_image_free(raw);
    return from_rgba8(tw, th, std::move(pixels));
}

std::shared_ptr<ImageBuffer> ImageBuffer::load_file(const std::string& file_path, uint32_t max_dim) {
    std::vector<uint8_t> bytes;
    if (!read_file_bytes(file_path, bytes)) return nullptr;
    return load_memory(bytes.data(), bytes.size(), max_dim);
}

} // namespace ktui

#include <ktui/presentation/svg_engine.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244 4267 4305 4456 4457 4458 4996)
#endif
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace ktui {

namespace {

uint64_t fnv1a_hash(std::string_view data, uint64_t seed = 0xcbf29ce484222325ULL) noexcept {
    uint64_t h = seed;
    for (unsigned char c : data) {
        h ^= static_cast<uint64_t>(c);
        h *= 0x100000001b3ULL;
    }
    return h;
}

uint64_t combine_hash(uint64_t h, uint64_t v) noexcept {
    h ^= v;
    h *= 0x100000001b3ULL;
    return h;
}

// 线程安全文件内容缓存 (避免每帧重复读盘)
std::string load_svg_file_cached(const std::string& path) {
    static std::mutex mtx;
    static std::unordered_map<std::string, std::string> file_cache;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = file_cache.find(path);
    if (it != file_cache.end()) {
        return it->second;
    }

    std::ifstream ifs(path, std::ios::binary);
    std::string content;
    if (ifs) {
        std::ostringstream oss;
        oss << ifs.rdbuf();
        content = oss.str();
    }
    file_cache.emplace(path, content);
    return content;
}

// 在内存中对 R8 蒙版执行双线性旋转采样 (用于设置齿轮等需要连续旋转动效的 SVG 图标，零重复解析 XML)
void rotate_r8_bilinear(const uint8_t* src, uint32_t w, uint32_t h, float angle_rad, uint8_t* dst) noexcept {
    const float cx = (static_cast<float>(w) - 1.0f) * 0.5f;
    const float cy = (static_cast<float>(h) - 1.0f) * 0.5f;
    const float ca = std::cos(-angle_rad);
    const float sa = std::sin(-angle_rad);

    for (uint32_t y = 0; y < h; ++y) {
        float dy = static_cast<float>(y) - cy;
        for (uint32_t x = 0; x < w; ++x) {
            float dx = static_cast<float>(x) - cx;
            float sx = cx + dx * ca - dy * sa;
            float sy = cy + dx * sa + dy * ca;

            int x0 = static_cast<int>(std::floor(sx));
            int y0 = static_cast<int>(std::floor(sy));
            float fx = sx - static_cast<float>(x0);
            float fy = sy - static_cast<float>(y0);

            auto sample = [&](int ix, int iy) noexcept -> float {
                if (ix < 0 || iy < 0 || ix >= static_cast<int>(w) || iy >= static_cast<int>(h)) return 0.0f;
                return static_cast<float>(src[static_cast<size_t>(iy) * w + static_cast<size_t>(ix)]);
            };

            float c00 = sample(x0, y0);
            float c10 = sample(x0 + 1, y0);
            float c01 = sample(x0, y0 + 1);
            float c11 = sample(x0 + 1, y0 + 1);
            float c0 = c00 + fx * (c10 - c00);
            float c1 = c01 + fx * (c11 - c01);
            float val = c0 + fy * (c1 - c0);
            dst[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>(std::clamp(std::lround(val), 0L, 255L));
        }
    }
}

// 缓存未旋转的基础 NanoSVG 光栅化 R8 蒙版，供静态渲染与旋转采样复用
const std::vector<uint8_t>& rasterize_base_r8(uint64_t base_key, std::string_view svg_xml,
                                              uint32_t px_w, uint32_t px_h) {
    static std::mutex mtx;
    static std::unordered_map<uint64_t, std::vector<uint8_t>> base_cache;

    std::lock_guard<std::mutex> lock(mtx);
    auto it = base_cache.find(base_key);
    if (it != base_cache.end()) {
        return it->second;
    }

    std::vector<uint8_t> alpha_mask(static_cast<size_t>(px_w) * px_h, 0);

    // nsvgParse 会原地修改输入字符串，必须拷贝一份以 '\0' 结尾的缓冲区
    std::string mutable_xml(svg_xml);
    NSVGimage* image = nsvgParse(mutable_xml.data(), "px", 96.0f);
    if (image && image->width > 0.0f && image->height > 0.0f) {
        NSVGrasterizer* rast = nsvgCreateRasterizer();
        if (rast) {
            std::vector<uint8_t> rgba(static_cast<size_t>(px_w) * px_h * 4, 0);
            float scale_x = static_cast<float>(px_w) / image->width;
            float scale_y = static_cast<float>(px_h) / image->height;
            float scale = std::min(scale_x, scale_y);
            float tx = (static_cast<float>(px_w) - image->width * scale) * 0.5f;
            float ty = (static_cast<float>(px_h) - image->height * scale) * 0.5f;

            nsvgRasterize(rast, image, tx, ty, scale, rgba.data(),
                          static_cast<int>(px_w), static_cast<int>(px_h),
                          static_cast<int>(px_w) * 4);

            for (size_t i = 0; i < alpha_mask.size(); ++i) {
                alpha_mask[i] = rgba[i * 4 + 3];
            }
            nsvgDeleteRasterizer(rast);
        }
        nsvgDelete(image);
    }

    auto res = base_cache.emplace(base_key, std::move(alpha_mask));
    return res.first->second;
}

} // namespace

void SvgEngine::draw_svg_xml(PaintContext& ctx,
                             std::string_view svg_xml,
                             Rect dst_rect,
                             Color tint,
                             float rotation_rad) {
    if (svg_xml.empty() || tint.a <= 0.001f || dst_rect.width() <= 0.0f || dst_rect.height() <= 0.0f) {
        return;
    }

    const float scale = std::max(1.0f, ctx.render_scale());
    const uint32_t px_w = std::clamp(static_cast<uint32_t>(std::ceil(dst_rect.width() * scale)), 1U, 256U);
    const uint32_t px_h = std::clamp(static_cast<uint32_t>(std::ceil(dst_rect.height() * scale)), 1U, 256U);

    // 角度量化至 3° 步长 (120 档，覆盖 360° 丝滑旋转且限制图集槽位占用)
    constexpr float kTwoPi = 6.28318530718f;
    float norm_ang = std::fmod(rotation_rad, kTwoPi);
    if (norm_ang < 0.0f) norm_ang += kTwoPi;
    int angle_bucket = static_cast<int>(std::lround(norm_ang / kTwoPi * 120.0f)) % 120;

    uint64_t xml_hash = fnv1a_hash(svg_xml);
    uint64_t base_key = combine_hash(combine_hash(xml_hash, px_w), px_h);
    uint64_t atlas_key = combine_hash(base_key, static_cast<uint64_t>(angle_bucket) + 0x517cc1b727220a95ULL);

    if (ctx.has_cached_mask(atlas_key)) {
        ctx.draw_alpha_mask(atlas_key, dst_rect, px_w, px_h, nullptr, tint);
        return;
    }

    const auto& base_mask = rasterize_base_r8(base_key, svg_xml, px_w, px_h);
    if (angle_bucket == 0) {
        ctx.draw_alpha_mask(atlas_key, dst_rect, px_w, px_h, base_mask.data(), tint);
    } else {
        std::vector<uint8_t> rotated(static_cast<size_t>(px_w) * px_h, 0);
        float quant_rad = (static_cast<float>(angle_bucket) / 120.0f) * kTwoPi;
        rotate_r8_bilinear(base_mask.data(), px_w, px_h, quant_rad, rotated.data());
        ctx.draw_alpha_mask(atlas_key, dst_rect, px_w, px_h, rotated.data(), tint);
    }
}

void SvgEngine::draw_svg_file(PaintContext& ctx,
                              const std::string& file_path,
                              Rect dst_rect,
                              Color tint,
                              float rotation_rad) {
    if (file_path.empty()) return;
    std::string xml = load_svg_file_cached(file_path);
    if (!xml.empty()) {
        draw_svg_xml(ctx, xml, dst_rect, tint, rotation_rad);
    }
}

void SvgEngine::draw_svg_path(PaintContext& ctx,
                              std::string_view path_d,
                              float viewbox_w,
                              float viewbox_h,
                              Rect dst_rect,
                              Color tint,
                              float stroke_width,
                              bool fill,
                              float rotation_rad) {
    if (path_d.empty()) return;
    char header[160];
    std::snprintf(header, sizeof(header),
                  "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %.2f %.2f\" width=\"%.2f\" height=\"%.2f\"><path d=\"",
                  viewbox_w, viewbox_h, viewbox_w, viewbox_h);

    std::string xml;
    xml.reserve(path_d.size() + 256);
    xml.append(header);
    xml.append(path_d);
    if (stroke_width > 0.0f) {
        char tail[128];
        std::snprintf(tail, sizeof(tail),
                      "\" fill=\"%s\" stroke=\"#ffffff\" stroke-width=\"%.2f\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/></svg>",
                      fill ? "#ffffff" : "none", stroke_width);
        xml.append(tail);
    } else {
        xml.append("\" fill=\"#ffffff\"/></svg>");
    }

    draw_svg_xml(ctx, xml, dst_rect, tint, rotation_rad);
}

} // namespace ktui

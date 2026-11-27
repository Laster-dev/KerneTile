#pragma once

#include "../widget.hpp"
#include <ktui/binding/binding.hpp>
#include <functional>
#include <memory>
#include <string>
#include <ktui/pal/platform_types.hpp>

namespace ktui {


// 颜色转换与棋盘格绘制公共辅助
struct ColorPickerUtil {
    static void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) noexcept {
        h = std::fmod(std::fmod(h, 360.0f) + 360.0f, 360.0f);
        s = std::clamp(s, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;
        float rp = 0.0f, gp = 0.0f, bp = 0.0f;
        if (h < 60.0f)       { rp = c; gp = x; bp = 0.0f; }
        else if (h < 120.0f) { rp = x; gp = c; bp = 0.0f; }
        else if (h < 180.0f) { rp = 0.0f; gp = c; bp = x; }
        else if (h < 240.0f) { rp = 0.0f; gp = x; bp = c; }
        else if (h < 300.0f) { rp = x; gp = 0.0f; bp = c; }
        else                 { rp = c; gp = 0.0f; bp = x; }
        r = rp + m;
        g = gp + m;
        b = bp + m;
    }

    static void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) noexcept {
        r = std::clamp(r, 0.0f, 1.0f);
        g = std::clamp(g, 0.0f, 1.0f);
        b = std::clamp(b, 0.0f, 1.0f);
        float cmax = std::max({r, g, b});
        float cmin = std::min({r, g, b});
        float delta = cmax - cmin;
        v = cmax;
        s = (cmax > 1e-6f) ? (delta / cmax) : 0.0f;
        if (delta <= 1e-6f) {
            h = 0.0f;
        } else if (cmax == r) {
            h = 60.0f * std::fmod((g - b) / delta, 6.0f);
            if (h < 0.0f) h += 360.0f;
        } else if (cmax == g) {
            h = 60.0f * (((b - r) / delta) + 2.0f);
        } else {
            h = 60.0f * (((r - g) / delta) + 4.0f);
        }
    }

    static std::string format_hex(Color c, bool include_alpha) {
        auto to_u8 = [](float x) -> unsigned {
            return static_cast<unsigned>(std::clamp(std::lround(x * 255.0f), 0L, 255L));
        };
        char buf[16];
        if (include_alpha) {
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", to_u8(c.a), to_u8(c.r), to_u8(c.g), to_u8(c.b));
        } else {
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", to_u8(c.r), to_u8(c.g), to_u8(c.b));
        }
        return std::string(buf);
    }

    static void paint_checkerboard(PaintContext& ctx, Rect r, float cell) {
        Color c1{0.82f, 0.82f, 0.82f, 1.0f};
        Color c2{0.62f, 0.62f, 0.62f, 1.0f};
        ctx.draw_rect(r, c1, 0.0f, 0.0f);
        int cols = static_cast<int>(std::ceil(r.width() / cell));
        int rows = static_cast<int>(std::ceil(r.height() / cell));
        for (int row = 0; row < rows; ++row) {
            for (int col = (row & 1); col < cols; col += 2) {
                ctx.draw_rect(Rect::from_xywh(r.min_x + static_cast<float>(col) * cell,
                                              r.min_y + static_cast<float>(row) * cell,
                                              cell, cell),
                              c2, 0.0f, 0.0f);
            }
        }
    }
};

} // namespace ktui

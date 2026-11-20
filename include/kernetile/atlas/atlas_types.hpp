#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>

namespace kernetile::atlas {

enum class AtlasFormat : uint8_t {
    R8_UNORM,       // 灰度字形图集 (1 字节/像素)
    RGBA8_UNORM     // 彩色图标图集 (4 字节/像素)
};

struct RectU32 {
    uint32_t x{0};
    uint32_t y{0};
    uint32_t w{0};
    uint32_t h{0};

    [[nodiscard]] constexpr uint32_t right() const noexcept { return x + w; }
    [[nodiscard]] constexpr uint32_t bottom() const noexcept { return y + h; }
    [[nodiscard]] constexpr uint32_t area() const noexcept { return w * h; }
};

// 动态图集分配区域元数据 (包含外扩边界与定点化 UV 映射)
struct AtlasRegion {
    RectU32 total_rect;     // 包含 1px 安全外边距的图集物理像素矩形 (x, y, w+2, h+2)
    RectU32 content_rect;   // 内缩 1px 后的真实数据内容像素矩形 (x+1, y+1, w, h)

    // 定点化 16-bit 归一化 UV 坐标 (0~65535 线性映射至 0.0~1.0)
    // 严格与 80 字节 UiPrimitive::atlas_u0~v1 布局完全吻合
    uint16_t u0{0};
    uint16_t v0{0};
    uint16_t u1{0};
    uint16_t v1{0};

    static constexpr uint16_t to_fixed_uv(float normalized) noexcept {
        float clamped = std::clamp(normalized, 0.0f, 1.0f);
        return static_cast<uint16_t>(clamped * 65535.0f + 0.5f);
    }

    static constexpr float from_fixed_uv(uint16_t fixed_val) noexcept {
        return static_cast<float>(fixed_val) / 65535.0f;
    }
};

} // namespace kernetile::atlas

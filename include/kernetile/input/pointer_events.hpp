#pragma once

#include "kernetile/math/vec.hpp"
#include <cstdint>
#include <cmath>

namespace kernetile::input {

enum class PointerType : uint8_t {
    Mouse = 0,
    Touch = 1,
    Pen   = 2
};

enum class PointerEventType : uint8_t {
    Down      = 0,
    Move      = 1,
    Up        = 2,
    Cancel    = 3,
    RawUpdate = 4,
    Enter     = 5,
    Leave     = 6
};

namespace ButtonMask {
    constexpr uint16_t None      = 0x0000;
    constexpr uint16_t Primary   = 0x0001; // 鼠标左键 / 触控接触 / 笔尖接触
    constexpr uint16_t Secondary = 0x0002; // 鼠标右键 / 笔侧键
    constexpr uint16_t Auxiliary = 0x0004; // 鼠标滚轮中键
    constexpr uint16_t Back      = 0x0008; // 侧边后退键
    constexpr uint16_t Forward   = 0x0010; // 侧边前进键
    constexpr uint16_t Eraser    = 0x0020; // 数位板笔尾擦除器
}

namespace ModifierMask {
    constexpr uint16_t None    = 0x0000;
    constexpr uint16_t Alt     = 0x0001;
    constexpr uint16_t Control = 0x0002;
    constexpr uint16_t Shift   = 0x0004;
    constexpr uint16_t Meta    = 0x0008; // Windows 键 / Mac Command 键
}

// ============================================================================
// W3C Pointer Events Level 3 统一输入数据包
// 标准化鼠标、多点触控与专业电磁手写笔采样
// ============================================================================
struct PointerEvent {
    PointerEventType type{PointerEventType::Move};
    uint32_t         pointer_id{0};
    PointerType      pointer_type{PointerType::Mouse};

    // 物理视口绝对像素坐标 (亚像素浮点精度)
    double client_x{0.0};
    double client_y{0.0};

    // 接触椭圆包围尺寸 (单位: 物理像素)
    float width{1.0f};
    float height{1.0f};

    // 物理压感与副压感 (归一化区间 [0.0, 1.0])
    float pressure{0.0f};
    float tangential_pressure{0.0f};

    // 笔身几何倾角 (角度制: tilt_x, tilt_y in [-90, 90])
    float tilt_x{0.0f};
    float tilt_y{0.0f};

    // W3C Pointer Events L3 球面坐标表示
    // altitude_angle: 笔身与表面夹角 [0, pi/2], 默认垂直 pi/2 (1.5707963 rad)
    // azimuth_angle:  笔身投影与正X轴逆时针夹角 [0, 2*pi)
    float altitude_angle{1.5707963f};
    float azimuth_angle{0.0f};

    // 键位与状态掩码
    uint16_t buttons{0};
    uint16_t modifiers{0};

    uint64_t timestamp_us{0}; // 微秒级硬件时间戳
    bool     is_primary{true};

    // 工具函数：根据 tilt_x/y (度) 换算 W3C L3 球面角 (弧度)
    static void tilt_to_spherical(
        float tilt_x_deg,
        float tilt_y_deg,
        float& out_altitude_rad,
        float& out_azimuth_rad)
    {
        constexpr float deg2rad = 3.14159265358979323846f / 180.0f;
        float rad_x = tilt_x_deg * deg2rad;
        float rad_y = tilt_y_deg * deg2rad;

        // 计算三维单位法线向量
        float tan_x = std::tan(rad_x);
        float tan_y = std::tan(rad_y);

        // 视口表面坐标系: 笔身向量指向表面
        float z = 1.0f / std::sqrt(tan_x * tan_x + tan_y * tan_y + 1.0f);
        float x = tan_x * z;
        float y = tan_y * z;

        // altitude: 与 XY 平面的夹角 asin(z)
        out_altitude_rad = std::asin(std::clamp(z, 0.0f, 1.0f));

        // azimuth: atan2(y, x), 归一化到 [0, 2*pi)
        float az = std::atan2(y, x);
        if (az < 0.0f) {
            az += 2.0f * 3.14159265358979323846f;
        }
        out_azimuth_rad = az;
    }

    // 工具函数：根据 W3C L3 球面角换算 tilt_x/y
    static void spherical_to_tilt(
        float altitude_rad,
        float azimuth_rad,
        float& out_tilt_x_deg,
        float& out_tilt_y_deg)
    {
        constexpr float rad2deg = 180.0f / 3.14159265358979323846f;
        float cos_alt = std::cos(altitude_rad);
        float sin_alt = std::sin(altitude_rad);

        if (sin_alt <= 0.0001f) {
            out_tilt_x_deg = 90.0f * std::cos(azimuth_rad);
            out_tilt_y_deg = 90.0f * std::sin(azimuth_rad);
            return;
        }

        float tan_x = cos_alt * std::cos(azimuth_rad) / sin_alt;
        float tan_y = cos_alt * std::sin(azimuth_rad) / sin_alt;

        out_tilt_x_deg = std::atan(tan_x) * rad2deg;
        out_tilt_y_deg = std::atan(tan_y) * rad2deg;
    }
};

} // namespace kernetile::input

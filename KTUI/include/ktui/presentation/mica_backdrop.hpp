#pragma once

#include <ktui/pal/platform_types.hpp>
#include <array>
#include <cstdint>

namespace ktui {

// 窗口自算材质类型 (严格对齐 Windows 11 WinUI 3 MicaController 官方规范)
enum class WindowBackdrop : uint8_t {
    None = 0,     // 纯色背景
    Mica = 1,     // 标准云母 (Base)：D2D1 Luminosity Blend (1.0) + TintOpacity (暗 #202020 80% / 亮 #F3F3F3 50%)，失焦平滑退化为 Fallback 纯色
    MicaAlt = 2   // 深层云母 (Tabbed)：D2D1 Luminosity Blend (暗 0.96 / 亮 1.0) + TintOpacity (暗 #0A0A0A 0% / 亮 #DADADA 50%)
};

// 自算云母壁纸缓存与 4x4 屏幕空间色场采样器
// 参照 D:\CUI\CUI.Core\ui\framework\render\GraphicsContext.cpp Homemade Mica 原理：
//   1. Windows 读取系统壁纸 (SPI_GETDESKWALLPAPER / TranscodedWallpaper) 并降采样至 <=256px
//   2. 使用 KerneTile 内置 DualKawaseBlur 一次性离线强模糊 (0 运行时磁盘/IPC 开销)
//   3. 按窗口相对当前显示器的屏幕坐标采样 4x4 双线性色场并按主题叠加
class MicaBackdropEngine {
public:
    static void invalidate_wallpaper() noexcept;

    // 采样当前窗口屏幕位置对应的 4x4 打包 RGBA8 云母色场
    // monitor_uv_rect: 窗口在当前显示器上的归一化区域 [0..1]
    // active_factor: 窗口聚焦激活权重 [0.0 = 完全失焦纯色, 1.0 = 完全聚焦云母]，支持平滑渐入渐出
    static bool build_mica_grid(WindowBackdrop backdrop,
                                bool is_dark,
                                float active_factor,
                                Rect monitor_uv_rect,
                                std::array<uint32_t, 16>& out_grid);

    static bool build_mica_grid(WindowBackdrop backdrop,
                                bool is_dark,
                                bool is_active,
                                Rect monitor_uv_rect,
                                std::array<uint32_t, 16>& out_grid) {
        return build_mica_grid(backdrop, is_dark, is_active ? 1.0f : 0.0f, monitor_uv_rect, out_grid);
    }
};

} // namespace ktui

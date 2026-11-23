#pragma once

#include <ktui/pal/platform_types.hpp>
#include <array>
#include <cstdint>

namespace ktui {

// 窗口自算材质类型 (对齐 CUI.Core WindowBackdrop)
enum class WindowBackdrop : uint8_t {
    None = 0,     // 纯色背景
    Mica = 1,     // 标准云母：主题底色 + 20%(暗)/24%(亮) 屏幕空间模糊壁纸采样，失焦退化为纯色
    MicaAlt = 2   // 深层云母：更深底色 + 34%(暗)/38%(亮) 更高饱和度壁纸采样
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
    static bool build_mica_grid(WindowBackdrop backdrop,
                                bool is_dark,
                                bool is_active,
                                Rect monitor_uv_rect,
                                std::array<uint32_t, 16>& out_grid);
};

} // namespace ktui

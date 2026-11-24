#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <string>
#include <string_view>

namespace ktui {

// NanoSVG 矢量渲染引擎 (基于 memononen/nanosvg)
// 架构: NanoSVG 解析 + 抗锯齿光栅化 -> 提取 R8 Alpha 通道上传至 KerneTile DynamicAtlas ->
//       单图元 80B GPU/CPU 硬件加速着色渲染，带 (内容哈希, 像素宽高, 角度) 多级缓存，支持 HiDPI 与动态换色
class SvgEngine {
public:
    // 直接渲染完整 SVG XML 字符串 (支持旋转角 rotation_rad，适用于设置齿轮等旋转图标)
    static void draw_svg_xml(PaintContext& ctx,
                             std::string_view svg_xml,
                             Rect dst_rect,
                             Color tint,
                             float rotation_rad = 0.0f);

    // 从本地 .svg 文件加载并缓存渲染
    static void draw_svg_file(PaintContext& ctx,
                              const std::string& file_path,
                              Rect dst_rect,
                              Color tint,
                              float rotation_rad = 0.0f);

    // 便捷渲染内联 SVG Path ("M... Z")，自动包装为标准 viewBox SVG
    static void draw_svg_path(PaintContext& ctx,
                              std::string_view path_d,
                              float viewbox_w,
                              float viewbox_h,
                              Rect dst_rect,
                              Color tint,
                              float stroke_width = 0.0f,
                              bool fill = true,
                              float rotation_rad = 0.0f);
};

} // namespace ktui

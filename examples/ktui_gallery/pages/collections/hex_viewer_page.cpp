#include "../gallery_pages.hpp"
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

// ============================================================================
// 2. 十六进制 / 字节流查看与编辑格 (HexViewer)
// ============================================================================
WidgetPtr build_hex_viewer_page(GalleryApp& app) {
    auto status = std::make_shared<Property<std::string>>("支持按住鼠标左键拖动连续选择字节（Hex ↔ ASCII 双向联动高亮），宽高与列数随窗口自适应铺满，选中后可按 0-9 / A-F 编辑。");
    std::vector<uint8_t> sample = {
        0x7F, 0x45, 0x4C, 0x46, 0x02, 0x01, 0x01, 0x00, 0x4B, 0x65, 0x72, 0x6E, 0x65, 0x54, 0x69, 0x6C,
        0x65, 0x20, 0x4B, 0x54, 0x55, 0x49, 0x20, 0x32, 0x30, 0x32, 0x36, 0x00, 0x00, 0x80, 0x3F, 0x40,
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE, 0x10, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x2C, 0x20, 0x47, 0x50, 0x55, 0x20, 0x55, 0x49, 0x21, 0x0A, 0x00
    };
    const char* payload = "KerneTile-D3D12-Compute-TileBinning-16x16-Fluent2-Mica-Atlas1024-ThinLTO-ZeroExceptions-";
    sample.reserve(4096);
    for (size_t i = 0; sample.size() < 4096; ++i) {
        if ((i % 32) < 24) sample.push_back(static_cast<uint8_t>(payload[i % 88]));
        else sample.push_back(static_cast<uint8_t>((i * 37 + 0xA5) & 0xFF));
    }

    return app.make_full_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("HexViewer (十六进制字节流查看与编辑格)", 24.0f),
            app.make_muted("三栏等宽矩阵（Offset + 自适应 8/16/24/32 字节 Hex + ASCII），支持按住拖动连续选择、Hex ↔ ASCII 双向联动高亮、就地半字节编辑与 LE/BE 解码。", 13.0f),
            Expanded {
                .flex = 1.0f,
                .child = HexViewer {
                    .bytes = sample,
                    .editable = true,
                    .big_endian = false,
                    .on_byte_edited = [status](size_t off, uint8_t val) {
                        char buf[80];
                        std::snprintf(buf, sizeof(buf), "已修改偏移 0x%04X -> 0x%02X (%u)", static_cast<unsigned>(off), static_cast<unsigned>(val), static_cast<unsigned>(val));
                        status->set(buf);
                    }
                }
            },
            app.make_accent(*status, 12.0f, FontWeight::Medium)
        }
    });
}

} // namespace gallery::pages

#include "../gallery_pages.hpp"

using namespace ktui;

namespace gallery::pages {

WidgetPtr build_teaching_tip_page(GalleryApp& app) {
    static constexpr const char* kBulbSvg =
        "M12 2a7 7 0 0 0-4 12.74V17a2 2 0 0 0 2 2h4a2 2 0 0 0 2-2v-2.26A7 7 0 0 0 12 2zm-2 19a1 1 0 0 0 1 1h2a1 1 0 1 0 0-2h-2a1 1 0 0 0-1 1z";
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            app.make_page_title("TeachingTip", 22.0f),
            app.make_muted("WinUI 3 功能引导气泡，复用 FluentPopupSurface 真高斯阴影与 130ms 缓出动效，带指向目标锚点的矢量三角尾标。", 12.5f),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "触发引导气泡", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 10.0f,
                        .content = {
                            Button {
                                .text = "弹出带尾标引导气泡",
                                .variant = ButtonVariant::Accent,
                                .on_click = [&app]() {
                                    Vec2 anchor{420.0f, 185.0f};
                                    if (Window* w = Window::active_window()) {
                                        anchor = Vec2{w->size().x * 0.52f, w->titlebar_height() + 140.0f};
                                    }
                                    TeachingTip::show(TeachingTipDesc{
                                        .title = "试试新的主题涟漪切换",
                                        .subtitle = "点击右上角标题栏主题按钮，即可体验仿 CUI 圆形 SDF 涟漪展开式深浅色平滑过渡效果。",
                                        .svg_path = kBulbSvg,
                                        .action_text = "立即了解",
                                        .close_text = "知道了",
                                        .preferred_placement = TeachingTipPlacement::Bottom,
                                        .on_action = [&app]() { app.tip_status.set("TeachingTip: 用户点击了主操作「立即了解」"); },
                                        .on_close  = [&app]() { app.tip_status.set("TeachingTip: 用户关闭了引导气泡"); }
                                    }, anchor);
                                }
                            },
                            Button {
                                .text = "弹出居中无尾标气泡",
                                .variant = ButtonVariant::Standard,
                                .on_click = [&app]() {
                                    TeachingTip::show(TeachingTipDesc{
                                        .title = "KTUI Gallery 31 个展厅已就绪",
                                        .subtitle = "所有浮层均统一接入 FluentPopupSurface 真高斯阴影与 FrameClock 帧时钟调度。",
                                        .svg_path = kBulbSvg,
                                        .action_text = "查看 VirtualList",
                                        .close_text = "关闭",
                                        .on_action = [&app]() {
                                            app.tip_status.set("TeachingTip: 已跳转至 VirtualList 展厅");
                                            app.nav_selected.set("virtual_list");
                                        },
                                        .on_close = [&app]() { app.tip_status.set("TeachingTip: 已关闭居中引导气泡"); }
                                    });
                                }
                            }
                        }
                    },
                    app.make_accent(app.tip_status, 12.5f)
                }
            })
        }
    });
}

} // namespace gallery::pages

#include "../gallery_pages.hpp"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

// ============================================================================
// 3. 防误触“长按确认”按钮 (HoldConfirmButton)
// ============================================================================
WidgetPtr build_hold_confirm_button_page(GalleryApp& app) {
    auto log_text = std::make_shared<Property<std::string>>("操作日志: 尚未触发任何高危操作（按住按钮满时长触发，中途松开将自动回弹取消）");
    auto count = std::make_shared<int>(0);

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("HoldConfirmButton (防误触长按确认按钮)", 24.0f),
            app.make_muted("无需弹窗打断心流：支持胶囊进度条与圆形 360° 进度环两种形态，按住满时长触发高危操作，中途松手平滑回弹取消。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 14.0f,
                .content = {
                    Text { .text = "胶囊进度条样式 (PillBar)", .font_size = 13.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .spacing = 12.0f,
                        .content = {
                            HoldConfirmButton {
                                .text = "长按 1.5s 删除生产数据库",
                                .holding_text = "正在解锁删除",
                                .confirmed_text = "✓ 已安全执行删除",
                                .hold_duration_sec = 1.5f,
                                .danger = true,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 已确认执行「删除生产数据库」");
                                }
                            },
                            HoldConfirmButton {
                                .text = "长按 1.0s 清空 Redis 缓存",
                                .holding_text = "按住清空缓存",
                                .confirmed_text = "✓ 缓存已清空",
                                .hold_duration_sec = 1.0f,
                                .danger = true,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 已确认执行「清空 Redis 缓存」");
                                }
                            },
                            HoldConfirmButton {
                                .text = "长按 1.2s 发布到线上集群",
                                .holding_text = "正在触发发布",
                                .confirmed_text = "✓ 发布流水线已启动",
                                .hold_duration_sec = 1.2f,
                                .danger = false,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 已确认执行「发布到线上集群」");
                                }
                            }
                        }
                    },
                    Text { .text = "圆形进度环样式 (CircularRing)", .font_size = 13.0f, .font_weight = FontWeight::SemiBold },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 20.0f,
                        .content = {
                            HoldConfirmButton {
                                .text = "删除",
                                .hold_duration_sec = 1.5f,
                                .danger = true,
                                .shape = HoldConfirmShape::CircularRing,
                                .diameter = 64.0f,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 圆形长按确认「删除」已执行");
                                }
                            },
                            HoldConfirmButton {
                                .text = "重启",
                                .hold_duration_sec = 1.2f,
                                .danger = false,
                                .shape = HoldConfirmShape::CircularRing,
                                .diameter = 64.0f,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 圆形长按确认「重启服务」已执行");
                                }
                            },
                            HoldConfirmButton {
                                .text = "销毁",
                                .hold_duration_sec = 1.8f,
                                .danger = true,
                                .shape = HoldConfirmShape::CircularRing,
                                .diameter = 74.0f,
                                .on_confirmed = [log_text, count]() {
                                    ++(*count);
                                    log_text->set("操作日志: [#" + std::to_string(*count) + "] 圆形长按确认「销毁实例」已执行");
                                }
                            }
                        }
                    },
                    app.make_accent(*log_text, 12.5f, FontWeight::Medium)
                }
            })
        }
    });
}

} // namespace gallery::pages

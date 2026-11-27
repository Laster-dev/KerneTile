#include "../gallery_pages.hpp"
#include <cmath>
#include <cstdio>

using namespace ktui;

namespace gallery::pages {

namespace {

WidgetPtr page_header(GalleryApp& app, const std::string& title, const std::string& subtitle) {
    return VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 4.0f,
        .content = {
            app.make_page_title(title, 22.0f),
            Text { .text = subtitle, .font_size = 12.5f }.bind(&RenderText::set_color, app.muted_text)
        }
    };
}

} // namespace

// ==================== PasswordBox ====================
WidgetPtr build_password_box_page(GalleryApp& app) {
    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 12.0f,
        .content = {
            page_header(app, "PasswordBox", "WinUI 3 密码输入框，支持 Peek 按住窥视、Hidden 严格隐藏与掩码态安全禁复制。"),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "按住窥视模式 (PasswordRevealMode::Peek)", .font_size = 14.0f },
                    PasswordBox {
                        .placeholder = "输入访问密码...",
                        .header = "访问令牌",
                        .reveal_mode = PasswordRevealMode::Peek,
                        .width = 260.0f,
                        .on_change = [&app](const std::string& v) {
                            app.password_status.set("密码长度: " + std::to_string(v.size()) + " 字符 (按住右侧眼睛可临时预览)");
                        },
                        .on_submit = [&app](const std::string& v) {
                            app.password_status.set("已按 Enter 提交密码 (长度 " + std::to_string(v.size()) + ")");
                        }
                    }.bind_two_way(app.password_val),
                    Text { .font_size = 12.0f }.bind(&RenderText::set_text, app.password_status).bind(&RenderText::set_color, app.muted_text)
                }
            }),
            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    Text { .text = "其他显隐模式对比 (Hidden / Visible)", .font_size = 14.0f },
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            PasswordBox {
                                .password = "Secret123",
                                .header = "严格隐藏 (Hidden)",
                                .reveal_mode = PasswordRevealMode::Hidden,
                                .width = 210.0f
                            },
                            PasswordBox {
                                .password = "VisibleKey",
                                .header = "明文可见 (Visible)",
                                .reveal_mode = PasswordRevealMode::Visible,
                                .width = 210.0f
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

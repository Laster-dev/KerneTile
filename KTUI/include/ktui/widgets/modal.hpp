#pragma once

#include <ktui/pal/platform_types.hpp>
#include <ktui/app/window.hpp>
#include <ktui/presentation/paint_context.hpp>
#include <ktui/presentation/text_measure.hpp>
#include <ktui/tree/render_box.hpp>
#include <ktui/widgets/widget.hpp>
#include <ktui/widgets/box.hpp>
#include <ktui/widgets/vstack.hpp>
#include <ktui/widgets/hstack.hpp>
#include <ktui/widgets/text.hpp>
#include <ktui/widgets/button.hpp>
#include <string>
#include <memory>
#include <functional>

namespace ktui {

// 模态弹窗参数描述符 (ModalDesc)
struct ModalDesc {
    std::string title{"提示"};
    std::string message{""};
    std::string confirm_text{"确定 OK"};
    std::string cancel_text{"取消 Cancel"};
    float width{340.0f};
    std::function<void()> on_confirm{nullptr};
    std::function<void()> on_cancel{nullptr};
};

// 弹出模态对话框函数 (全屏半透明遮罩 Barrier + 居中对话框)
inline uint64_t show_modal(Window* window, ModalDesc desc) {
    if (!window) return 0;

    std::shared_ptr<uint64_t> modal_id = std::make_shared<uint64_t>(0);

    auto close_fn = [window, modal_id]() {
        if (*modal_id != 0) {
            window->hide_overlay(*modal_id);
            *modal_id = 0;
        }
    };

    WidgetPtr modal_widget = Box {
        .width = desc.width,
        .background_color = Color{0.14f, 0.14f, 0.16f, 1.0f},
        .border_color = Color{0.28f, 0.28f, 0.32f, 1.0f},
        .border_width = 1.0f,
        .corner_radius = 4.0f,
        .padding = EdgeInsets(12.0f, 16.0f),
        .child = VStack {
            .justify = JustifyContent::FlexStart,
            .align_items = AlignItems::Stretch,
            .spacing = 12.0f,
            .padding = EdgeInsets::zero(),
            .content = {
                // 1. 标题行
                HStack {
                    .justify = JustifyContent::SpaceBetween,
                    .align_items = AlignItems::Center,
                    .spacing = 8.0f,
                    .padding = EdgeInsets::zero(),
                    .content = {
                        Text {
                            .text = desc.title,
                            .font_size = 14.5f,
                            .color = Color{0.95f, 0.95f, 0.98f, 1.0f}
                        },
                        Button {
                            .text = "X",
                            .height = 20.0f,
                            .corner_radius = 2.0f,
                            .padding = EdgeInsets(2.0f, 6.0f),
                            .background_color = Color{0.20f, 0.20f, 0.22f, 1.0f},
                            .hover_color = Color{0.75f, 0.22f, 0.22f, 1.0f},
                            .text_color = Color{0.80f, 0.80f, 0.85f, 1.0f},
                            .border_width = 0.0f,
                            .font_size = 11.0f,
                            .on_click = [close_fn, on_can = desc.on_cancel]() {
                                close_fn();
                                if (on_can) on_can();
                            }
                        }
                    }
                },
                // 2. 消息内容
                Text {
                    .text = desc.message,
                    .font_size = 13.0f,
                    .color = Color{0.82f, 0.82f, 0.86f, 1.0f}
                },
                // 3. 底部操作按钮
                HStack {
                    .justify = JustifyContent::FlexEnd,
                    .align_items = AlignItems::Center,
                    .spacing = 8.0f,
                    .padding = EdgeInsets::zero(),
                    .content = {
                        Button {
                            .text = desc.cancel_text,
                            .height = 26.0f,
                            .corner_radius = 3.0f,
                            .padding = EdgeInsets(4.0f, 12.0f),
                            .background_color = Color{0.22f, 0.22f, 0.25f, 1.0f},
                            .hover_color = Color{0.28f, 0.28f, 0.32f, 1.0f},
                            .text_color = Color{0.85f, 0.85f, 0.90f, 1.0f},
                            .border_color = Color{0.32f, 0.32f, 0.36f, 1.0f},
                            .border_width = 1.0f,
                            .font_size = 12.5f,
                            .on_click = [close_fn, on_can = desc.on_cancel]() {
                                close_fn();
                                if (on_can) on_can();
                            }
                        },
                        Button {
                            .text = desc.confirm_text,
                            .height = 26.0f,
                            .corner_radius = 3.0f,
                            .padding = EdgeInsets(4.0f, 14.0f),
                            .background_color = Color{0.15f, 0.44f, 0.80f, 1.0f},
                            .hover_color = Color{0.20f, 0.52f, 0.90f, 1.0f},
                            .text_color = Color::white(),
                            .border_color = Color{0.28f, 0.58f, 0.95f, 1.0f},
                            .border_width = 1.0f,
                            .font_size = 12.5f,
                            .on_click = [close_fn, on_conf = desc.on_confirm]() {
                                close_fn();
                                if (on_conf) on_conf();
                            }
                        }
                    }
                }
            }
        }
    };

    auto root_node = modal_widget->create_render_node();
    // 预排版计算对话框高度
    root_node->layout(BoxConstraints::loose(Vec2{desc.width, 1000.0f}));
    float modal_h = root_node->size().y;

    Vec2 win_sz = window->size();
    Vec2 pos = Vec2{
        std::max(0.0f, (win_sz.x - desc.width) * 0.5f),
        std::max(0.0f, (win_sz.y - modal_h) * 0.42f)
    };

    OverlayEntry entry;
    entry.content = root_node;
    entry.global_pos = pos;
    entry.size = Vec2{desc.width, modal_h};
    entry.dismiss_on_outside_click = false; // 模态弹窗点击遮罩不自动关闭，需点确定或取消
    entry.is_modal = true;
    entry.barrier_color = Color{0.0f, 0.0f, 0.0f, 0.55f};

    *modal_id = window->show_overlay(std::move(entry));
    return *modal_id;
}

} // namespace ktui

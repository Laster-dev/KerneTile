#include <ktui/ktui.hpp>
#include <clocale>
#include <string>

using namespace ktui;

int main() {
    std::setlocale(LC_ALL, "");

    Application app(create_platform_backend(false));

    WindowDesc desc;
    desc.title = "KerneTile Studio";
    desc.size = Vec2{480.0f, 300.0f};
    desc.resizable = false;

    auto window = app.create_window(desc);
    if (!window) return 1;

    int count = 0;
    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> locked{false};
    Property<std::string> input_text{"KerneTile KTUI"};
    Property<std::string> input_preview{"输入预览: KerneTile KTUI"};

    auto refresh_count = [&]() {
        int step = step_ten.get() ? 10 : 1;
        count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" +
                       (locked.get() ? "  [已锁定]" : ""));
    };

    auto card = [&](WidgetPtr child) {
        return Box {
            .background_color = Color{0.15f, 0.15f, 0.18f, 1.0f},
            .border_color = Color{0.22f, 0.22f, 0.26f, 1.0f},
            .border_width = 1.0f,
            .corner_radius = 5.0f,
            .padding = EdgeInsets(12.0f, 14.0f),
            .child = std::move(child)
        };
    };

    window->set_content(Box {
        .background_color = Color{0.11f, 0.11f, 0.13f, 1.0f},
        .padding = EdgeInsets(16.0f, 18.0f),
        .child = VStack {
            .align_items = AlignItems::Stretch,
            .spacing = 12.0f,
            .content = {
                card(VStack {
                    .spacing = 10.0f,
                    .content = {
                        Text { .text = Bind(count_text), .font_size = 14.0f },
                        HStack {
                            .align_items = AlignItems::Center,
                            .spacing = 8.0f,
                            .content = {
                                Button {
                                    .text = "+ 增加",
                                    .variant = ButtonVariant::Accent,
                                    .on_click = [&]() { count += step_ten.get() ? 10 : 1; refresh_count(); },
                                    .disabled = Bind(locked)
                                },
                                Button {
                                    .text = "- 减少",
                                    .on_click = [&]() { count -= step_ten.get() ? 10 : 1; refresh_count(); },
                                    .disabled = Bind(locked)
                                },
                                Button {
                                    .text = "重置",
                                    .variant = ButtonVariant::Danger,
                                    .on_click = [&]() { count = 0; refresh_count(); },
                                    .disabled = Bind(locked)
                                }
                            }
                        }
                    }
                }),
                card(HStack {
                    .align_items = AlignItems::Center,
                    .spacing = 20.0f,
                    .content = {
                        Checkbox { .checked = Bind(step_ten), .label = "步长 10 (+10)", .on_change = [&](bool) { refresh_count(); } },
                        Checkbox { .checked = Bind(locked), .label = "锁定按钮", .on_change = [&](bool) { refresh_count(); } }
                    }
                }),
                card(VStack {
                    .align_items = AlignItems::Stretch,
                    .spacing = 8.0f,
                    .content = {
                        Text { .text = "单行文本输入 (支持撤销/重做、双向绑定):", .color = Color{0.78f, 0.80f, 0.84f, 1.0f} },
                        TextInput {
                            .text = Bind(input_text),
                            .placeholder = "键入中文或英文...",
                            .on_change = [&](const std::string& val) {
                                input_preview.set(val.empty() ? "输入预览: (空)" : "输入预览: " + val);
                            }
                        },
                        Text { .text = Bind(input_preview), .color = Color{0.38f, 0.78f, 0.98f, 1.0f} }
                    }
                })
            }
        }
    });

    window->show();
    return app.run();
}

#include <ktui/ktui.hpp>
#include <clocale>
#include <memory>
#include <string>

using namespace ktui;

int main() {
    std::setlocale(LC_ALL, "");

    Application app(create_platform_backend(false));

    WindowDesc desc;
    desc.title = "KerneTile Studio";
    desc.size = Vec2{520.0f, 380.0f};
    desc.min_size = Vec2{380.0f, 320.0f};
    desc.resizable = true;

    auto window = app.create_window(desc);
    if (!window) return 1;

    int count = 0;
    int child_window_seq = 1;
    std::vector<std::shared_ptr<Property<std::string>>> child_notes;
    Property<std::string> count_text{"当前计数: 0   (步长: 1)"};
    Property<bool> step_ten{false};
    Property<bool> locked{false};
    Property<std::string> input_text{"KerneTile KTUI"};
    Property<std::string> input_preview{"输入预览: KerneTile KTUI"};
    Property<Color> window_bg{Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f}};
    Property<Color> card_bg{Color{0x27 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f}};
    Property<Color> card_border{Color{1.0f, 1.0f, 1.0f, 0.06f}};
    Property<Color> muted_text{Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f}};

    app.theme.subscribe([&](const Theme& t) {
        if (t.is_dark) {
            window_bg.set(Color{0x20 / 255.0f, 0x20 / 255.0f, 0x20 / 255.0f, 1.0f});
            card_bg.set(Color{0x27 / 255.0f, 0x28 / 255.0f, 0x2C / 255.0f, 1.0f});
            card_border.set(Color{1.0f, 1.0f, 1.0f, 0.06f});
            muted_text.set(Color{0xC5 / 255.0f, 0xC5 / 255.0f, 0xC5 / 255.0f, 1.0f});
        } else {
            window_bg.set(Color{0xEA / 255.0f, 0xED / 255.0f, 0xF1 / 255.0f, 1.0f});
            card_bg.set(Color{0xFA / 255.0f, 0xFB / 255.0f, 0xFD / 255.0f, 1.0f});
            card_border.set(Color{0.0f, 0.0f, 0.0f, 0.07f});
            muted_text.set(Color{0x59 / 255.0f, 0x60 / 255.0f, 0x6A / 255.0f, 1.0f});
        }
    });

    auto refresh_count = [&]() {
        int step = step_ten.get() ? 10 : 1;
        count_text.set("当前计数: " + std::to_string(count) + "   (步长: " + std::to_string(step) + ")" +
                       (locked.get() ? "  [已锁定]" : ""));
    };

    auto card = [&](WidgetPtr child) {
        return Box {
            .background_color = Bind(card_bg),
            .border_color = Bind(card_border),
            .border_width = 1.0f,
            .corner_radius = 5.0f,
            .padding = EdgeInsets(12.0f, 14.0f),
            .child = std::move(child)
        };
    };

    auto spawn_child_window = [&]() {
        int idx = child_window_seq++;
        WindowDesc child_desc;
        child_desc.title = "工具窗口 #" + std::to_string(idx);
        child_desc.size = Vec2{360.0f, 240.0f};
        child_desc.min_size = Vec2{260.0f, 180.0f};
        child_desc.resizable = true;

        auto child_win = app.create_window(child_desc);
        if (!child_win) return;

        auto note = std::make_shared<Property<std::string>>("窗口 #" + std::to_string(idx) + " 独立输入");
        child_notes.push_back(note);
        child_win->set_content(Box {
            .background_color = Bind(window_bg),
            .padding = EdgeInsets(14.0f, 16.0f),
            .child = VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 10.0f,
                .content = {
                    card(VStack {
                        .align_items = AlignItems::Stretch,
                        .spacing = 8.0f,
                        .content = {
                            Text { .text = "独立无边框子窗口 (共享全局计数绑定)", .color = Bind(muted_text) },
                            Text { .text = Bind(count_text), .font_size = 14.0f },
                            HStack {
                                .align_items = AlignItems::Center,
                                .spacing = 8.0f,
                                .content = {
                                    Button {
                                        .text = "+1",
                                        .variant = ButtonVariant::Accent,
                                        .on_click = [&]() { count += 1; refresh_count(); }
                                    },
                                    Button {
                                        .text = "-1",
                                        .on_click = [&]() { count -= 1; refresh_count(); }
                                    }
                                }
                            }
                        }
                    }),
                    TextInput {
                        .text = Bind(*note),
                        .placeholder = "在此窗口独立输入..."
                    }
                }
            }
        });
        child_win->show();
    };

    // 在标题栏右侧添加自定义操作按钮 (位于置顶 Pin 按钮左侧，对齐 CUI.Core)
    window->titlebar().add_action("+", "新建工具窗口", spawn_child_window);

    window->set_content(Box {
        .background_color = Bind(window_bg),
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
                                },
                                Button {
                                    .text = "+ 新建窗口",
                                    .on_click = spawn_child_window
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
                        Text { .text = "单行文本输入 (支持撤销/重做、双向绑定):", .color = Bind(muted_text) },
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

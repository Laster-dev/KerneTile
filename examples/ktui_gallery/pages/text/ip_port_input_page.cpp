#include "../gallery_pages.hpp"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace ktui;

namespace gallery::pages {

namespace {

// 轻量辅助：捕获任意子控件创建后的 RenderBox 弱引用，供 SpotlightTour 实时定位真实目标控件
class RefCaptureWidget : public Widget {
public:
    RefCaptureWidget(WidgetPtr inner, std::shared_ptr<std::weak_ptr<RenderBox>> slot)
        : m_inner(std::move(inner)), m_slot(std::move(slot)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = m_inner ? m_inner->create_render_node() : nullptr;
        if (m_inner && node) m_inner->set_render_node(node);
        if (m_slot) *m_slot = node;
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (m_slot) *m_slot = node;
        if (m_inner) m_inner->update_render_node(std::move(node));
    }

private:
    WidgetPtr m_inner;
    std::shared_ptr<std::weak_ptr<RenderBox>> m_slot;
};

WidgetPtr with_ref(WidgetPtr child, std::shared_ptr<std::weak_ptr<RenderBox>> slot) {
    return make_widget<RefCaptureWidget>(std::move(child), std::move(slot));
}

} // namespace

// ============================================================================
// 6. IP 端口输入控件 (IpPortInput)
// ============================================================================
WidgetPtr build_ip_port_input_page(GalleryApp& app) {
    auto ep_text = std::make_shared<Property<std::string>>("当前服务地址: 192.168.1.1:8080  (LAN)");
    auto ref_slot = std::make_shared<std::weak_ptr<RenderBox>>();

    return app.make_scroll_page(VStack {
        .align_items = AlignItems::Stretch,
        .spacing = 14.0f,
        .content = {
            app.make_page_title("IpPortInput (IPv4 + 端口分段输入控件)", 24.0f),
            app.make_muted("原生支持 4 段 IPv4（0~255）+ 端口（0~65535）分段输入：输满 3 位或按 '.' 自动跳下一段，按 ':' 直达端口，Backspace 自动回跳，上下键微调并实时识别网段类型。", 13.0f),

            app.make_card(VStack {
                .align_items = AlignItems::Stretch,
                .spacing = 12.0f,
                .content = {
                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            Text { .text = "服务端点 (IP:Port):", .font_size = 13.0f, .font_weight = FontWeight::Medium },
                            with_ref(IpPortInput {
                                .ip = "192.168.1.1",
                                .port = 8080,
                                .show_port = true,
                                .on_changed = [ep_text, ref_slot](const std::string& ip, uint16_t port) {
                                    std::string cat = "LAN";
                                    if (auto b = ref_slot->lock()) cat = static_cast<RenderIpPortInput*>(b.get())->address_category();
                                    ep_text->set("当前服务地址: " + ip + ":" + std::to_string(port) + "  (" + cat + ")");
                                }
                            }, ref_slot)
                        }
                    },

                    HStack {
                        .align_items = AlignItems::Center,
                        .spacing = 14.0f,
                        .content = {
                            Text { .text = "纯 IPv4 网关地址:", .font_size = 13.0f, .font_weight = FontWeight::Medium },
                            IpPortInput { .ip = "10.0.0.1", .port = 0, .show_port = false }
                        }
                    },

                    app.make_accent(*ep_text, 12.5f, FontWeight::Medium),

                    HStack {
                        .spacing = 8.0f,
                        .content = {
                            Button {
                                .text = "本地调试 127.0.0.1:3000",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderIpPortInput*>(b.get())->set_address("127.0.0.1", 3000, true);
                                }
                            },
                            Button {
                                .text = "内网服务 192.168.1.100:443",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderIpPortInput*>(b.get())->set_address("192.168.1.100", 443, true);
                                }
                            },
                            Button {
                                .text = "公网 DNS 8.8.8.8:53",
                                .on_click = [ref_slot]() {
                                    if (auto b = ref_slot->lock()) static_cast<RenderIpPortInput*>(b.get())->set_address("8.8.8.8", 53, true);
                                }
                            }
                        }
                    }
                }
            })
        }
    });
}

} // namespace gallery::pages

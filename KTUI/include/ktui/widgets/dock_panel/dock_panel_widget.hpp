#pragma once

namespace ktui {

struct DockPanel : BindableMixin<DockPanel, RenderDockPanel> {
    std::vector<DockPane> panes{};
    float left_width{200.0f};
    float right_width{200.0f};
    float top_height{110.0f};
    float bottom_height{120.0f};
    EdgeInsets padding{EdgeInsets::zero()};
    bool allow_detach_window{true};
    Fn<void(const std::string&, DockZone)> on_dock_changed{nullptr};
    Fn<void(const std::string&, bool)> on_detach_changed{nullptr};
    RenderDockPanel::DetachCallback on_detach_pane{nullptr};

    operator WidgetPtr() const&;
    operator WidgetPtr() &&;
};

class DockPanelWidget : public Widget {
public:
    explicit DockPanelWidget(const DockPanel& desc) : m_desc(desc) {}
    explicit DockPanelWidget(DockPanel&& desc) noexcept : m_desc(std::move(desc)) {}

    std::shared_ptr<RenderBox> create_render_node() override {
        auto node = make_render_node<RenderDockPanel>(
            m_desc.left_width, m_desc.right_width, m_desc.top_height, m_desc.bottom_height,
            m_desc.padding, m_desc.allow_detach_window,
            m_desc.on_dock_changed, m_desc.on_detach_changed, m_desc.on_detach_pane);
        std::vector<RenderDockPanel::LivePane> live;
        live.reserve(m_desc.panes.size());
        for (auto& p : m_desc.panes) {
            std::shared_ptr<RenderBox> box = p.content ? p.content->create_render_node() : nullptr;
            if (p.content && box) p.content->set_render_node(box);
            live.push_back(RenderDockPanel::LivePane{
                p.id, p.title, p.zone, p.zone, p.closable, false, false, std::move(box), {}, {}
            });
        }
        node->set_panes(std::move(live));
        return node;
    }

    void update_render_node(std::shared_ptr<RenderBox> node) override {
        if (auto dp = std::static_pointer_cast<RenderDockPanel>(node)) {
            dp->set_padding(m_desc.padding);
            dp->set_allow_detach_window(m_desc.allow_detach_window);
            dp->set_on_dock_changed(m_desc.on_dock_changed);
            dp->set_on_detach_changed(m_desc.on_detach_changed);
            dp->set_on_detach_pane(m_desc.on_detach_pane);
        }
    }

private:
    DockPanel m_desc;
};

inline DockPanel::operator WidgetPtr() const& { return make_widget<DockPanelWidget>(*this); }
inline DockPanel::operator WidgetPtr() && { return make_widget<DockPanelWidget>(std::move(*this)); }

} // namespace ktui
